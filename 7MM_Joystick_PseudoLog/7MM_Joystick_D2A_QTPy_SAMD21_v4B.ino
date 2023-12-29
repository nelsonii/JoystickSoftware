// 7MM_Joystick_D2A_QTPy_SAMD21_v4A (AKA PSEUDOLOG)
// 2023-11-27-B
const long buildDate = 2023112702;

//=================================================================================

// Board: Adafruit QT PY (SAMD21)
// Optimize: Faster (-O3)
// USB Stack: TinyUSB  <-- Important: Using TinyUSB, not Arduino USB!
// Debug: Off
// Port: COMxx (Adafruit QT PY (SAMD21))

// This sketch is only valid on boards which have native USB support
// and compatibility with Adafruit TinyUSB library. 
// For example SAMD21, SAMD51, nRF52840.

// 2022-11-03 : Changed button assignment to match Slider (or other press-down sticks)
// 2023-05-24-A : Code improvements / streamlining
// 2023-05-24-A : Range and centering improvements
// 2023-06-08-A : Software based reboot (for field loading of UF2)
// 2023-11-20-A : NEW PRODUCT : Digital To Analog (D2A) Button/Stick for XBOX AC USB Ports


#if !defined(USE_TINYUSB)
 #error("Please select TinyUSB from the Tools->USB Stack menu!")
#endif

//=================================================================================

#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <FlashStorage.h> // EEPROM-like storage for SAMD21

//=================================================================================

const boolean debug = true; // if true, output info to serial monitor.

#define SERIAL_BAUDRATE     115200 // all the new boards can handle this speed
#define SERIAL_TIMEOUT      0  // set to 0 in prod, 10 sec (10000) in dev

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
uint32_t orange = pixels.Color(255,40,0);

//=================================================================================

// A structure to hold the settings. The "valid" variable is set to "true"
// when the structure if filled with actual data for the first time.
typedef struct {
  boolean valid;
  int sensitivity;
} ConSetting;

//For storing a "ConSetting" in the "localFlashStorage"
FlashStorage(localFlashStorage, ConSetting);

//defaultSettings is the variable holding the setting structure for runtime use.
ConSetting defaultSetting;

//=================================================================================

//Joystick via TinyUSB
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_GAMEPAD() };
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
hid_gamepad_report_t    joystick;

//=================================================================================

// Joystick pins
#define SW_UP A0
#define SW_DN A1  
#define SW_LT A2
#define SW_RT A3

//=================================================================================

// invert if needed
const bool invHorz = false; 
const bool invVert = false;

//=================================================================================

// values from switches
long rawLT, rawRT, rawUP, rawDN;

// converted switch values
long adjHorz, adjVert; // data from joystick (cleaned/adjusted)
long mapHorz, mapVert; // mapped values, based on raw
long accHorz, accVert; // acceleration for pseudo analog

int minHorz=0;
int centeredHorz=512;
int maxHorz=1023;

int minVert=0;
int centeredVert=512;
int maxVert=1023;

//hard limits, to catch out-of-range readings
const int limitMinHorz=0;
const int limitMaxHorz=1023;
const int limitMinVert=0;
const int limitMaxVert=1023;

// The XBOX Adapative Controller (XAC) likes joystick values from -127 to + 127
// adjust these min/max based on your upstream device. Same values for both X and Y.
const int joyMin = -127;
const int joyMax = +127;

//=================================================================================


void setup() {

  // Startup the NeoPixel
  pixels.begin(); 
  pixels.clear(); pixels.setPixelColor(0, orange); pixels.show();

  if (debug) { 
    Serial.begin(SERIAL_BAUDRATE);  
    //Wait for the serial monitor to be opened, or timeout after x seconds
    unsigned long serialTimeout = millis(); 
    while (!Serial && (millis() - serialTimeout <= SERIAL_TIMEOUT)) { delay(10); } 
    delay(200); // Give serial monitor time to catch up
    Serial.println("7MM_Joystick_D2A_QTPy_SAMD21_v4A");
    Serial.print("Version: "); Serial.println(buildDate);
  }

  pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();

  // Set U/D/L/R (from joystick) to input
  pinMode(SW_UP, INPUT_PULLUP);
  pinMode(SW_DN, INPUT_PULLUP);
  pinMode(SW_LT, INPUT_PULLUP);
  pinMode(SW_RT, INPUT_PULLUP);

  //Check for bootloader reset/request (allows field reprogramming)
  HandleResetRequest();

  //Load the last settings (if none, use defaults)
  setupFlashDefaults();

  // Startup TinyUSB for Joystick
  #if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
    // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    TinyUSB_Device_Init(0);
  #endif
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(10); // IMPORTANT: This will loop until device is mounted.

  //See if the user wants to update the sensitivity setting
  HandleConfigRequest();

  //Center on startup
  adjHorz = centeredHorz;
  adjVert = centeredVert;
  
  // All ready.
  pixels.clear(); pixels.setPixelColor(0,whiteLow); pixels.show();

}//setup


