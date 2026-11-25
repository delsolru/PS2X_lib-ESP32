/*
 * PS2X_Telemetry — stream the pad state to a PC / SD card / logger.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * What it shows:
 *   - a fixed-rate, machine readable output (CSV for spreadsheets and
 *     plotting, JSON for scripts / ROS-style tooling)
 *   - the link quality fields (ok, fails, frame time) so a log also
 *     tells you whether the pad or the wiring was the problem
 *   - a "quiet" mode for battery powered logging
 *
 * CSV columns
 *   ms,buttons,lx,ly,rx,ry,ok,fails,frame_us
 *   buttons is the 16 bit mask of *pressed* buttons (PSB_* constants)
 *
 * Serial commands (type the letter, then Enter)
 *   c  CSV mode (default)      j  JSON mode
 *   q  quiet (no output)       r  resume previous mode
 *   h  header line
 *
 * Example: log to a file on the PC
 *   screen /dev/ttyUSB0 115200 > pad.csv      (Linux/macOS)
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

#define RATE_HZ 50
#define PERIOD_MS (1000 / RATE_HZ)

enum Mode : uint8_t { MODE_CSV, MODE_JSON, MODE_QUIET };

PS2X ps2x;
Mode mode = MODE_CSV;
Mode savedMode = MODE_CSV;
uint32_t lastSample = 0;
uint8_t  errCode = PS2X_ERR_NOT_CONFIGURED;

/* pressed buttons as one 16 bit mask (PS2X keeps the inverted word) */
uint16_t pressedMask() {
  return (uint16_t)((~ps2x.ButtonDataByte()) & 0xFFFF);
}

void emitHeader() {
  if (mode == MODE_QUIET) return;
  if (mode == MODE_CSV) {
    Serial.println(F("ms,buttons,lx,ly,rx,ry,ok,fails,frame_us"));
  } else {
    Serial.println(F("# JSON lines, one object per sample"));
  }
}

void emitSample() {
  if (mode == MODE_QUIET) return;

  uint16_t b   = pressedMask();
  uint8_t  lx  = ps2x.Analog(PSS_LX);
  uint8_t  ly  = ps2x.Analog(PSS_LY);
  uint8_t  rx  = ps2x.Analog(PSS_RX);
  uint8_t  ry  = ps2x.Analog(PSS_RY);
  bool     ok  = ps2x.ok();
  uint16_t f   = ps2x.failedReads();
  uint16_t us  = ps2x.frameMicros();

  if (mode == MODE_CSV) {
    Serial.print(millis());
    Serial.print(',');
    Serial.print(b);
    Serial.print(',');
    Serial.print(lx);
    Serial.print(',');
    Serial.print(ly);
    Serial.print(',');
    Serial.print(rx);
    Serial.print(',');
    Serial.print(ry);
    Serial.print(',');
    Serial.print(ok ? 1 : 0);
    Serial.print(',');
    Serial.print(f);
    Serial.print(',');
    Serial.println(us);
  } else {
    Serial.print(F("{\"ms\":"));
    Serial.print(millis());
    Serial.print(F(",\"b\":"));
    Serial.print(b);
    Serial.print(F(",\"lx\":"));
    Serial.print(lx);
    Serial.print(F(",\"ly\":"));
    Serial.print(ly);
    Serial.print(F(",\"rx\":"));
    Serial.print(rx);
    Serial.print(F(",\"ry\":"));
    Serial.print(ry);
    Serial.print(F(",\"ok\":"));
    Serial.print(ok ? 1 : 0);
    Serial.print(F(",\"f\":"));
    Serial.print(f);
    Serial.print(F(",\"us\":"));
    Serial.print(us);
    Serial.println('}');
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 'c':
        mode = MODE_CSV;
        emitHeader();
        break;
      case 'j':
        mode = MODE_JSON;
        emitHeader();
        break;
      case 'q':
        if (mode != MODE_QUIET) savedMode = mode;
        mode = MODE_QUIET;
        break;
      case 'r':
        mode = savedMode;
        emitHeader();
        break;
      case 'h':
        emitHeader();
        break;
      default:
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  errCode = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT);
  Serial.print(F("# PS2X_lib 2.0 (LROB.RU edition) config_gamepad() -> "));
  Serial.println(PS2X::errorText(errCode));
  Serial.print(F("# controller: "));
  Serial.println(PS2X::typeText(ps2x.readType()));
  Serial.println(F("# commands: c=csv j=json q=quiet r=resume h=header"));
  emitHeader();
}

void loop() {
  handleSerial();

  if (millis() - lastSample < PERIOD_MS) return;
  lastSample = millis();

  bool ok = ps2x.read_gamepad();
  if (ok) {
    errCode = PS2X_OK;
  } else if (errCode == PS2X_OK) {
    errCode = ps2x.lastError();
    if (mode != MODE_QUIET) {
      Serial.print(F("# link lost, err="));
      Serial.println(errCode);
    }
  }

  emitSample();
}
