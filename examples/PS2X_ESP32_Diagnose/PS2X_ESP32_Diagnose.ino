/*
 * PS2X_ESP32_Diagnose — serial console for bringing up a PS/2 controller
 * on an ESP32 (also compiles on AVR).
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * Upload, open the serial monitor at 115200 and watch the output.
 * Commands (type a letter + Enter):
 *   c  run config_gamepad() again (hot-plug, after fixing the wiring)
 *   d  dump the complete internal state of the library
 *   r  rumble test (small + big motor)
 *   s  slow the bus down (more tolerant with long wires)
 *   f  speed the bus up
 *   ?  help
 */

#include <PS2X_lib.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define PS2_DAT 27
  #define PS2_CMD 23
  #define PS2_SEL 19
  #define PS2_CLK 18
#else
  #define PS2_DAT 13
  #define PS2_CMD 11
  #define PS2_SEL 10
  #define PS2_CLK 12
#endif

PS2X ps2x;

uint8_t  err       = PS2X_ERR_NOT_CONFIGURED;
uint8_t  lastType  = 0;
uint16_t lastBtn   = 0xFFFF;
uint32_t lastStatus = 0;
bool     firstStatus = true;

void printHeader() {
  Serial.println();
  Serial.println(F("=== PS2X_lib diagnose ==="));
#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("chip      : "));
  Serial.print(ESP.getChipModel());
  Serial.print(F(" rev "));
  Serial.println(ESP.getChipRevision());
  Serial.print(F("cpu       : "));
  Serial.print(getCpuFrequencyMhz());
  Serial.println(F(" MHz"));
#endif
  Serial.print(F("library   : "));
  Serial.println(F(PS2X_LIB_VERSION));
  Serial.print(F("pins      : dat="));
  Serial.print(PS2_DAT);
  Serial.print(F(" cmd="));
  Serial.print(PS2_CMD);
  Serial.print(F(" att="));
  Serial.print(PS2_SEL);
  Serial.print(F(" clk="));
  Serial.println(PS2_CLK);
  Serial.print(F("timing    : "));
  Serial.print(ps2x.clkLowUs());
  Serial.print('/');
  Serial.print(ps2x.clkHighUs());
  Serial.print('/');
  Serial.print(ps2x.byteGapUs());
  Serial.println(F(" us (low/high/byte-gap)"));
  Serial.println(F("type '?' for the command list"));
}

void printHelp() {
  Serial.println(F("commands: c=reconfigure  d=dump  r=rumble  s=slower  f=faster  ?=help"));
}

void printError() {
  Serial.print(F("error "));
  Serial.print(err);
  Serial.print(F(" ("));
  Serial.print(PS2X::errorText(err));
  Serial.println(')');
  if (err == PS2X_ERR_BAD_PIN) {
    Serial.println(F("  -> ESP32: GPIO 6..11 are SPI flash, 34..39 input only,"));
    Serial.println(F("     do not use a strapping pin (0, 2, 12, 15) for DAT."));
  }
  if (err == PS2X_ERR_NO_CONTROLLER) {
    Serial.println(F("  -> check: receiver power (3.3V), GND common, DAT pull-up,"));
    Serial.println(F("     CLK/CMD/ATT not swapped, receiver paired with the pad."));
  }
}

bool configure() {
  err = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
  Serial.print(F("config_gamepad() -> "));
  printError();

  lastType = ps2x.readType();
  Serial.print(F("controller type   : "));
  Serial.println(PS2X::typeText(lastType));

  if (err == PS2X_OK) {
    Serial.print(F("first frame       : "));
    Serial.print(ps2x.frameMicros());
    Serial.println(F(" us"));
  }
  return err == PS2X_OK;
}

