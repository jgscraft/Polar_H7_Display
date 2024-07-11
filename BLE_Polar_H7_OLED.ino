// https://forum.arduino.cc/t/connect-nano-sense-to-polar-bluetooth-heart-rate-monitor/1169822/22
//
// BLE_Polar_H7_OLED
// 
// Hardware:  UNO R4 WiFi 
//            TinyS3 worked for initial test and then later wouldn't even run blink  
//            Couldn't get QT Py ESP32-S3 to load on 1.8 nor 2.3
//            OLED 128x64 SH1106 or SSD1309 - ended up using 2.4" HiLetgo SSD1309
//            Polar H7 Chest strap
//
//            Adafruit ItsyBitsy ESP32  - verified 240703
//            Adafruit Feather ESP32 V2 - verified 240608
//            HiLetgo ESP32 Dev Module  - verified 240703
//            Arduino UNO WiFi R4       - verified 240703
//            
//
// 240509 - (JGS) Bluetooth code is copied from link above, scroll down for last code example
//        - BLE doesn't always start with UNO R4 WiFi reset, needs full power off reset
//        - use a BLE scanner to get device name, must match exactly
//        - added OLED Display
// 
// 240509 - Working with HiLetgo "ESP32 Dev Module" This is ESP32-WROOM 
//          !!!! NON-STANDARD PINOUT !!!!
//
// 240511 - Added 2.42" OLED display - works great!
//          SH1106 driver works with 1.2" SH1106 and 2.4" SSD1309 128x64 pixel displays
//          SSD1306 driver works for SSD1306 and SSD1309 chips - be sure to set correct height (128x32)
//
// 240608 - Additional screen info on startup
//        - Screen message if connection lost
//        - "Production" version uses Adafruit Feather ESP32 V2 with LiPo battery support
//        - EN pin on Feather brought to low results in 0.06mA current draw so use this as on/off switch
//        - show voltage if certain board (Feather ESP32 V2)
//
// 240704 - using ItsyBitsy ESP32 along with LiPo Amigo Pro, use voltage divider to measure battery voltage on A2
//

// Select one of these...
#define OLED_SH1106               // 128x64
//#define OLED_SSD1306              // 128x32

#include <ArduinoBLE.h>

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>             // Adafruit graphics library
#include <U8g2_for_Adafruit_GFX.h>    // use U8g2 font library, select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

#ifdef OLED_SH1106
  #include <Adafruit_SH110X.h>
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  #define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
  //#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
  #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
  Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

#ifdef OLED_SSD1306
  #include <Adafruit_SSD1306.h>
  #define SCREEN_WIDTH    128     // OLED display width, in pixels
  #define SCREEN_HEIGHT   32      // OLED display height, in pixels
  #define SCREEN_ADDRESS  0x3C 
  #define OLED_RESET      -1      // Reset pin # (or -1 if sharing Arduino reset pin)
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;          // all text output goes through this

const char* deviceName = "Polar H7 8C505015";       // <-- from BLE scan, also matches ID: on Polar case
//const char* deviceName = "Polar H7 C248F711";       
const char* serviceUUID = "180D";
const char* charUUID = "2A37";

BLEDevice peripheral;
BLEService heartRateService;
BLECharacteristic heartRateMeasurementCharacteristics;
bool isSubscribed = false; // new flag

