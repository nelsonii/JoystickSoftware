// 7MM_Joystick_Nuncheck_v2A
// 2022-05-28-A
const long buildDate = 2022052801;

//=================================================================================

// IMPORTANT: For the Wii Nunchuck device I am using the QT PY (SAMD21) board.
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

#include <Adafruit_TinyUSB.h>
#include <WiiChuck.h>
#include <Adafruit_NeoPixel.h>

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

//Nunchuck object
Accessory nunchuck;

//Nunchuck buttons 
bool btnZ = false;
bool btnC = false;

// values from analog stick
long rawHorz, rawVert;
long mapHorz, mapVert;

// invert if needed
const bool invHorz = false;
const bool invVert = true;

//=================================================================================

// The XBOX Adapative Controller (XAC) likes joystick values from -127 to + 127
// adjust these min/max based on your upstream device. Same values for both X and Y.
const int joyMin = -127;
const int joyMax = +127;

//=================================================================================

// set the default min/max values.
// Determined during testing. These are for Wii Nunchuck style joysticks.
// Adjust values based on what you actually get from the stick being used.

int minHorz=0;
int centeredHorz=127;
int maxHorz=255;

int minVert=0;
int centeredVert=127;
int maxVert=255;

//hard limits, to catch out-of-range readings
const int limitMinHorz=0;
const int limitMaxHorz=255;
const int limitMinVert=0;
const int limitMaxVert=255;


//=================================================================================


void setup() {

  // Startup the NeoPixel
  pixels.begin(); 
  pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();

  // Startup TinyUSB for Joystick
  #if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
    // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    TinyUSB_Device_Init(0);
  #endif
  // IMPORTANT: This will loop until device is mounted.
  //            So, if you see a RED light, it's powered, but not mounted.
  //            When the LED goes yellow, it's mounted.
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(10);

  // Startup the nunchuck object.
  pixels.clear(); pixels.setPixelColor(0,yellowHigh); pixels.show();
  nunchuck.begin();
  if (nunchuck.type == Unknown) {
    nunchuck.type = NUNCHUCK;
  }

  // All ready.
  pixels.clear(); pixels.setPixelColor(0,greenLow); pixels.show();

}//setup


//=================================================================================


void loop() {

  nunchuck.readData();
  
  rawHorz = nunchuck.getJoyX();
  rawVert = nunchuck.getJoyY();

  if (nunchuck.getButtonZ()) { btnZ=true; } else { btnZ=false; }
  if (nunchuck.getButtonC()) { btnC=true; } else { btnC=false; }
  
  // update the min/max during run, in case we see something outside of the normal
  if (rawHorz<minHorz) {minHorz=rawHorz;}
  if (rawHorz>maxHorz) {maxHorz=rawHorz;}
  if (rawVert<minVert) {minVert=rawVert;}
  if (rawVert>maxVert) {maxVert=rawVert;}

  // Map values to a range the upstread device likes
  mapHorz = map(rawHorz, minHorz, maxHorz, joyMin, joyMax);
  mapVert = map(rawVert, minVert, maxVert, joyMin, joyMax);
 
  // Invert value if requested (if "up" should go "down" or "left" to "right")
  if (invHorz) {mapHorz = -mapHorz;}
  if (invVert) {mapVert = -mapVert;}

  // Send the values to the joystick object
  if ( usb_hid.ready() ) {
      joystick.x = mapHorz;
      joystick.y = mapVert;
      if (btnZ && !btnC) { joystick.buttons = GAMEPAD_BUTTON_4; }
      else if (!btnZ && btnC) { joystick.buttons = GAMEPAD_BUTTON_5; }
      else if (btnZ && btnC) {joystick.buttons = GAMEPAD_BUTTON_4 + GAMEPAD_BUTTON_5; }
      else joystick.buttons = 0;
      usb_hid.sendReport(0, &joystick, sizeof(joystick));
    }

}//loop
