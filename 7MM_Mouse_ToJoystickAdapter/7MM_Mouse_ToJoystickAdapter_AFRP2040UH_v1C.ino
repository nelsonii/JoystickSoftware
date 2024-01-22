//7MM_Mouse_ToJoystickAdapter_AFRP2040UH_v1C
// 2023-07-31-A
const long buildDate = 2023073101;

//=================================================================================

// Board: Adafruit Feather RP2040 USB Host
// CPU Speed: 120 Mhz (critical -- change from 133 Mhz)
// Debug Port: Enabled on Serial (default is Disabled)
// USB Stack: TinyUSB  <-- Important: Using TinyUSB, not Arduino USB!
// All other settings are default.

//------------------------------------------------------------------
// In my dual IDE environment, I recommend using Arduino 2.1.1 (2.x)
//------------------------------------------------------------------

// 2023-06-22-A : New product, based on Palm Float Joystick code. Extensive rewrite.

//=================================================================================

// You MUST use the TinyUSB package instead of the usual "joystick.h".
// TOOLS >> USB STACK >> TinyUSB must be selected (not "arduino").
#if !defined(USE_TINYUSB)
 #error("Please select TinyUSB from the Tools->USB Stack menu!")
#endif

//=================================================================================

#include "pio_usb.h" // required for RP2040 host function
#include "Adafruit_TinyUSB.h"
#include <elapsedMillis.h> // https://reference.arduino.cc/reference/en/libraries/elapsedmillis/
#include <EEPROM.h> // EEPROM-like storage for RP2040

//=================================================================================

// USB Host object
Adafruit_USBH_Host USBHost;

//=================================================================================

//Joystick via TinyUSB
uint8_t const desc_joystick_report[] = { TUD_HID_REPORT_DESC_GAMEPAD() };
Adafruit_USBD_HID usb_joystick(desc_joystick_report, sizeof(desc_joystick_report), HID_ITF_PROTOCOL_NONE, 2, false);
hid_gamepad_report_t    joystick;

//=================================================================================

enum { PIXEL_BRIGHTNESS = 0x20 }; // 32

enum {
  RED = 0xff0000,
  YELLOW = 0xffff00,
  GREEN = 0x00ff00,
  BLUE = 0x0000ff,
  PINK = 0xff66ff,
  ORANGE = 0xe8760c
};

enum {
  COLOR_STARTUP = BLUE,
  COLOR_ERROR = RED,
  COLOR_MOUNT_MOUSE = GREEN,
  COLOR_MOUNT_OTHER = ORANGE,
  COLOR_UNMOUNT = RED
};

void setPixel(uint32_t color); // prototype (bit banging code below)

//=================================================================================

int sensitivity = 1; // default multiplier of 1

elapsedMillis MouseLastMoved;

//=================================================================================

int rawHorz, rawVert; // raw values from sensor
int lmtHorz, lmtVert; // limited (range checked) value
int mapHorz, mapVert; // mapped value (limited)

const bool invHorz = false; 
const bool invVert = false; 

//Mouse ranges 
int minHorz=-10;
int centeredHorz=0;
int maxHorz=+10;
int absoluteLimitHorz = 100;

int minVert=-10;
int centeredVert=0;
int maxVert=+10;
int absoluteLimitVert = 100;

int deadzone=1;

// The XBOX Adapative Controller (XAC) likes joystick values from -127 to + 127
// adjust these min/max based on your upstream device. Same values for both X and Y.
const int joyMin = -127;
const int joyMax = +127;

//=================================================================================


void setup() {
  
  //Core0 on RP2040 : Setup

  // Enable neopixel (bit banging)
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
  pinMode(PIN_NEOPIXEL, OUTPUT);
  digitalWrite(PIN_NEOPIXEL, LOW);
  setPixel(0);
  delay(1);
  setPixel(COLOR_STARTUP);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  //Get upstream usb working (seen as joystick by XBOX AC/PC)
  TinyUSB_Device_Init(0);
  usb_joystick.begin();
  while( !TinyUSBDevice.mounted() ) {Serial.println("not mounted"); setPixel(COLOR_ERROR); delay(500); setPixel(COLOR_STARTUP); }

  sensitivity=getDefaultSensitivity();


}//setup