void setup() {
  Serial.begin(115200);
  delay(1000);
  //while (!Serial); // Comment out after development, otherwise won't run on battery only
  Serial.print(__DATE__);  Serial.print("  "); Serial.println( __FILE__);


#ifdef OLED_SH1106
  delay(250); // wait for the OLED to power up
  display.begin(i2c_Address, true); // Address 0x3C default
#endif

#ifdef OLED_SSD1306
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
#endif

 u8g2_for_adafruit_gfx.begin(display);                 // associate text with a display
 display.clearDisplay();                               // clear the graphcis buffer  
//  u8g2_for_adafruit_gfx.setFont(u8g2_font_8x13_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  u8g2_for_adafruit_gfx.setFont(u8g2_font_6x10_tf);     // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  u8g2_for_adafruit_gfx.setFontMode(1);                 // use u8g2 transparent mode (this is default)
  u8g2_for_adafruit_gfx.setFontDirection(0);            // left to right (this is default)
  u8g2_for_adafruit_gfx.setForegroundColor( 1 );        // apply Adafruit GFX color
  u8g2_for_adafruit_gfx.setCursor(0,8);                 // start writing at this position
  u8g2_for_adafruit_gfx.print("Polar H7 Display");  

#ifdef ARDUINO_ADAFRUIT_FEATHER_ESP32_V2                // this board has builtin has voltage detector
  #define VBATPIN A13
  float measuredvbat = analogReadMilliVolts(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat /= 1000; // convert to volts!
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  u8g2_for_adafruit_gfx.setCursor(0,62);     
  u8g2_for_adafruit_gfx.print("Battery (V):");  
  u8g2_for_adafruit_gfx.setCursor(90,62);     
  u8g2_for_adafruit_gfx.print(measuredvbat);            // display LiPo battery voltage
#endif

#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_ESP32                // this board has builtin has voltage detector
  #define VBATPIN A2
  float measuredvbat = analogReadMilliVolts(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat /= 1000; // convert to volts!
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  u8g2_for_adafruit_gfx.setCursor(0,62);     
  u8g2_for_adafruit_gfx.print("Battery (V):");  
  u8g2_for_adafruit_gfx.setCursor(90,62);     
  u8g2_for_adafruit_gfx.print(measuredvbat);            // display LiPo battery voltage
#endif

  display.display();                          
  u8g2_for_adafruit_gfx.setCursor(0,23);                // setup for next line

  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    u8g2_for_adafruit_gfx.print("BLE Failed");  
    display.display();                          
    while (1);
  }
  Serial.println("Scanning for peripherals...");
  u8g2_for_adafruit_gfx.print("Scanning for");  
  u8g2_for_adafruit_gfx.setCursor(0,34);                // setup for next line
  u8g2_for_adafruit_gfx.print(deviceName);  
  display.display();                                 
}

void loop() {
  if (!isSubscribed) {
    BLE.scan();
    peripheral = BLE.available();
    
    if (peripheral) {
      if (strcmp(peripheral.localName().c_str(), deviceName) == 0) {
        BLE.stopScan();
        if (peripheral.connect()) {
          if (peripheral.discoverAttributes()) {
            heartRateService = peripheral.service(serviceUUID);
            if (heartRateService) {
              heartRateMeasurementCharacteristics = heartRateService.characteristic(charUUID);
              if (heartRateMeasurementCharacteristics) {
                if (heartRateMeasurementCharacteristics.subscribe()) {
                  isSubscribed = true; // set flag to true
                  Serial.println("Subscribed to heart rate measurements.");
                  u8g2_for_adafruit_gfx.setCursor(0,49);
                  u8g2_for_adafruit_gfx.print("Subscribed...");  
                  display.display();                                 

                }
              }
            }
          }
        }
      }
    }
  } else {
    // If subscribed, just try to read the values
    if (heartRateMeasurementCharacteristics.valueUpdated()) {
      byte buffer[8];
      int len = heartRateMeasurementCharacteristics.valueLength();
      heartRateMeasurementCharacteristics.readValue(buffer, len);
      byte flags = buffer[0];
      int bpm = (bitRead(flags, 0) == 0) ? buffer[1] : (buffer[2] << 8) | buffer[1];
      Serial.print("Heart rate: ");
      Serial.print(bpm);
      Serial.println(" bpm");

      display.clearDisplay();  
      u8g2_for_adafruit_gfx.setFont( u8g2_font_logisoso58_tf );   // really big font     
      if( bpm < 100) {
        u8g2_for_adafruit_gfx.setCursor(25,63);                   // keep 2- and 3-digit numbers centered
      } else {
        u8g2_for_adafruit_gfx.setCursor(10,63);
      }
      u8g2_for_adafruit_gfx.print(bpm);  
      display.display();                                 
      
    } else {
      Serial.println("No new values.");
      // check if we are still subscribed...
      if (!heartRateMeasurementCharacteristics.subscribe()) {
        Serial.println("Lost subscription.");

        display.clearDisplay();                               // clear the graphcis buffer  
        u8g2_for_adafruit_gfx.setFont(u8g2_font_7x13_tf);     // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
        u8g2_for_adafruit_gfx.setCursor(0,20);                // start writing at this position
        u8g2_for_adafruit_gfx.print("Lost connection.");  
        display.display();                          

      }
    }
  }
  delay(500); // delay to slow down the loop
}
