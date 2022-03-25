//7MM_Joystick_Slide_v2E

//=================================================================================

#include "Joystick.h"

//=================================================================================

Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, 
  JOYSTICK_TYPE_JOYSTICK, 1, 0,
  true, true, false, false, false, false,
  false, false, false, false, false);

// constructor: use default id, it's a joystick, there is one button,
//              there are no hat buttons, there is an X and Y analog
//              and no other controls.

//=================================================================================

const boolean debug = true; // if true, output info to serial monitor.
const boolean graph = false; // if true, output data for plotter/graph viewing.

//=================================================================================

// Joystick pins
#define BUTTON_PIN  A1 // physical pin 19 // Pushbutton of the thumbstick
#define VERT_PIN    A2 // physical pin 20 // AKA the "Y" -- dependent on how thumbstick is oriented
#define HORZ_PIN    A3 // physical pin 21 // AKA the "X" -- dependent on how thumbstick is oriented

// Mapping to XBOX Adaptive Controller
// X and Y will auto map to Axis 0/1 or 2/3 depending on which side you plug
// the joystick into (left=0/1 or right=2/3). The pushbutton on the stick needs
// to be set & programmed depending on placement. 
// ********* TODO: Some sort of config on stick?
// 10=Left Stick Press, 11=Right Stick Press
#define XBOX_AC_STICK_BUTTON   10 

// Joystick variables - Button (with debounce)
int SEL, prevSEL = HIGH;
long lastDebounceTime = 0;
long debounceDelay = 50;  //millis

//=================================================================================

// values from analog stick
int rawHorz, rawVert;
int mapHorz, mapVert;

// invert if needed
bool invHorz = true; // 7MM: palmstick=false, slidestick=true
bool invVert = true; // 7MM: palmstick=false, slidestick=true

// set the default min/max values. determined during testing.
int minHorz=0;
int maxHorz=1023;
int minVert=0;
int maxVert=1023;

//=================================================================================


void setup() {

  if (debug) { Serial.begin(115200); }

  // Set HORZ and VERT pins (from joystick) to input
  pinMode(VERT_PIN, INPUT);
  pinMode(HORZ_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  //Turn off the TX/RX red LEDs
  pinMode(LED_BUILTIN_TX,INPUT);
  pinMode(LED_BUILTIN_RX,INPUT);
    
  // Startup the joystick object. 
  Joystick.begin(false); // false indicates that sendState method call required (so we do all changes at once).

  // I'm using Map below, but setting range Just In Case.
  // Also setting Axis to zero at startup.
  Joystick.setXAxisRange(-127, 127); Joystick.setXAxis(0);
  Joystick.setYAxisRange(-127, 127); Joystick.setYAxis(0);
  Joystick.sendState();

  // init mins and maxes
  minHorz = 102;//analogRead(HORZ_PIN);
  maxHorz = 922;//analogRead(HORZ_PIN);
  minVert = 102;//analogRead(VERT_PIN);
  maxVert = 922;//analogRead(VERT_PIN);

}//setup


//=================================================================================


void loop() {

  // ----------------------------------------------------

  // Check for button push - and debounce
  int reading = digitalRead(BUTTON_PIN);
  if (reading != prevSEL) { lastDebounceTime = millis(); } // reset timer
  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != SEL) { SEL = reading;
      if (SEL == LOW) {Joystick.pressButton(XBOX_AC_STICK_BUTTON);}
      else {Joystick.releaseButton(XBOX_AC_STICK_BUTTON);}
    }
  }
  prevSEL = reading;

  // ----------------------------------------------------

  // Read analog values from input pins.
  rawHorz = analogRead(HORZ_PIN); delay(25);
  rawVert = analogRead(VERT_PIN); delay(25);

  // Toss out invalid ranges
  if (rawHorz<0 || rawHorz>1023) {rawHorz=512;} 
  if (rawVert<0 || rawVert>1023) {rawVert=512;} 
  
  // set mins and maxes
  if (rawHorz<minHorz) {minHorz=rawHorz;}
  if (rawHorz>maxHorz) {maxHorz=rawHorz;}
  if (rawVert<minVert) {minVert=rawVert;}
  if (rawVert>maxVert) {maxVert=rawVert;}
  
  // Map values to a range the XAC likes
  mapHorz = map(rawHorz, minHorz, maxHorz, -127, 127);
  mapVert = map(rawVert, minVert, maxVert, -127, 127);

  if (debug) {
    if (!graph){
      Serial.print("Raw H/V: ");
      Serial.print(rawHorz);
      Serial.print(" ");
      Serial.print(rawVert);
      Serial.print(" ");
    }
    if (!graph) {
      Serial.print("Min/Max Horz: ");
      Serial.print(minHorz);
      Serial.print(" ");
      Serial.print(maxHorz);
      Serial.print(" ");
    }
    if (!graph) {
      Serial.print("Min/Max Vert: ");
      Serial.print(minVert);
      Serial.print(" ");
      Serial.print(maxVert);
      Serial.print(" ");
    }
    if (!graph) {Serial.print("Button: ");}
    Serial.print(digitalRead(BUTTON_PIN));
    Serial.print(" ");
    if (!graph) {Serial.print("MAP H/V: ");}
    Serial.print(mapHorz);
    Serial.print(" ");
    Serial.print(mapVert);
    Serial.println();
  }

  // Invert value if requested (if "up" should go "down" or "left" to "right")
  if (invHorz) {mapHorz = -mapHorz;}
  if (invVert) {mapVert = -mapVert;}

  // Send the values to the joystick object
  Joystick.setXAxis(mapHorz);
  Joystick.setYAxis(mapVert);

  // ----------------------------------------------------

  // Send updated joystick state to HID upstream
  Joystick.sendState();

}//loop


//=================================================================================
