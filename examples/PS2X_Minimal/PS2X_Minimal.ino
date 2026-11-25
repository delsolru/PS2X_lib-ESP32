/*
 * PS2X_Minimal — the smallest possible working sketch.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * Shows the three calls you need: config_gamepad(), read_gamepad()
 * and the button/analog getters.  Also turns the sticks into a
 * -100..+100 "tank drive" mix, which is what most robots need.
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

/* stick is 0..255 in the middle 128 -> -100..+100 */
int stickToPercent(uint8_t v) {
  int p = (int)v - 128;
  if (p > -8 && p < 8) p = 0;          /* dead zone */
  p = p * 100 / 127;
  if (p > 100)  p = 100;
  if (p < -100) p = -100;
  return p;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  uint8_t err = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT);
  Serial.print(F("controller: "));
  Serial.println(PS2X::errorText(err));
}

void loop() {
  if (!ps2x.read_gamepad()) {
    Serial.println(F("no valid frame"));
    delay(200);
    return;
  }

  int left  = stickToPercent(ps2x.Analog(PSS_LY));
  int right = stickToPercent(ps2x.Analog(PSS_RY));

  Serial.print(F("left="));
  Serial.print(left);
  Serial.print(F(" right="));
  Serial.print(right);

  if (ps2x.Button(PSB_START)) Serial.print(F(" START"));
  if (ps2x.ButtonPressed(PSB_CROSS)) Serial.print(F(" X!"));
  if (ps2x.Button(PSB_L1)) Serial.print(F(" L1"));
  if (ps2x.Button(PSB_R1)) Serial.print(F(" R1"));
  Serial.println();

  delay(50);
}
