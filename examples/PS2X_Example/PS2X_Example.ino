/*
 * PS2X_Example — full demo for PS2X_lib 2.0 (LROB.RU edition, https://lrob.ru)
 *
 * Same API as the original PS2X_lib 1.9 example (Bill Porter, Shutter,
 * Eric Wetzel, Kurt Eckhardt — GPLv3), so existing sketches keep working.
 * Works on AVR and on ESP32.
 *
 * Wiring (see README.md):
 *   controller   ESP32        Arduino Uno
 *   ---------    -----        -----------
 *   DAT (data)   GPIO27       D13
 *   CMD (cmd)    GPIO23       D11
 *   ATT (sel)    GPIO19       D10
 *   CLK (clock)  GPIO18       D12
 *   VCC          3.3V *       3.3V *
 *   GND          GND          GND
 *
 *  * Most receivers are 3.3 V parts.  Do NOT feed 5 V into an ESP32 pin
 *    (DAT is an ESP32 input!).  If your receiver really needs 5 V use a
 *    level shifter or at least a 2.2k/3.3k divider on DAT.
 */

#include <PS2X_lib.h>

/* ---------------- pin assignment ---------------- */
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

/* ---------------- modes ----------------
 * pressures = analog reading of the buttons (only supported by some
 *             controllers, and it forces a 21 byte frame per read)
 * rumble    = vibration motors enabled
 */
#define pressures false
#define rumble    false

PS2X ps2x;

uint8_t errorCode = 0;
uint8_t type      = 0;
uint8_t vibrate   = 0;

void setup() {
  Serial.begin(115200);
  delay(300);   /* let a wireless receiver boot up */

  /* GamePad(clock, command, attention, data, pressures?, rumble?) */
  errorCode = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT,
                                  pressures, rumble);

  Serial.print(F("config_gamepad() -> "));
  Serial.print(errorCode);
  Serial.print(F(" ("));
  Serial.print(PS2X::errorText(errorCode));
  Serial.println(F(")"));

  if (errorCode == PS2X_OK) {
    Serial.println(F("Controller found and configured."));
    Serial.println(F("X vibrates the controller, L1/R1 print the sticks."));
  } else {
    Serial.println(F("Check wiring and power, then press reset."));
    Serial.println(F("Tip: run the PS2X_ESP32_Diagnose sketch for details."));
  }

  type = ps2x.readType();
  Serial.print(F("Controller type: "));
  Serial.println(PS2X::typeText(type));
}

void loop() {
  if (errorCode != PS2X_OK) {
    /* retry the configuration every second so the controller can be
     * plugged in later (hot-plug is handled by the library as well) */
    delay(1000);
    errorCode = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT,
                                    pressures, rumble);
    type = ps2x.readType();
    return;
  }

  if (type == 2) {
    /* ---------------- Guitar Hero ---------------- */
    ps2x.read_gamepad();

    if (ps2x.ButtonPressed(GREEN_FRET))  Serial.println(F("Green Fret"));
    if (ps2x.ButtonPressed(RED_FRET))    Serial.println(F("Red Fret"));
    if (ps2x.ButtonPressed(YELLOW_FRET)) Serial.println(F("Yellow Fret"));
    if (ps2x.ButtonPressed(BLUE_FRET))   Serial.println(F("Blue Fret"));
    if (ps2x.ButtonPressed(ORANGE_FRET)) Serial.println(F("Orange Fret"));
    if (ps2x.ButtonPressed(STAR_POWER))  Serial.println(F("Star Power"));

    if (ps2x.Button(UP_STRUM))   Serial.println(F("Up Strum"));
    if (ps2x.Button(DOWN_STRUM)) Serial.println(F("Down Strum"));
    if (ps2x.Button(PSB_START))  Serial.println(F("Start held"));
    if (ps2x.Button(PSB_SELECT)) Serial.println(F("Select held"));

    if (ps2x.Button(ORANGE_FRET)) {
      Serial.print(F("Whammy bar: "));
      Serial.println(ps2x.Analog(WHAMMY_BAR), DEC);
    }
  } else {
    /* ---------------- DualShock ---------------- */
    /* rumble: motor 1 = small on/off, motor 2 = big motor 0..255 */
    ps2x.read_gamepad(false, vibrate);

    if (ps2x.Button(PSB_START))  Serial.println(F("Start held"));
    if (ps2x.Button(PSB_SELECT)) Serial.println(F("Select held"));

    if (ps2x.Button(PSB_PAD_UP)) {
      Serial.print(F("Up held this hard: "));
      Serial.println(ps2x.Analog(PSAB_PAD_UP), DEC);
    }
    if (ps2x.Button(PSB_PAD_RIGHT)) {
      Serial.print(F("Right held this hard: "));
      Serial.println(ps2x.Analog(PSAB_PAD_RIGHT), DEC);
    }
    if (ps2x.Button(PSB_PAD_LEFT)) {
      Serial.print(F("Left held this hard: "));
      Serial.println(ps2x.Analog(PSAB_PAD_LEFT), DEC);
    }
    if (ps2x.Button(PSB_PAD_DOWN)) {
      Serial.print(F("Down held this hard: "));
      Serial.println(ps2x.Analog(PSAB_PAD_DOWN), DEC);
    }

    /* vibrate with the big motor depending on how hard X is pressed */
    vibrate = ps2x.Analog(PSAB_CROSS);

    if (ps2x.NewButtonState()) {
      if (ps2x.Button(PSB_L3))       Serial.println(F("L3 pressed"));
      if (ps2x.Button(PSB_R3))       Serial.println(F("R3 pressed"));
      if (ps2x.Button(PSB_L2))       Serial.println(F("L2 pressed"));
      if (ps2x.Button(PSB_R2))       Serial.println(F("R2 pressed"));
      if (ps2x.Button(PSB_TRIANGLE)) Serial.println(F("Triangle pressed"));
    }

    if (ps2x.ButtonPressed(PSB_CIRCLE))  Serial.println(F("Circle just pressed"));
    if (ps2x.NewButtonState(PSB_CROSS))  Serial.println(F("X just changed"));
    if (ps2x.ButtonReleased(PSB_SQUARE)) Serial.println(F("Square just released"));

    if (ps2x.Button(PSB_L1) || ps2x.Button(PSB_R1)) {
      Serial.print(F("Stick LY,LX,RY,RX: "));
      Serial.print(ps2x.Analog(PSS_LY), DEC); Serial.print(',');
      Serial.print(ps2x.Analog(PSS_LX), DEC); Serial.print(',');
      Serial.print(ps2x.Analog(PSS_RY), DEC); Serial.print(',');
      Serial.println(ps2x.Analog(PSS_RX), DEC);
    }
  }

  delay(50);
}
