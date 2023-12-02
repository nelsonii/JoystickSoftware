//7MM_Joystick_Touch_QTPy_SAMD21_v4A
// 2023-06-09-B
const long buildDate = 2023060902;

//=================================================================================

// Board: Adafruit QT PY (SAMD21)
// Optimize: Faster (-O3)
// USB Stack: TinyUSB  <-- Important: Using TinyUSB, not Arduino USB!
// Debug: Off
// Port: COMxx (Adafruit QT PY (SAMD21))

// This sketch is only valid on boards which have native USB support
// and compatibility with Adafruit TinyUSB library. 
// For example SAMD21, SAMD51, nRF52840.

// 2023-06-08 : Initial version, based on Palm v4A code with latest improvements

#if !defined(USE_TINYUSB)
 #error("Please select TinyUSB from the Tools->USB Stack menu!")
#endif

#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include "Adafruit_TSC2007.h"


//=================================================================================

const boolean debug = true; // if true, output info to serial monitor.

//=================================================================================

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN_NEOPIXEL);

uint32_t redLow = pixels.Color(64,0,0);
uint32_t redHigh = pixels.Color(255,0,0);
uint32_t greenLow = pixels.Color(0,64,0);
uint32_t greenHigh = pixels.Color(0,255,0);
uint32_t blueLow = pixels.Color(0,0,64);
uint32_t blueHigh = pixels.Color(0,0,255);
uint32_t pinkLow = pixels.Color(64,13,30);
uint32_t pinkHigh = pixels.Color(255,51,119);
uint32_t yellowLow = pixels.Color(64,43,0);
uint32_t yellowHigh = pixels.Color(255,170,0);
uint32_t whiteLow = pixels.Color(64,64,64);
uint32_t whiteHigh = pixels.Color(255,255,255);
uint32_t black = pixels.Color(0,0,0);

//=================================================================================

//Joystick via TinyUSB

uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_GAMEPAD() };

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);

hid_gamepad_report_t    joystick;

//=================================================================================

Adafruit_TSC2007 touch;
uint16_t touchX, touchY, touchZ1, touchZ2;
uint16_t deadZone = 10;

//=================================================================================

int rawHorz, rawVert; // raw values from sensor
int lmtHorz, lmtVert; // limited (range checked) value
int mapHorz, mapVert; // mapped value (limited)

int minHorz=500;
int centeredHorz=2000;
int maxHorz=3500;

int minVert=500;
int centeredVert=2000;
int maxVert=3500;

// invert if needed
bool invHorz = false; 
bool invVert = false; 

// The XBOX Adapative Controller (XAC) likes joystick values from -127 to + 127
// adjust these min/max based on your upstream device. Same values for both X and Y.
const int joyMin = -127;
const int joyMax = +127;

//=================================================================================

void setup() {

  if (debug) { Serial.begin(115200); }

  // Startup the NeoPixel
  pixels.begin(); 
  pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();

  //Check for bootloader reset/request (allows field reprogramming)
  HandleResetRequest();

  // Startup TinyUSB for Joystick
  #if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
    // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    TinyUSB_Device_Init(0);
  #endif
  // IMPORTANT: This will loop until device is mounted.
  //            So, if you see a RED light, it's powered, but not mounted.
  //            When the LED goes yellow, then green, it's mounted.
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(10);

  // Startup Touch Device
  pixels.clear(); pixels.setPixelColor(0,blueHigh); pixels.show();

  while (!touch.begin()) {
    if (debug) Serial.println("TSC2007 : Could not find touch controller.");
    delay(100);
  }
  if (debug) Serial.println("TSC2007 : Touch Controller Found");

  //Confirm that valid readings are being received (ribbon cable not unplugged/broken)
  touch.read_touch(&touchX, &touchY, &touchZ1, &touchZ2);
  while (touchZ1>touchZ2) {
    //Invalid state -- Z1 should never be greater than Z2 
    pixels.clear(); pixels.setPixelColor(0, whiteLow); pixels.show(); delay(50);
    touch.read_touch(&touchX, &touchY, &touchZ1, &touchZ2); delay(50);
    pixels.clear(); pixels.setPixelColor(0, blueHigh); pixels.show(); delay(100);
  }

  //Find neutral touch / deadzone
  pixels.clear(); pixels.setPixelColor(0,yellowHigh); pixels.show();
  int iNumberOfSamples = 10;
  long lSampleSum = 0;
  for (int iSample = 1; iSample<=iNumberOfSamples; iSample++) {
    touch.read_touch(&touchX, &touchY, &touchZ1, &touchZ2);
    lSampleSum += (long) touchZ1;
  }
  deadZone=round((lSampleSum/iNumberOfSamples)*2.00); // 200% of sampled Z
 
  // All ready.
  pixels.clear(); pixels.setPixelColor(0,greenHigh); pixels.show();

}//setup

