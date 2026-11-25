/*
 * PS2X_TankDrive — drive two DC motors (L298N / L293D / TB6612 style
 * "IN1 + IN2 + EN(PWM)" drivers) from a PS/2 pad.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * What it shows:
 *   - tank mixing (left stick Y = left motor, right stick Y = right motor)
 *   - dead zone, per-motor invert, speed limit
 *   - slew limiting: the motors ramp up/down instead of slamming, which
 *     saves gearboxes and keeps the current below the driver's limit
 *   - emergency stop on SELECT, resume on START
 *   - L2/R2 as slow/fast trigger on top of the sticks
 *
 * Wiring
 *   PS/2 receiver          ESP32       Arduino UNO
 *     DAT                  GPIO27        D13
 *     CMD                  GPIO23        D11
 *     ATT                  GPIO19        D10
 *     CLK                  GPIO18        D12
 *     VCC (3.3 V!)         3.3V          3.3V
 *     GND                  GND           GND
 *
 *   L298N left motor       ESP32       UNO      L298N right motor   ESP32   UNO
 *     ENA (PWM)            GPIO25       D9         ENB (PWM)        GPIO33   D6
 *     IN1                  GPIO26       D8         IN3              GPIO21   D5
 *     IN2                  GPIO32       D7         IN4              GPIO22   D4
 *     power / GND             -- external supply, common GND with the MCU --
 *
 * ESP32 pins avoid 6..11 (flash), 34..39 (input only) and the strapping
 * pins 0/2/12/15; the pad pins (18/19/23/27) stay free.
 */

#include <PS2X_lib.h>

/* ---------------- PS/2 pins ---------------- */
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

/* ---------------- motor driver pins ---------------- */
#if defined(ARDUINO_ARCH_ESP32)
  #define ENA 25
  #define IN1 26
  #define IN2 32
  #define ENB 33
  #define IN3 21
  #define IN4 22
#else
  #define ENA 9
  #define IN1 8
  #define IN2 7
  #define ENB 6
  #define IN3 5
  #define IN4 4
#endif

/* ---------------- tuning ---------------- */
#define DEADZONE      12     /* stick counts ignored around the centre   */
#define MAX_SPEED     220    /* 0..255 limit, keeps the driver cool      */
#define RAMP_STEP     18     /* max change of one update() call          */
#define INVERT_LEFT   0      /* flip if a motor turns the wrong way      */
#define INVERT_RIGHT  0
#define UPDATE_MS     10

PS2X ps2x;
uint8_t errCode = PS2X_ERR_NOT_CONFIGURED;

/* ============================================================
 *  Very small motor abstraction:
 *  one PWM pin + two direction pins, -255..255 input, slew limited.
 * ============================================================ */
class Motor {
  public:
    void begin(uint8_t en, uint8_t in1, uint8_t in2, bool invert = false) {
      _en = en; _in1 = in1; _in2 = in2; _inv = invert; 
      pinMode(_en, OUTPUT);
      pinMode(_in1, OUTPUT);
      pinMode(_in2, OUTPUT);
      _cur = 0;
      _target = 0;
      apply(0);
#if defined(ARDUINO_ARCH_ESP32)
      ledcSetup(_ch, 20000, 8);        /* 20 kHz, above hearing */
      ledcAttachPin(_en, _ch);
#else
      digitalWrite(_en, LOW);
#endif
    }

    void setChannel(uint8_t ch) { _ch = ch; }

    void set(int speed) { _target = constrain(speed, -255, 255); }

    void stopNow() { _target = 0; _cur = 0; apply(0); }

    void update() {
      if (_cur < _target) {
        _cur += RAMP_STEP;
        if (_cur > _target) _cur = _target;
      } else if (_cur > _target) {
        _cur -= RAMP_STEP;
        if (_cur < _target) _cur = _target;
      }
      apply(_cur);
    }

    int speed() const { return _cur; }

