/*
 * PS2X_DualControllers — two pads on one MCU.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * What it shows:
 *   - two independent PS2X objects (each pad needs its own 4 wires and
 *     its own receiver; two pads cannot share one PS/2 bus)
 *   - a typical split: pad 1 drives the robot, pad 2 controls the arm /
 *     turret — or a second player
 *   - graceful behaviour when only one of the two pads is plugged in:
 *     the missing one never blocks the other, and it comes back by
 *     itself thanks to the library's hot-plug recovery
 *
 * Timing note: reading two pads takes roughly 1-2 ms, so this sketch
 * polls both at about 200-400 Hz — plenty for any hand control.
 *
 * Wiring (Each receiver: VCC 3.3 V, GND common! DAT is an ESP32 input.)
 *
 *   pad 1  ESP32   UNO          pad 2  ESP32   UNO
 *     DAT  GPIO27   D13           DAT  GPIO26   D4
 *     CMD  GPIO23   D11           CMD  GPIO21   D6
 *     ATT  GPIO19   D10           ATT  GPIO25   D5
 *     CLK  GPIO18   D12           CLK  GPIO22   D7
 */

#include <PS2X_lib.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define P1_DAT 27
  #define P1_CMD 23
  #define P1_SEL 19
  #define P1_CLK 18
  #define P2_DAT 26
  #define P2_CMD 21
  #define P2_SEL 25
  #define P2_CLK 22
#else
  #define P1_DAT 13
  #define P1_CMD 11
  #define P1_SEL 10
  #define P1_CLK 12
  #define P2_DAT 4
  #define P2_CMD 6
  #define P2_SEL 5
  #define P2_CLK 7
#endif

#define DEADZONE 12

PS2X pad1;
PS2X pad2;

bool pad1Ready = false;
bool pad2Ready = false;

uint32_t lastPrint = 0;

int stick(uint8_t raw) {
  int v = (int)raw - 128;
  if (v > -DEADZONE && v < DEADZONE) return 0;
  return constrain(v * 2, -255, 255);
}

/* bring a pad into analog mode; returns true on success */
bool bringUp(PS2X &pad, uint8_t clk, uint8_t cmd, uint8_t att, uint8_t dat,
             const char *name) {
  uint8_t err = pad.config_gamepad(clk, cmd, att, dat, false, false);
  Serial.print(name);
  Serial.print(F(": "));
  Serial.println(PS2X::errorText(err));
  return err == PS2X_OK;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pad1Ready = bringUp(pad1, P1_CLK, P1_CMD, P1_SEL, P1_DAT, "pad 1");
  pad2Ready = bringUp(pad2, P2_CLK, P2_CMD, P2_SEL, P2_DAT, "pad 2");

  if (!pad1Ready || !pad2Ready) {
    Serial.println(F("missing pad(s) will be retried automatically (hot-plug)"));
  }
}

void loop() {
  /* --- read both pads. A failed read must not block the other one --- */
  if (pad1.read_gamepad()) {
    pad1Ready = true;
  } else if (pad1Ready) {
    pad1Ready = false;
    Serial.println(F("pad 1 lost"));
  }

  if (pad2.read_gamepad()) {
    pad2Ready = true;
  } else if (pad2Ready) {
    pad2Ready = false;
    Serial.println(F("pad 2 lost"));
  }

  /* --- pad 1: drive (tank style, no motor code here on purpose) --- */
  if (pad1Ready) {
    int left  = stick(pad1.Analog(PSS_LY));
    int right = stick(pad1.Analog(PSS_RY));

    if (pad1.ButtonPressed(PSB_SELECT)) Serial.println(F("P1: handbrake"));
    if (pad1.Button(PSB_R1))            Serial.println(F("P1: boost"));
    if (pad1.Button(PSB_L1))            Serial.println(F("P1: creep"));

    /* pad 2 (if present) adds 2x fine control while driving */
    if (pad2Ready && pad2.Button(PSB_R2)) {
      left /= 2;
      right /= 2;
    }

    /* send these two numbers to your motor driver */
    if (millis() - lastPrint > 500) {
      Serial.print(F("P1 drive L="));
      Serial.print(left);
      Serial.print(F(" R="));
      Serial.print(right);
    }
  }

  /* --- pad 2: arm / turret commands --- */
  if (pad2Ready) {
    if (pad2.ButtonPressed(PSB_TRIANGLE)) Serial.println(F("P2: arm up step"));
    if (pad2.ButtonPressed(PSB_CROSS))    Serial.println(F("P2: arm down step"));
    if (pad2.ButtonPressed(PSB_SQUARE))   Serial.println(F("P2: gripper toggle"));
    if (pad2.ButtonPressed(PSB_CIRCLE))   Serial.println(F("P2: gripper toggle"));
    if (pad2.NewButtonState(PSB_PAD_UP))   Serial.println(F("P2: turret cw"));
    if (pad2.NewButtonState(PSB_PAD_DOWN)) Serial.println(F("P2: turret ccw"));

    int swing = stick(pad2.Analog(PSS_LX));
    if ((millis() - lastPrint > 500) || swing != 0) {
      Serial.print(F("  P2 swing="));
      Serial.print(swing);
    }
  }

  if (millis() - lastPrint > 500) {
    lastPrint = millis();
    Serial.print(F("  ok[1]="));
    Serial.print(pad1Ready ? 1 : 0);
    Serial.print(F(" ok[2]="));
    Serial.print(pad2Ready ? 1 : 0);
    Serial.println();
  }

  delay(5);
}