//=================================================================================

void loop() {

  if(touch.read_touch(&touchX, &touchY, &touchZ1, &touchZ2)) {
    if (touchZ1>deadZone) { 
      
      rawHorz = touchX;
      rawVert = touchY;
      
      lmtHorz = constrain(rawHorz, minHorz, maxHorz);
      lmtVert = constrain(rawVert, minVert, maxVert);
      
      // Map values to a range the XAC likes.
      // Also take into account range differences on +/- of each axis (ugh!)
      if (lmtHorz<centeredHorz) { mapHorz = map(lmtHorz, minHorz, centeredHorz, joyMin, 0);}
      else { mapHorz = map(lmtHorz, centeredHorz, maxHorz, 0, joyMax); }
      if (lmtVert<centeredVert) { mapVert = map(lmtVert, minVert, centeredVert, joyMin, 0);} 
      else { mapVert = map(lmtVert, centeredVert, maxVert, 0, joyMax); }
     
      // Invert value if requested (if "up" should go "down" or "left" to "right")
      if (invHorz) {mapHorz = -mapHorz;}
      if (invVert) {mapVert = -mapVert;}
    }else{
      mapHorz = 0; mapVert=0;
    }//deadZone
  }else{
    mapHorz = 0; mapVert=0;
  }//touch

  if ( usb_hid.ready() ) {
      joystick.x = mapHorz;
      joystick.y = mapVert;
      usb_hid.sendReport(0, &joystick, sizeof(joystick));
    }

  if (debug) {serialDebug();}

}//loop

//=================================================================================

void HandleResetRequest(){

  // New feature allows user to trigger a boot loader mode, making field updates easier.

  long startTime = millis();

  /*
  while (digitalRead(BUTTON_PIN)==LOW) { 
    pixels.clear(); pixels.setPixelColor(0, pinkHigh); pixels.show(); delay(100);
    if ((millis()-startTime) > 5000) { ResetToBootLoader(); }
    //if ((millis()-startTime) > 5000) { return; }
    pixels.clear(); pixels.setPixelColor(0, whiteLow); pixels.show(); delay(50);
  }
  */
  
}//HandleResetRequest()

//=================================================================================

void ResetToBootLoader(){

  *(uint32_t *)(0x20000000 + 32768 -4) = 0xf01669ef;     // Store special flag value in last word in RAM.
  NVIC_SystemReset();    // Like pushing the reset button.

}//ResetToBootLoader

//=================================================================================


void serialDebug() {

  Serial.print(buildDate);
  Serial.print(" DZ:"); Serial.print(deadZone); 
  Serial.print(" Z1:"); Serial.print(touchZ1); 
  Serial.print(" Z2: "); Serial.print(touchZ2);
  Serial.print(" RH:"); Serial.print(rawHorz);
  Serial.print(" RV:"); Serial.print(rawVert);
  Serial.print(" LH:"); Serial.print(lmtHorz);
  Serial.print(" LV:"); Serial.print(lmtVert);
  Serial.print(" MH: "); Serial.print(mapHorz);
  if (invHorz) { Serial.print("i"); } // i indicates value was inverted 
  Serial.print(" MV: "); Serial.print(mapVert);
  if (invVert) { Serial.print("i"); } // i indicates value was inverted
  //Serial.print(" BTN: "); Serial.print(digitalRead(BUTTON_PIN));  
  Serial.println();

}//serialDebug