//=================================================================================


void loop() {

  // Read analog values from input pins.
  rawUP = analogRead(SW_UP); 
  rawDN = analogRead(SW_DN); 
  rawLT = analogRead(SW_LT); 
  rawRT = analogRead(SW_RT); 

  //Convert the raw digital values to pseudo analog
  processDigitalToAnalog();
   
  // Map values to a range the XAC likes.
  // Also take into account range differences on +/- of each axis (ugh!)
  if (adjHorz<centeredHorz) { mapHorz = map(adjHorz, minHorz, centeredHorz, joyMin, 0);}
  else { mapHorz = map(adjHorz, centeredHorz, maxHorz, 0, joyMax); }
  if (adjVert<centeredVert) { mapVert = map(adjVert, minVert, centeredVert, joyMin, 0);} 
  else { mapVert = map(adjVert, centeredVert, maxVert, 0, joyMax); }

  // Invert value if requested (if "up" should go "down" or "left" to "right")
  if (invHorz) {mapHorz = -mapHorz;}
  if (invVert) {mapVert = -mapVert;}

  if (debug) {serialDebug();}
  
  if ( usb_hid.ready() ) {
      joystick.x = mapHorz;
      joystick.y = mapVert;
      usb_hid.sendReport(0, &joystick, sizeof(joystick));
    }

}//loop

//=================================================================================


void processDigitalToAnalog() {

    // if digital switches are used with analog read, they will float
    // The D2A (unlike the original SwitchPseudoLog) uses COMMON GROUND
    // with a PULLUP -- therefore, look for any LOW values. They aren't always
    // zero (due to resistance in the switches), so a cutoff of about 10 is used.

    // The amount of movement is dependent on:
    // (a) how long the switch is held (longer-bigger jumps)
    // (b) multiplied by the sensitivity set (via the setting desired by user)

    int buttonThreshold = 10; // button will read analog 10 or lower when pressed
    int adjSense = defaultSetting.sensitivity; //a value of 1 to 20

    static unsigned long refreshInterval = map(adjSense, 1, 20, 50, 5); 
    static unsigned long lastRefreshTime = 0;
    
    if(millis()-lastRefreshTime < refreshInterval ) { return; }
    else lastRefreshTime += refreshInterval;
    
    //vertical - up/down
    if (rawUP<=buttonThreshold) { //toward min (subtract)
      accVert = constrain(accVert+adjSense, 0, 1023);
      adjVert = constrain(adjVert-accVert, 0, 1023);
      } 
    else if (rawDN<=buttonThreshold) { //toward max (add)
      accVert = constrain(accVert+adjSense, 0, 1023);
      adjVert = constrain(adjVert+accVert, 0, 1023);
      }
    else { //reset / re-center
      //accVert=0; //reset
      //Gradual recentering, decay time is have of attack, and I'm not sure why. :-(
      if (adjVert>centeredVert) { adjVert=constrain(adjVert-accVert, 512, 1023) ; }
      else if (adjVert<centeredVert) { adjVert=constrain(adjVert+accVert, 0, 512); }
      else { adjVert=centeredVert; accVert=0; } // re-center fallback
      }
    
    //horizontal -- left/right
    if (rawLT<=buttonThreshold) { //toward min (subtract)   
      accHorz = constrain(accHorz+adjSense, 0, 1023);
      adjHorz = constrain(adjHorz-accHorz, 0, 1023);
      } 
    else if (rawRT<=buttonThreshold) { //toward max (add)
      accHorz = constrain(accHorz+adjSense, 0, 1023);
      adjHorz = constrain(adjHorz+accHorz, 0, 1023);
      } 
    else { //reset / re-center 
      //Gradual recentering, decay time is have of attack, and I'm not sure why. :-(
      if (adjHorz>centeredHorz) { adjHorz=constrain(adjHorz-accHorz, 512, 1023) ; }
      else if (adjHorz<centeredHorz) { adjHorz=constrain(adjHorz+accHorz, 0, 512); }
      else { adjHorz=centeredHorz; accHorz=0; }//re-center fallback
      }

}//processDigitalReadings

//=================================================================================

