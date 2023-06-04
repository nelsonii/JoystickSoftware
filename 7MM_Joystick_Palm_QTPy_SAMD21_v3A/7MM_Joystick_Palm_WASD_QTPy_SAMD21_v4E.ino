//7MM_Joystick_Palm_WASD_QTPy_SAMD21_v4E
// 2023-06-04-A
const long buildDate = 2023060401;

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
// 2023-05-26-A : Streamline / update keyboard presses
// 2023-06-04-A : Diagonal code fix (clear before send)
// 2023-06-04-A : Software based reboot (for field loading of UF2)

#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>

//=================================================================================

const boolean debug = false; // if true, will output raw readings also.

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
uint32_t tealLow = pixels.Color(0,64,64);
uint32_t tealHigh = pixels.Color(0,128,128);
uint32_t black = pixels.Color(0,0,0);

//=================================================================================

//Keyboard via TinyUSB
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_KEYBOARD() };

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_KEYBOARD, 2, false);

uint8_t const report_id = 0;
uint8_t const modifier = 0;

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

uint8_t lastkeychecksum=0; // record the previous keycode

//=================================================================================

// invert if needed
bool invHorz = false; // 7MM: palmstick=false, slidestick=true
bool invVert = false; // 7MM: palmstick=false, slidestick=true

//=================================================================================

int rawHorz, rawVert; // raw values from sensor
int lmtHorz, lmtVert; // limited (range checked) value
int mapHorz, mapVert; // mapped value (limited)

// set the default min/max values.

// Determined during testing. These are for geneic PS2 style joysticks.
// Adjust values based on what you actually get from the stick being used.

// Test with values from 6-12
int deadzone=15; // most sticks are 7, some are worse (at 12)

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

  Serial.begin(115200);

  // Set HORZ and VERT and BUTT pins (from joystick) to input
  pinMode(VERT_PIN, INPUT);
  pinMode(HORZ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  //Sampling centerpoint
  pixels.clear(); pixels.setPixelColor(0,yellowHigh); pixels.show();
  
  // On startup, take some readings from the (hopefully) 
  // centered joystick to determine actual center for X and Y. 
  int iNumberOfSamples = 50;
  long lSampleSumHorz = 0;
  long lSampleSumVert = 0;
  for (int iSample = 1; iSample<=iNumberOfSamples; iSample++) {
    lSampleSumHorz += analogRead(HORZ_PIN); delay(5);
    lSampleSumVert += analogRead(VERT_PIN); delay(5);
  }
  centeredHorz=int(lSampleSumHorz/iNumberOfSamples);
  centeredVert=int(lSampleSumVert/iNumberOfSamples);

  // All ready.
  pixels.clear(); pixels.setPixelColor(0,tealLow); pixels.show();

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
      if (SEL == LOW) {
        btnStick = true;
      }
      else {
        btnStick = false;
      }
    }
  }
  prevSEL = reading;
  // ----------------------------------------------------

  // Read analog values from input pins.
  rawHorz = analogRead(HORZ_PIN); delay(5);
  rawVert = analogRead(VERT_PIN); delay(5);

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

  // Map joystick movement to keyboard commands (WASD)
  // These are in arrays so the commands can be easily re-mapped to other controls
  //crsr
  //uint8_t hidcode[] = { HID_KEY_SPACE, HID_KEY_ARROW_UP, HID_KEY_ARROW_LEFT, HID_KEY_ARROW_DOWN, HID_KEY_ARROW_RIGHT };
  //wasd
  uint8_t hidcode[] = { HID_KEY_SPACE, HID_KEY_W, HID_KEY_A, HID_KEY_S, HID_KEY_D };

  // used to avoid send multiple consecutive zero report for keyboard
  static bool keyPressedPreviously = false;
  
  uint8_t count=0;
  uint8_t keycode[6] = { 0 };
  uint8_t keychecksum=0;

  if (btnStick) { keycode[count++] = hidcode[0]; keychecksum=keychecksum+1; Serial.println("PRESS "); } // Press
  if (mapVert<-deadzone) { keycode[count++] = hidcode[1]; keychecksum=keychecksum+2; Serial.print("UP "); } //Up
  if (mapHorz<-deadzone) { keycode[count++] = hidcode[2]; keychecksum=keychecksum+4; Serial.print("LEFT "); } //Left
  if (mapVert>+deadzone) { keycode[count++] = hidcode[3]; keychecksum=keychecksum+8; Serial.print("DOWN "); } //Down
  if (mapHorz>+deadzone) { keycode[count++] = hidcode[4]; keychecksum=keychecksum+16; Serial.print("RIGHT "); } //Right
  if (count>0) { Serial.print(keychecksum); Serial.println(); }
  
  if ( usb_hid.ready() ) {
    if (count>0) {
      if (keychecksum!=lastkeychecksum) {
        usb_hid.keyboardRelease(0); delay(10); // Added this because moving from "W" to "WA" (etc) didn't work w/o it.
        usb_hid.keyboardReport(report_id, modifier, keycode); 
        keyPressedPreviously = true;
        lastkeychecksum = keychecksum;
        Serial.print("PRESS "); Serial.println(keychecksum);
      }else{
        //do nothing -- user is holding down same keys
        Serial.print("HOLDING "); Serial.println(keychecksum);
      }
    }else{
      if ( keyPressedPreviously ) { 
        usb_hid.keyboardRelease(0); 
        keyPressedPreviously = false;
        lastkeychecksum = 0;
        Serial.println("RELEASE");
      }
    }
  }//hid ready

  delay(10);

  // ----------------------------------------------------

  if (debug) {serialDebug();}

}//loop


//=================================================================================

void HandleResetRequest(){

  // New feature allows user to trigger a boot loader mode, making field updates easier.

  long startTime = millis();

  while (digitalRead(BUTTON_PIN)==LOW) { 
    pixels.clear(); pixels.setPixelColor(0, pinkHigh); pixels.show(); delay(50);
    if ((millis()-startTime) > 5000) { ResetToBootLoader(); }
    //if ((millis()-startTime) > 5000) { return; }
    pixels.clear(); pixels.setPixelColor(0, whiteHigh); pixels.show(); delay(50);
  }
  
}//HandleResetRequest()

//=================================================================================

void ResetToBootLoader(){

  *(uint32_t *)(0x20000000 + 32768 -4) = 0xf01669ef;     // Store special flag value in last word in RAM.
  NVIC_SystemReset();    // Like pushing the reset button.

}//ResetToBootLoader

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
  Serial.println();;

}//serialDebug
