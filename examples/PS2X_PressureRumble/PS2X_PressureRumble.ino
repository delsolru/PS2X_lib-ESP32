/*
 * PS2X_PressureRumble — pressure sensitive buttons + vibration feedback.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * What it shows:
 *   - config_gamepad(..., pressures = true, rumble = true): the pad then
 *     answers with the full 21 byte frame (mode 0x79), so every button
 *     reports how hard it is pressed (0..255) instead of just on/off
 *   - how to drive the two vibration motors: the small one is on/off,
 *     the big one takes 0..255
 *   - a compact "rumble follows the trigger" feedback loop, which is
 *     what makes a robot feel like it is doing something
 *
 * Note: only genuine DualShock 2 pads support pressure values. If your
 * pad is digital-only, config_gamepad() returns error 3 — the sketch
 * falls back to plain on/off buttons so it stays useful.
 *
 * Wiring
 *   PS/2 receiver          ESP32        Arduino UNO
 *     DAT                  GPIO27         D13
 *     CMD                  GPIO23         D11
 *     ATT                  GPIO19         D10
 *     CLK                  GPIO18         D12
 *     VCC 3.3V             3.3V           3.3V
 *     GND                  GND            GND
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

/* pressure thresholds, 0..255 */
#define R2_SMALL_ON   36    /* small motor starts buzzing            */
#define L2_BIG_MIN    40    /* big motor: below this it will not spin */

PS2X ps2x;
bool pressures = false;
uint32_t lastPrint = 0;

void printState() {
  Serial.print(F("ok="));
  Serial.print(ps2x.ok() ? 1 : 0);
  Serial.print(F(" fails="));
  Serial.print(ps2x.failedReads());
  Serial.print(F(" frame="));
  Serial.print(ps2x.frameMicros());
  Serial.print(F("us |"));

  if (pressures) {
    /* analog (pressure) values, 0..255 */
    Serial.print(F(" L2="));
    Serial.print(ps2x.Analog(PSAB_L2));
    Serial.print(F(" R2="));
    Serial.print(ps2x.Analog(PSAB_R2));
    Serial.print(F(" X="));
    Serial.print(ps2x.Analog(PSAB_CROSS));
    Serial.print(F(" []="));
    Serial.print(ps2x.Analog(PSAB_SQUARE));
    Serial.print(F(" up="));
    Serial.print(ps2x.Analog(PSAB_PAD_UP));
    Serial.print(F(" down="));
    Serial.print(ps2x.Analog(PSAB_PAD_DOWN));
  } else {
    /* digital fallback */
    Serial.print(F(" L2="));
    Serial.print(ps2x.Button(PSB_L2) ? 1 : 0);
    Serial.print(F(" R2="));
    Serial.print(ps2x.Button(PSB_R2) ? 1 : 0);
    Serial.print(F(" X="));
    Serial.print(ps2x.Button(PSB_CROSS) ? 1 : 0);
  }

  Serial.print(F(" | LX="));
  Serial.print(ps2x.Analog(PSS_LX));
  Serial.print(F(" LY="));
  Serial.print(ps2x.Analog(PSS_LY));
  Serial.print(F(" RX="));
  Serial.print(ps2x.Analog(PSS_RX));
  Serial.print(F(" RY="));
  Serial.println(ps2x.Analog(PSS_RY));
}

/* read one frame, addressing the two vibration motors */
void readPad() {
  if (!pressures) {
    ps2x.read_gamepad(false, 0);
    return;
  }

  /* big motor strength follows the L2 trigger, small motor follows R2 */
  uint8_t big   = ps2x.Analog(PSAB_L2);
  if (big < L2_BIG_MIN) big = 0;          /* 0 = motor off           */
  bool    small = ps2x.Analog(PSAB_R2) > R2_SMALL_ON;

  ps2x.read_gamepad(small, big);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  uint8_t err = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT,
                                    true, true);   /* pressures, rumble */
  Serial.print(F("config_gamepad(pressures, rumble) -> "));
  Serial.println(PS2X::errorText(err));

  if (err == PS2X_OK) {
    pressures = true;
    Serial.println(F("pressures enabled: press the buttons gently"));
  } else if (err == PS2X_ERR_NO_PRESSURE) {
    Serial.println(F("this pad has no pressure sensors, falling back to on/off"));
    ps2x.enableRumble();
  } else {
    Serial.println(F("no pad yet — hot-plug will bring it up"));
  }

  Serial.print(F("controller: "));
  Serial.println(PS2X::typeText(ps2x.readType()));
}

void loop() {
  readPad();

  if (!ps2x.ok()) {
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      printState();
    }
    return;
  }

  /* vibration feedback: the pad buzzes when you hit a wall */
  if (pressures) {
    if (ps2x.Analog(PSAB_CROSS) > 60) {
      /* hard X press = short distinct feedback on the small motor */
      ps2x.read_gamepad(true, 0);
      delay(60);
      ps2x.read_gamepad(false, 0);
    }
  }

  if (millis() - lastPrint > 500) {
    lastPrint = millis();
    printState();
  }
}