void setupFlashDefaults() {

  defaultSetting = localFlashStorage.read();

  if (defaultSetting.valid==true) {
    if (debug) {
      Serial.println("Flash Settings Found and Loaded.");
    }
    return;
  }//found

  if (defaultSetting.valid==false) {
    if (debug) Serial.println("   No Flash Settings found.");
    //Nothing in flash storage, put in some defaults
    defaultSetting.valid = true;
    defaultSetting.sensitivity = 10; // range will be 1 to 20
    //Store the new defaults in flash
    localFlashStorage.write(defaultSetting);
    if (debug) Serial.println("   Defaults Settings written.");
    return;    
  }//missing
 
}//setupFlashDefaults

//=================================================================================

void HandleConfigRequest(){

  long startTime = millis();
  //PULLUP so LOW = Pressed
  while (digitalRead(SW_DN)==LOW) { 
    pixels.clear(); pixels.setPixelColor(0, blueHigh); pixels.show(); delay(50);
    if ((millis()-startTime) > 2000) { ReConfigure(); }
    pixels.clear(); pixels.setPixelColor(0, whiteLow); pixels.show(); delay(50);
  }

}//HandleConfigRequest

//=================================================================================

void ReConfigure() {

  
  //wait for user to release down button
  while (digitalRead(SW_DN) == LOW) {  yield(); delay(1); } // wait for button release
  
  pixels.clear(); pixels.setPixelColor(0, yellowHigh); pixels.show();

  int newSensitivity = defaultSetting.sensitivity;
  int shiftBy = 0;
  
  //Keep accepting commands until user presses down button (acts as "enter")
  while(digitalRead(SW_DN)==HIGH) {
   //Read Left/Right buttons for Down/Up Value
     if (digitalRead(SW_LT)==LOW) {
      pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();
      while (digitalRead(SW_LT)==LOW) { yield(); delay(1); } // wait for button release
      shiftBy = -1;
     }else if (digitalRead(SW_RT)==LOW) {
      pixels.clear(); pixels.setPixelColor(0, greenHigh); pixels.show();
      while (digitalRead(SW_RT)==LOW) { yield(); delay(1); } // wait for button release
      shiftBy = -1;
     } else {
      pixels.clear(); pixels.setPixelColor(0, yellowHigh); pixels.show();
      shiftBy = 0;
     }
     newSensitivity = constrain(newSensitivity+shiftBy, 1, 20);
     yield(); delay(1);
   }

  //wait for user to release down button
  while (digitalRead(SW_DN) == LOW) {  yield(); delay(1); }  // wait for button release

  //If Sensitivity Value changed, save to EEPROM
  if (newSensitivity!=defaultSetting.sensitivity) {
    defaultSetting.valid = true;
    defaultSetting.sensitivity = newSensitivity;
    localFlashStorage.write(defaultSetting);
  }

  NVIC_SystemReset();    // Like pushing the reset button.
  
}//ReConfigure

//=================================================================================

void HandleResetRequest(){

  // New feature allows user to trigger a boot loader mode, making field updates easier.
  long startTime = millis();
  //PULLUP so LOW = Pressed
  while (digitalRead(SW_UP)==LOW) { 
    pixels.clear(); pixels.setPixelColor(0, pinkHigh); pixels.show(); delay(50);
    //if ((millis()-startTime) > 5000) { return; } // TEST ***********************
    if ((millis()-startTime) > 5000) { ResetToBootLoader(); } // PROD ***********************
    pixels.clear(); pixels.setPixelColor(0, whiteLow); pixels.show(); delay(50);
  }
  
}//HandleResetRequest()

//=================================================================================

void ResetToBootLoader(){
  *(uint32_t *)(0x20000000 + 32768 -4) = 0xf01669ef;     // Store special flag value in last word in RAM.
  NVIC_SystemReset();    // Like pushing the reset button.
}//ResetToBootLoader

//=================================================================================

void serialDebug() {

  Serial.print(buildDate);Serial.print("\t");
  Serial.print("Sen: ");Serial.print(defaultSetting.sensitivity); Serial.print("\t");
  /*
  //Raw
  Serial.print("Raw U/D/L/R: ");
  Serial.print(rawUP); Serial.print("\t");
  Serial.print(rawDN); Serial.print("\t");
  Serial.print(rawLT); Serial.print("\t");
  Serial.print(rawRT); Serial.print("\t");
  */
  //Adjusted
  Serial.print("AH:"); Serial.print(adjHorz); Serial.print("\t");
  Serial.print("AV:"); Serial.print(adjVert); Serial.print("\t");
  //Mapped
  Serial.print("MH: "); Serial.print(mapHorz);
  if (invHorz) { Serial.print("i"); } // i indicates value was inverted 
  Serial.print("\t");
  Serial.print("MV: "); Serial.print(mapVert);
  if (invVert) { Serial.print("i"); } // i indicates value was inverted
  Serial.println();
  
}//serialDebug