//=================================================================================

void loop() {

  //Core0 on RP2040
  //The Core0 loop doesn't do much, as the translation is handled by
  //Core1 (HOST) and it's report received callbacks.
  Serial.flush();

  //Center the joystick after 100 ms of non mouse movement
  if (MouseLastMoved > 100) {
    if (usb_joystick.ready()) {
      joystick.x = 0; joystick.y = 0;
      usb_joystick.sendReport(0, &joystick, sizeof(joystick));
    }else{Serial.println("usb_joystick not ready");}
    digitalWrite(LED_BUILTIN, LOW);
    MouseLastMoved = 0;
  }//mouselastmoved

}//loop

//=================================================================================

void setup1() {

  //Core1 on RP2040 : Setup

  Serial.println("Core1 setup to run TinyUSB host with pio-usb");

  // Check for CPU frequency, must be multiple of 120Mhz for bit-banging USB
  uint32_t cpu_hz = clock_get_hz(clk_sys);
  if (cpu_hz != 120000000UL && cpu_hz != 240000000UL) {
    while (!Serial) {  delay(10); } // wait for native usb
    Serial.printf("Error: CPU Clock = %lu, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
    Serial.printf("Change your CPU Clock to either 120 or 240 Mhz in Menu->CPU Speed \r\n");
    while (1) { delay(1); }
  }

  pinMode(PIN_5V_EN, OUTPUT); digitalWrite(PIN_5V_EN, HIGH);

  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have
  // most of the host bit-banging processing works done in core1 to free up
  // core0 for other works
  USBHost.begin(1);

}

//=================================================================================

void loop1() {

  USBHost.task();

}

//=================================================================================

//--------------------------------------------------------------------+
// TinyUSB Host callbacks
// Note: running in the same core where Brain.USBHost.task() is called
//--------------------------------------------------------------------+
extern "C" {

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use.
// tuh_hid_parse_report_descriptor() can be used to parse common/simple enough
// descriptor. Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE,
// it will be skipped therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
  
  (void) desc_report;
  (void) desc_len;
  uint16_t vid, pid;

  tuh_vid_pid_get(dev_addr, &vid, &pid);

  Serial.printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
  Serial.printf("VID = %04x, PID = %04x\r\n", vid, pid);

  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
    setPixel(COLOR_MOUNT_MOUSE); 
    Serial.printf("HID Mouse\r\n");
    if (!tuh_hid_receive_report(dev_addr, instance)) {
      Serial.printf("Error: cannot request to receive report\r\n");
    }
  }else{ setPixel(COLOR_MOUNT_OTHER); }

}//mount


// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  setPixel(COLOR_UNMOUNT); 
  Serial.printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}//unmount


// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,uint8_t const *report, uint16_t len) {
  
  setPixel(COLOR_MOUNT_MOUSE); 
  digitalWrite(LED_BUILTIN, HIGH);  
  MouseLastMoved = 0;

  //Translate the mouse report to joystick
  translate_report((hid_mouse_report_t const *) report);
  // continue to request to receive report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.printf("Error: cannot request to receive report\r\n");
  }

}//received

} // extern C

//=================================================================================

