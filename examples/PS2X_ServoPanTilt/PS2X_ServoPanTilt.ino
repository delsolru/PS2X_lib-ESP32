/*
 * PS2X_ServoPanTilt — pan/tilt camera or arm joints driven by a PS/2 pad.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * What it shows:
 *   - the right stick *rate-controls* the pan and tilt servos (push and
 *     the servo keeps moving) — much nicer than absolute control for a
 *     camera gimbal or an arm
 *   - rate depends on stick deflection, so small pushes give fine moves
 *   - limits protect the mechanics, TRIANGLE = go home, SQUARE = slow
 *   - no external library needed on any platform:
 *       ESP32  -> LEDC hardware PWM
 *       AVR    -> the bundled Servo library if you have it, otherwise a
 *                 small Timer1 hardware PWM fallback (always compiles)
 *
 *   PS/2 receiver              ESP32        Arduino UNO / Nano
 *     DAT                      GPIO27         D4
 *     CMD                      GPIO23         D6
 *     ATT                      GPIO19         D5
 *     CLK                      GPIO18         D7
 *     VCC 3.3V                 3.3V           3.3V
 *     GND                      GND            GND
 *   pan servo signal           GPIO26         D9   (OC1A)
 *   tilt servo signal          GPIO25         D10  (OC1B)
 *   servo power                    separate 5-6 V supply, common GND!
 *
 * On Arduino Mega the Timer1 outputs are D11 (OC1A) and D12 (OC1B) —
 * change PAN_PIN / TILT_PIN there. Do not use the pad pins for them.
 */

#include <PS2X_lib.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define PS2_DAT 27
  #define PS2_CMD 23
  #define PS2_SEL 19
  #define PS2_CLK 18
  #define PAN_PIN  26
  #define TILT_PIN 25
#else
  #define PS2_DAT 4
  #define PS2_CMD 6
  #define PS2_SEL 5
  #define PS2_CLK 7
  #define PAN_PIN  9
  #define TILT_PIN 10
#endif

/* ---------------- limits ---------------- */
#define PAN_MIN   10
#define PAN_MAX   170
#define TILT_MIN  40      /* keep the arm away from the base     */
#define TILT_MAX  150
#define PAN_HOME  90
#define TILT_HOME 90

#define DEADZONE   15
#define MAX_STEP   3      /* degrees per control interval        */
#define FAST_STEP  7      /* with R2 held                        */
#define CONTROL_MS 20

/* ============================================================
 *  Servo abstractions: same API (write 0..180) on all platforms.
 * ============================================================ */
#if defined(ARDUINO_ARCH_ESP32)

/* ESP32: 50 Hz LEDC PWM, 16 bit resolution */
class MiniServo {
  public:
    void begin(uint8_t pin, uint8_t channel) {
      _channel = channel;
      pinMode(pin, OUTPUT);
      ledcSetup(_channel, 50, 16);
      ledcAttachPin(pin, _channel);
      write(90);
    }
    void write(int deg) {
      if (deg < 0)   deg = 0;
      if (deg > 180) deg = 180;
      /* 0.5 ms .. 2.5 ms inside a 20 ms period */
      uint32_t us = 500 + (uint32_t)deg * 2000 / 180;
      ledcWrite(_channel, (uint32_t)us * 65536UL / 20000UL);
    }
  private:
    uint8_t _channel = 0;
};

MiniServo panServo, tiltServo;

#else

/* AVR (and anything else): use the Servo library when it is installed */
#if defined(__has_include)
  #if __has_include(<Servo.h>)
    #define PS2X_HAVE_SERVO_LIB 1
  #endif
#endif

#if defined(PS2X_HAVE_SERVO_LIB)

  #include <Servo.h>
  Servo panServo, tiltServo;

  static inline void servoBegin()  { panServo.attach(PAN_PIN); tiltServo.attach(TILT_PIN); }
  static inline void servoWrite(int p, int t) { panServo.write(p); tiltServo.write(t); }

