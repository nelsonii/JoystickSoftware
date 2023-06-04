//7MM_Joystick_Palm_QTPy_SAMD21_v4A
// 2023-05-24-A
const long buildDate = 2023052401;

//=================================================================================

// IMPORTANT: For the V3 of the Palm Style device I am using the QT PY (SAMD21) board.
//            You MUST use the TinyUSB package instead of the usual "joystick.h".
//                TOOLS >> USB STACK >> TinyUSB must be selected (not "arduino").

// Board: Adafruit QT PY (SAMD21)
// Optimize: Faster (-O3)
// USB Stack: TinyUSB  <-- Important: Using TinyUSB, not Arduino USB!
// Debug: Off
// Port: COMxx (Adafruit QT PY (SAMD21))

// This sketch is only valid on boards which have native USB support
// and compatibility with Adafruit TinyUSB library. 
// For example SAMD21, SAMD51, nRF52840.

// 2022-11-03-A : Added missing button code
// 2023-05-20-A : Adjusted range based on newest readings
// 2023-05-20-A : Code improvements / streamlining
// 2023-05-24-A : Range and centering improvements

#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>

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

// Joystick pins
#define BUTTON_PIN  A1 // Pushbutton of the thumbstick
#define VERT_PIN    A2 // "Y" -- dependent on how thumbstick is oriented
#define HORZ_PIN    A3 // "X" -- dependent on how thumbstick is oriented

// Mapping to XBOX Adaptive Controller
// X and Y will auto map to Axis 0/1 or 2/3 depending on which side you plug
// the joystick into (left=0/1 or right=2/3). 

// Joystick variables - Button (with debounce)
bool btnStick = false;
int SEL;
int prevSEL = HIGH;
long lastDebounceTime = 0;
const int debounceDelay = 50;  //millis

//=================================================================================

// invert if needed
const bool invHorz = false; // 7MM: palmstick=false, slidestick=true
const bool invVert = false; // 7MM: palmstick=false, slidestick=true

//=================================================================================

int rawHorz, rawVert; // raw values from sensor
int lmtHorz, lmtVert; // limited (range checked) value
int mapHorz, mapVert; // mapped value (limited)

// set the default min/max values.

// Determined during testing. These are for geneic PS2 style joysticks.
// Adjust values based on what you actually get from the stick being used.

// Test with values from 6-12
int deadzone=2; // most sticks are 7, some are worse (at 12)

// Small toppers 200/800
// Large toppers 300/700
// Jumbo toppers H=127/524/775 V=220/528/790

int minHorz=127;
int centeredHorz=524;
int maxHorz=775;

int minVert=220;
int centeredVert=528;
int maxVert=790;

// The XBOX Adapative Controller (XAC) likes joystick values from -127 to + 127
// adjust these min/max based on your upstream device. Same values for both X and Y.
const int joyMin = -127;
const int joyMax = +127;

//=================================================================================


void setup() {

  if (debug) { Serial.begin(115200); }

  // Set HORZ and VERT and BUTT pins (from joystick) to input
  pinMode(VERT_PIN, INPUT);
  pinMode(HORZ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Startup the NeoPixel
  pixels.begin(); 

  // Startup TinyUSB for Joystick
  pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();
  #if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
    // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    TinyUSB_Device_Init(0);
  #endif
  // IMPORTANT: This will loop until device is mounted.
  //            So, if you see a RED light, it's powered, but not mounted.
  //            When the LED goes yellow, then green, it's mounted.
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(10);

  //Sampling centerpoint
  pixels.clear(); pixels.setPixelColor(0,yellowHigh); pixels.show();
  
  // On startup, take some readings from the (hopefully) 
  // centered joystick to determine actual center for X and Y. 
  int iNumberOfSamples = 50;
  long lSampleSumHorz = 0;
  long lSampleSumVert = 0;
  for (int iSample = 1; iSample<=iNumberOfSamples; iSample++) {
    lSampleSumHorz += analogRead(HORZ_PIN); delay(10);
    lSampleSumVert += analogRead(VERT_PIN); delay(10);
  }
  centeredHorz=int(lSampleSumHorz/iNumberOfSamples);
  centeredVert=int(lSampleSumVert/iNumberOfSamples);

  //TODO: Calculate deadzone dynamically
 
  // All ready.
  pixels.clear(); pixels.setPixelColor(0,greenLow); pixels.show();

}//setup


//=================================================================================


void loop() {

  // ----------------------------------------------------

  // Check for button push - and debounce
  int reading = digitalRead(BUTTON_PIN);
  if (reading != prevSEL) {
     lastDebounceTime = millis();// reset timer
  }
  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != SEL) {
      SEL = reading;
      if (SEL == LOW) { btnStick = true; } else { btnStick = false; }
    }
  }
  prevSEL = reading;
  // ----------------------------------------------------

  // Read analog values from input pins.
  rawHorz = analogRead(HORZ_PIN);
  rawVert = analogRead(VERT_PIN);

  lmtHorz = constrain(rawHorz, minHorz, maxHorz);
  lmtVert = constrain(rawVert, minVert, maxVert);
   
  // Map values to a range the XAC likes.
  // Also take into account range differences on +/- of each axis (ugh!)
  if (lmtHorz<centeredHorz) { mapHorz = map(lmtHorz, minHorz, centeredHorz, joyMin, 0);}
  else { mapHorz = map(lmtHorz, centeredHorz, maxHorz, 0, joyMax); }
  if (lmtVert<centeredVert) { mapVert = map(lmtVert, minVert, centeredVert, joyMin, 0);} 
  else { mapVert = map(lmtVert, centeredVert, maxVert, 0, joyMax); }

  //Handle center deadzone / sloppiness around center
  if (abs(mapHorz) <= deadzone) { mapHorz = 0; } 
  if (abs(mapVert) <= deadzone) { mapVert = 0; } 

  // Invert value if requested (if "up" should go "down" or "left" to "right")
  if (invHorz) {mapHorz = -mapHorz;}
  if (invVert) {mapVert = -mapVert;}

  if ( usb_hid.ready() ) {
      joystick.x = mapHorz;
      joystick.y = mapVert;
      if (btnStick) { joystick.buttons = GAMEPAD_BUTTON_2; } else joystick.buttons = 0;
      usb_hid.sendReport(0, &joystick, sizeof(joystick));
    }

  // ----------------------------------------------------

  if (debug) {serialDebug();}

}//loop


//=================================================================================


void serialDebug() {
  
  Serial.print(" RH:");
  Serial.print(rawHorz);
  Serial.print(" RV:");
  Serial.print(rawVert);
  Serial.print(" LH:");
  Serial.print(lmtHorz);
  Serial.print(" LV:");
  Serial.print(lmtVert);
  Serial.print(" MH: ");
  Serial.print(mapHorz);
  if (invHorz) { Serial.print("i"); } // i indicates value was inverted 
  Serial.print(" MV: ");
  Serial.print(mapVert);
  if (invVert) { Serial.print("i"); } // i indicates value was inverted
  Serial.print(" BTN: ");
  Serial.print(digitalRead(BUTTON_PIN));
  Serial.println();

}//serialDebug