void rumbleTest() {
  if (err != PS2X_OK) {
    Serial.println(F("rumble: no controller"));
    return;
  }
  Serial.println(F("rumble: small motor on, big motor 0..255"));
  ps2x.enableRumble();
  ps2x.read_gamepad(true, 0);
  delay(500);
  /* int, not uint8_t: a uint8_t counter would wrap at 255 and never leave
   * the loop */
  for (int v = 0; v <= 255; v += 5) {
    ps2x.read_gamepad(true, (uint8_t)v);
    delay(30);
  }
  ps2x.read_gamepad(false, 0);
  delay(200);
  Serial.println(F("rumble: done"));
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 'c':
        configure();
        break;
      case 'd':
        ps2x.debugPrint(Serial);
        break;
      case 'r':
        rumbleTest();
        break;
      case 's':
        ps2x.setTiming(ps2x.clkLowUs() + 2, ps2x.clkHighUs() + 1,
                       ps2x.byteGapUs() + 2);
        Serial.print(F("timing now "));
        Serial.print(ps2x.clkLowUs());
        Serial.print('/');
        Serial.print(ps2x.clkHighUs());
        Serial.print('/');
        Serial.print(ps2x.byteGapUs());
        Serial.println(F(" us"));
        configure();
        break;
      case 'f':
        ps2x.setTiming(ps2x.clkLowUs() > 3 ? ps2x.clkLowUs() - 1 : 3,
                       ps2x.clkHighUs() > 2 ? ps2x.clkHighUs() - 1 : 2,
                       ps2x.byteGapUs() > 3 ? ps2x.byteGapUs() - 1 : 3);
        Serial.print(F("timing now "));
        Serial.print(ps2x.clkLowUs());
        Serial.print('/');
        Serial.print(ps2x.clkHighUs());
        Serial.print('/');
        Serial.print(ps2x.byteGapUs());
        Serial.println(F(" us"));
        configure();
        break;
      case '?':
      case '\n':
      case '\r':
        if (c == '?') printHelp();
        break;
      default:
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  printHeader();
  configure();
}

void loop() {
  handleSerial();

  bool ok = ps2x.read_gamepad(false, 0);

  if (!ok) {
    if (err != PS2X_ERR_NO_CONTROLLER) {
      /* the library re-initialises the controller on its own */
      err = ps2x.lastError();
    }
    if (millis() - lastStatus > 2000) {
      lastStatus = millis();
      Serial.print(F("["));
      Serial.print(millis() / 1000);
      Serial.print(F("s] no valid frame (failed="));
      Serial.print(ps2x.failedReads());
      Serial.print(F(", last ok "));
      Serial.print(ps2x.lastOkAge());
      Serial.println(F(" ms ago)"));
    }
    return;
  }

  err = PS2X_OK;

  uint16_t buttons = (~ps2x.ButtonDataByte()) & 0xFFFF;  /* pressed bits */

  if (buttons != lastBtn || firstStatus) {
    lastBtn = buttons;
    Serial.print(F("buttons 0x"));
    if (buttons < 0x1000) Serial.print('0');
    if (buttons < 0x100) Serial.print('0');
    if (buttons < 0x10) Serial.print('0');
    Serial.print(buttons, HEX);
    Serial.print(F("  LX="));
    Serial.print(ps2x.Analog(PSS_LX));
    Serial.print(F(" LY="));
    Serial.print(ps2x.Analog(PSS_LY));
    Serial.print(F(" RX="));
    Serial.print(ps2x.Analog(PSS_RX));
    Serial.print(F(" RY="));
    Serial.println(ps2x.Analog(PSS_RY));
  }

  if (firstStatus || millis() - lastStatus > 1000) {
    firstStatus = false;
    lastStatus  = millis();
    Serial.print(F("status ok frame="));
    Serial.print(ps2x.frameMicros());
    Serial.print(F("us gap="));
    Serial.print(ps2x.readDelay());
    Serial.print(F("ms fails="));
    Serial.print(ps2x.failedReads());
    Serial.println();
  }
}