  private:
    void apply(int s) {
      if (_inv) s = -s;
      if (s > 0) {
        digitalWrite(_in1, HIGH);
        digitalWrite(_in2, LOW);
      } else if (s < 0) {
        digitalWrite(_in1, LOW);
        digitalWrite(_in2, HIGH);
        s = -s;
      } else {
        digitalWrite(_in1, LOW);
        digitalWrite(_in2, LOW);
        s = 0;
      }
      pwmWrite((uint8_t)s);
    }

    void pwmWrite(uint8_t duty) {
#if defined(ARDUINO_ARCH_ESP32)
      ledcWrite(_ch, duty);
#else
      analogWrite(_en, duty);
#endif
    }

    uint8_t _en = 0, _in1 = 0, _in2 = 0, _ch = 0;
    bool    _inv = false;
    int     _cur = 0, _target = 0;
};

Motor leftMotor, rightMotor;

uint32_t lastUpdate = 0;
uint32_t lastStatus = 0;
bool     eStop = false;

/* raw 0..255 -> -255..255 with a dead zone */
int stick(uint8_t raw) {
  int v = (int)raw - 128;
  if (v > -DEADZONE && v < DEADZONE) return 0;
  v = v * 2;
  return constrain(v, -255, 255);
}

void printState() {
  Serial.print(F("["));
  Serial.print(millis() / 1000);
  Serial.print(F("s] err="));
  Serial.print(errCode);
  Serial.print(F(" ok="));
  Serial.print(ps2x.ok() ? 1 : 0);
  Serial.print(F(" fails="));
  Serial.print(ps2x.failedReads());
  Serial.print(F(" frame="));
  Serial.print(ps2x.frameMicros());
  Serial.print(F("us L="));
  Serial.print(leftMotor.speed());
  Serial.print(F(" R="));
  Serial.print(rightMotor.speed());
  if (eStop) Serial.println(F("  [STOP]"));
  else       Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  lastUpdate = millis();

  rightMotor.setChannel(1);
  leftMotor.begin(ENA, IN1, IN2, INVERT_LEFT);
  rightMotor.begin(ENB, IN3, IN4, INVERT_RIGHT);

  errCode = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
  Serial.print(F("config_gamepad() -> "));
  Serial.println(PS2X::errorText(errCode));

  if (errCode != PS2X_OK) {
    Serial.println(F("Drive is locked until a controller answers (hot-plug works)."));
  }
  printState();
}

void loop() {
  /* never drive without a live pad */
  if (!ps2x.read_gamepad(false, 0)) {
    if (errCode == PS2X_OK) {
      errCode = ps2x.lastError();
      Serial.println(F("link lost -> motors stopped"));
    }
    eStop = true;
  } else {
    errCode = PS2X_OK;
  }

  if (ps2x.ButtonPressed(PSB_SELECT)) {
    eStop = true;
    Serial.println(F("emergency stop"));
  }
  if (ps2x.ButtonPressed(PSB_START)) {
    eStop = false;
    Serial.println(F("drive enabled"));
  }

  if (eStop) {
    leftMotor.stopNow();
    rightMotor.stopNow();
  } else {
    /* L2 = slow trigger (50 %), R2 = 100 %, nothing = 75 % cruise */
    int scale = 75;
    if (ps2x.Button(PSB_L2)) scale = 45;
    if (ps2x.Button(PSB_R2)) scale = 100;

    int l = stick(ps2x.Analog(PSS_LY)) * MAX_SPEED / 255;
    int r = stick(ps2x.Analog(PSS_RY)) * MAX_SPEED / 255;
    l = l * scale / 100;
    r = r * scale / 100;

    leftMotor.set(l);
    rightMotor.set(r);
  }

  if (millis() - lastUpdate >= UPDATE_MS) {
    lastUpdate = millis();
    leftMotor.update();
    rightMotor.update();
  }

  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    printState();
  }
}
