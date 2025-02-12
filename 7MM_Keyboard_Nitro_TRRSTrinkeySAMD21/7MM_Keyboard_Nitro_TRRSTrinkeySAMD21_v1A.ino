// 7MM_Keyboard_Nitro_TRRSTrinkeySAMD21_v1A

const long buildDate = 2024101201; // 2024-10-12-A
//HID_KEY_N

//=================================================================================

// Board: Adafruit TRRS Trinkey M0 (SAMD21)
// Optimize: Small (-Os) (standard)
// USB Stack: TinyUSB  <-- Important: Using TinyUSB, not Arduino USB!
// Debug: Off
// Port: COMxx (AdafruitTRRS Trinkey M0 (SAMD21))

// This sketch is only valid on boards which have native USB support
// and compatibility with Adafruit TinyUSB library. 
// For example SAMD21, SAMD51, nRF52840.

// You MUST use the TinyUSB package instead of the usual "keyboard.h".
// TOOLS >> USB STACK >> TinyUSB must be selected (not "arduino").

// 2024-10-02 : New product.

//=================================================================================

#if !defined(USE_TINYUSB)
 #error("Please select TinyUSB from the Tools->USB Stack menu!")
#endif

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
uint32_t tealLow = pixels.Color(0,64,64);
uint32_t tealHigh = pixels.Color(0,128,128);
uint32_t black = pixels.Color(0,0,0);

//=================================================================================

uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_KEYBOARD() };
Adafruit_USBD_HID usb_hid;

//=================================================================================

bool cableinserted = false; 
bool last_cablestate = false;

//=================================================================================

void setup() {

  if (debug) { Serial.begin(115200); }

  // Startup the NeoPixel
  pixels.begin(); 
  pixels.clear(); pixels.setPixelColor(0, redHigh); pixels.show();

  usb_hid.setBootProtocol(HID_ITF_PROTOCOL_KEYBOARD);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("Nitro Keyboard Button");
  
  // IMPORTANT: This will loop until device is mounted.
  //            So, if you see a RED light, it's powered, but not mounted.
  //            When the LED goes yellow, it's mounted.
  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(10);

}

void loop() {
  
  delay(10); // sample every 10 ms

  uint8_t keycode[6] = { 0 };
  uint8_t count = 0;
  
  // used to avoid send multiple consecutive zero report for keyboard
  static bool keyPressedPreviously = false;

  //IMPORTANT: This is a little weird here. First we set the PIN_TIP to OUTPUT
  //           so that we can check to see if the jack is inserted. Only poll if so.
  pinMode(PIN_TIP, OUTPUT); digitalWrite(PIN_TIP, LOW);
  pinMode(PIN_TIP_SWITCH, INPUT_PULLUP);  
  delay(1); // Give pins time to settle.

  cableinserted = digitalRead(PIN_TIP_SWITCH);

  if (cableinserted && !last_cablestate) {
    if (debug) { Serial.println("Plug Inserted"); }
    pixels.clear(); pixels.setPixelColor(0,greenHigh); pixels.show();
    delay(500);  // give time to plug completely
  }
  
  last_cablestate = cableinserted;
  
  if (!cableinserted) {
    keyPressedPreviously = false;
    usb_hid.keyboardRelease(0);
    pixels.clear(); pixels.setPixelColor(0,yellowHigh); pixels.show();
    return;
  }

  //At this point, the cable is inserted, so switch to INPUT
  pinMode(PIN_TIP, INPUT_PULLUP);  
  pinMode(PIN_SLEEVE, OUTPUT); digitalWrite(PIN_SLEEVE, LOW);   //Pseudo ground on sleeve
  delay(1); // Give pins time to settle.

  //                                        ---------
  //                                        ---------
  if (!digitalRead(PIN_TIP)) { keycode[0] = HID_KEY_N; count++; }
  //                                        ---------
  //                                        ---------

  if (count) {
    if ( TinyUSBDevice.suspended() ) { TinyUSBDevice.remoteWakeup(); }
    if ( !usb_hid.ready() ) return;
    //Got click and USB is ready, so send a keypress
    pixels.clear(); pixels.setPixelColor(0,blueHigh); pixels.show();   
    uint8_t const report_id = 0; uint8_t const modifier = 0;
    keyPressedPreviously = true;
    Serial.println("  Press");
    usb_hid.keyboardReport(report_id, modifier, keycode);
  }
  else
  {
    // Send All-zero report to indicate there is no keys pressed
    // Most of the time, it is, though we don't need to send zero report
    // every loop(), only a key is pressed in previous loop()
    if ( keyPressedPreviously )
    {
      keyPressedPreviously = false;
      Serial.println("  Release");
      usb_hid.keyboardRelease(0);
       pixels.clear(); pixels.setPixelColor(0,greenHigh); pixels.show();
   }
  }
}