void translate_report(hid_mouse_report_t const* report) {



  Serial.printf("X: %d Y: %d \r\n", report->x, report->y);

  rawHorz = report->x;
  rawVert = report->y;

  //Absolute limits -- just in case we get some really weird values
  lmtHorz = constrain(rawHorz, -absoluteLimitHorz, +absoluteLimitHorz);
  lmtVert = constrain(rawVert, -absoluteLimitVert, +absoluteLimitVert);

  // update the min/max during run, in case we see something outside of the normal
  if (rawHorz<minHorz) {minHorz=lmtHorz;}
  if (rawHorz>maxHorz) {maxHorz=lmtHorz;}
  if (rawVert<minVert) {minVert=lmtVert;}
  if (rawVert>maxVert) {maxVert=lmtVert;}

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

  if (usb_joystick.ready()) {
    joystick.x = mapHorz*sensitivity;
    joystick.y = mapVert*sensitivity;
    usb_joystick.sendReport(0, &joystick, sizeof(joystick));
  }else{Serial.println("usb_joystick not ready");}


}


//=================================================================================

int getDefaultSensitivity() {

  EEPROM.begin(256);

  //Note: EEPROM can only hold a value from 0 to 255

  int eeAddress = 0; // Address zero
  byte eeValue = EEPROM.read(eeAddress); 

  //If invalid value 1 and update EEPROM
  if (eeValue < 1 || eeValue > 10) {
    eeValue = 1;
    EEPROM.write(eeAddress, eeValue);
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed.");
    } else {
      Serial.println("ERROR! EEPROM commit failed!");
    } 
  }
  
  return (int) eeValue;
 
}

//=================================================================================

//--------------------------------------------------------------------+
// Neopixel bit-banging since PIO is used by USBH
//--------------------------------------------------------------------+

//setPixel
void __no_inline_not_in_flash_func(setPixel)(uint32_t color) {
  static uint32_t end_us = 0;
  static uint32_t last_color = 0x123456;

  if (last_color == color) {
    // no need to update
    return;
  }
  last_color = color;

  uint8_t r = (uint8_t)(color >> 16); // red
  uint8_t g = (uint8_t)(color >> 8);  // green
  uint8_t b = (uint8_t)color;

  // brightness correction
  r = (uint8_t)((r * PIXEL_BRIGHTNESS) >> 8);
  g = (uint8_t)((g * PIXEL_BRIGHTNESS) >> 8);
  b = (uint8_t)((b * PIXEL_BRIGHTNESS) >> 8);

  uint8_t buf[3] = {g, r, b};

  uint8_t *ptr, *end, p, bitMask;

  enum { PIN_MASK = (1ul << PIN_NEOPIXEL) };

  ptr = buf;
  end = ptr + 3;
  p = *ptr++;
  bitMask = 0x80;

  // wait for previous frame to finish
  enum { FRAME_TIME_US = 300 };

  uint32_t now = end_us;
  while (now - end_us < FRAME_TIME_US) {
    now = micros();
    if (now < end_us) {
      // micros() overflow
      end_us = now;
    }
  }

  uint32_t const isr_context = save_and_disable_interrupts();

  // RP2040 is 120 MHz, 120 cycle = 1us = 1000 ns
  // Neopixel is 800 KHz, 1T = 1.25 us = 150 nop
  while (1) {
    if (p & bitMask) {
      // T1H 0,8 us = 96 - 1 = 95 nop
      sio_hw->gpio_set = PIN_MASK;
      __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");

      // T1L 0,45 = 54 - 10 (ifelse) - 5 (overhead) = 44 nop
      sio_hw->gpio_clr = PIN_MASK;
      __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    } else {
      // T0H 0,4 us = 48 - 1 = 47 nop
      sio_hw->gpio_set = PIN_MASK;
      __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop;");

      // T0L 0.85 us = 102 - 10 (ifelse) - 5 (overhead) = 87 nop
      sio_hw->gpio_clr = PIN_MASK;
      __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"
                     "nop; nop; nop; nop;");
    }

    if (bitMask >>= 1) {
      // not full byte, shift to next bit
      __asm volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    } else {
      // probably take 10 nops
      // if a full byte is sent, next to another byte
      if (ptr >= end) {
        break;
      }
      p = *ptr++;
      bitMask = 0x80;
    }
  }

  restore_interrupts(isr_context);

  end_us = micros();
}