#else

  /*
   * Fallback for boards without the Servo library: Timer1 in fast PWM
   * mode, TOP = ICR1, so both outputs run at exactly 50 Hz.
   *   f = F_CPU / (prescaler * (ICR1 + 1)) = 16 MHz / (8 * 40000) = 50 Hz
   *   1 timer tick = 0.5 us -> pulse = 1000..5000 ticks = 0.5..2.5 ms
   */
  class Timer1Servo {
    public:
      static void init() {
        TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);
        TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);
        ICR1   = 39999;
        pinMode(PAN_PIN, OUTPUT);
        pinMode(TILT_PIN, OUTPUT);
        OCR1A  = 3000;
        OCR1B  = 3000;
      }
      static uint16_t ticks(int deg) {
        if (deg < 0)   deg = 0;
        if (deg > 180) deg = 180;
        return (uint16_t)(1000 + (uint32_t)deg * 2000UL / 180UL);
      }
  };

  static inline void servoBegin() { Timer1Servo::init(); }
  static inline void servoWrite(int p, int t) {
    OCR1A = Timer1Servo::ticks(p);
    OCR1B = Timer1Servo::ticks(t);
  }

#endif
#endif

PS2X ps2x;

int pan  = PAN_HOME;
int tilt = TILT_HOME;
int shownPan = -1, shownTilt = -1;
uint32_t lastMove = 0;
uint32_t lastPrint = 0;
bool slowMode = false;

/* stick -128..127 with dead zone, scaled to -100..100 */
int axisRate(uint8_t raw) {
  int v = (int)raw - 128;
  if (v > -DEADZONE && v < DEADZONE) return 0;
  return constrain(v * 100 / 127, -100, 100);
}

void writeServos() {
  if (pan == shownPan && tilt == shownTilt) return;
  shownPan = pan;
  shownTilt = tilt;
#if defined(ARDUINO_ARCH_ESP32)
  panServo.write(pan);
  tiltServo.write(tilt);
#else
  servoWrite(pan, tilt);
#endif
}

void report() {
  Serial.print(F("pan="));
  Serial.print(shownPan);
  Serial.print(F(" tilt="));
  Serial.print(shownTilt);
  Serial.print(F(" ok="));
  Serial.print(ps2x.ok() ? 1 : 0);
  Serial.print(F(" fails="));
  Serial.print(ps2x.failedReads());
  Serial.print(F(" frame="));
  Serial.print(ps2x.frameMicros());
  Serial.print(F("us"));
  if (slowMode) Serial.print(F(" [slow]"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(300);

#if defined(ARDUINO_ARCH_ESP32)
  panServo.begin(PAN_PIN, 0);
  tiltServo.begin(TILT_PIN, 1);
#else
  servoBegin();
#endif
  writeServos();

  uint8_t err = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT);
  Serial.print(F("config_gamepad() -> "));
  Serial.println(PS2X::errorText(err));
  Serial.println(F("Right stick = pan/tilt rate, TRIANGLE = home, SQUARE = slow"));
  report();
}

void loop() {
  /* keep working with the last known position if the pad is missing */
  if (!ps2x.read_gamepad()) {
    writeServos();
    if (millis() - lastPrint > 2000) {
      lastPrint = millis();
      Serial.println(F("no pad -> servos hold position"));
    }
    return;
  }

  if (ps2x.ButtonPressed(PSB_TRIANGLE)) {
    pan  = PAN_HOME;
    tilt = TILT_HOME;
    Serial.println(F("home"));
  }
  if (ps2x.ButtonPressed(PSB_SQUARE)) {
    slowMode = !slowMode;
    if (slowMode) Serial.println(F("slow mode on"));
    else          Serial.println(F("slow mode off"));
  }

  if (millis() - lastMove >= CONTROL_MS) {
    lastMove = millis();

    int step = slowMode ? 1 : (ps2x.Button(PSB_R2) ? FAST_STEP : MAX_STEP);
    int dx = axisRate(ps2x.Analog(PSS_RX));
    int dy = axisRate(ps2x.Analog(PSS_RY));

    pan  += (dx * step) / 100;
    tilt -= (dy * step) / 100;

    pan  = constrain(pan, PAN_MIN, PAN_MAX);
    tilt = constrain(tilt, TILT_MIN, TILT_MAX);
    writeServos();
  }

  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    report();
  }
}
