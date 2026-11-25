/*
 * PS2X_Calibrate — measure and store the stick calibration of your pad.
 *
 * PS2X_lib 2.0, LROB.RU edition (https://lrob.ru), GPLv3.
 * Derived from PS2X_lib 1.9 by Bill Porter, Shutter, Eric Wetzel and
 * Kurt Eckhardt.
 *
 * Why: cheap "wireless" pads and worn sticks often report a centre of
 * 120 or 135 instead of 128 and reach only 6 / 250 instead of 0 / 255.
 * Driving a robot with raw values then gives an off-centre, bent
 * response.  This sketch measures min / max / centre per axis, derives
 * a dead zone, applies it and stores the result in EEPROM/NVS so your
 * other sketches can use the same numbers.
 *
 * How to use:
 *   1. upload, open the serial monitor at 115200;
 *   2. rotate both sticks to every extreme and let them spring back,
 *      a few seconds of wiggling is enough;
 *   3. press 's' to store, 'p' to print the table, 'r' to reset,
 *      'h' to repaint the live view;
 *   4. copy the numbers into your own sketch, or reuse the small
 *      calibration helpers at the top of this file.
 *
 * Serial commands
 *   s  store the measured calibration (EEPROM on AVR, NVS on ESP32)
 *   l  load what is stored
 *   r  reset the measurement to the defaults (0 / 255 / 128)
 *   p  print the calibration table and the scaled axis values
 *   h  help
 */

#include <PS2X_lib.h>
#include <EEPROM.h>
#include <stddef.h>

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

#define EEPROM_SIZE   64
#define CAL_MAGIC     0x5032   /* "P2" */
#define CAL_VERSION   1

/* one axis: measured extremes, derived centre and dead zone */
struct AxisCal {
  uint8_t min;
  uint8_t max;
  uint8_t centre;
  uint8_t dead;
};

struct CalibData {
  uint16_t magic;
  uint8_t  version;
  AxisCal  axis[4];          /* LX, LY, RX, RY */
  uint8_t  checksum;
};

PS2X ps2x;

CalibData cal;
CalibData measured;

/* live extremes seen since the last reset */
uint8_t liveMin[4] = {255, 255, 255, 255};
uint8_t liveMax[4] = {0, 0, 0, 0};

const uint8_t axisIndex[4] = {PSS_LX, PSS_LY, PSS_RX, PSS_RY};
const char *axisName[4] = {"LX", "LY", "RX", "RY"};

uint32_t lastPrint = 0;

/* ------------------------------------------------------------ */
uint8_t checksumOf(const CalibData &d) {
  const uint8_t *p = (const uint8_t *)&d;
  uint8_t sum = 0;
  for (uint8_t i = 0; i < (uint8_t)(offsetof(CalibData, checksum)); i++) {
    sum = (uint8_t)(sum + p[i]);
  }
  return sum;
}

void setDefaults(CalibData &d) {
  d.magic = CAL_MAGIC;
  d.version = CAL_VERSION;
  for (uint8_t i = 0; i < 4; i++) {
    d.axis[i].min = 0;
    d.axis[i].max = 255;
    d.axis[i].centre = 128;
    d.axis[i].dead = 8;
  }
  d.checksum = checksumOf(d);
}

void saveCalib() {
  EEPROM.put(0, cal);
#if defined(ARDUINO_ARCH_ESP32)
  EEPROM.commit();
#endif
  Serial.println(F("calibration stored"));
}

bool loadCalib() {
  CalibData tmp;
  EEPROM.get(0, tmp);
  if (tmp.magic != CAL_MAGIC || tmp.version != CAL_VERSION ||
      tmp.checksum != checksumOf(tmp)) {
    Serial.println(F("no valid calibration in EEPROM"));
    return false;
  }
  cal = tmp;
  Serial.println(F("calibration loaded"));
  return true;
}

/* raw 0..255 -> -1000..+1000 using the stored calibration */
int32_t applyCalib(uint8_t raw, const AxisCal &a) {
  int32_t v = (int32_t)raw - (int32_t)a.centre;
  if (v > -(int32_t)a.dead && v < (int32_t)a.dead) return 0;
  int32_t hi = (int32_t)a.max - (int32_t)a.centre;
  int32_t lo = (int32_t)a.centre - (int32_t)a.min;
  if (v > 0) {
    if (hi < 1) hi = 1;
    return constrain(v * 1000 / hi, 0L, 1000L);
  }
  if (lo < 1) lo = 1;
  return constrain(v * 1000 / lo, -1000L, 0L);
}

void printTable() {
  Serial.println(F("axis  min  max  centre  dead   raw  scaled"));
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t raw = ps2x.Analog(axisIndex[i]);
    Serial.print(axisName[i]);
    Serial.print(F("   "));
    Serial.print(cal.axis[i].min);
    Serial.print(F("   "));
    Serial.print(cal.axis[i].max);
    Serial.print(F("   "));
    Serial.print(cal.axis[i].centre);
    Serial.print(F("     "));
    Serial.print(cal.axis[i].dead);
    Serial.print(F("    "));
    Serial.print(raw);
    Serial.print(F("   "));
    Serial.println(applyCalib(raw, cal.axis[i]));
  }
}

void printHelp() {
  Serial.println(F("s=store  l=load  r=reset  p=print table  h=help"));
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 's': {
        /* derive centre and dead zone from the measured extremes */
        for (uint8_t i = 0; i < 4; i++) {
          uint8_t lo = liveMin[i];
          uint8_t hi = liveMax[i];
          if (hi - lo < 40) {            /* axis never moved: keep old  */
            Serial.print(axisName[i]);
            Serial.println(F(" did not move enough, not stored"));
            continue;
          }
          cal.axis[i].min = lo;
          cal.axis[i].max = hi;
          cal.axis[i].centre = (uint8_t)((lo + hi) / 2);
          cal.axis[i].dead = (uint8_t)constrain((hi - lo) / 25, 4, 20);
        }
        cal.checksum = checksumOf(cal);
        saveCalib();
        printTable();
        break;
      }
      case 'l':
        if (loadCalib()) printTable();
        break;
      case 'r':
        setDefaults(cal);
        for (uint8_t i = 0; i < 4; i++) {
          liveMin[i] = 255;
          liveMax[i] = 0;
        }
        Serial.println(F("measurement reset, wiggle the sticks again"));
        break;
      case 'p':
        printTable();
        break;
      case 'h':
      case '?':
        printHelp();
        break;
      default:
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

#if defined(ARDUINO_ARCH_ESP32)
  EEPROM.begin(EEPROM_SIZE);
#endif

  setDefaults(cal);
  loadCalib();

  uint8_t err = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT);
  Serial.print(F("config_gamepad() -> "));
  Serial.println(PS2X::errorText(err));
  printHelp();
}

void loop() {
  if (!ps2x.read_gamepad()) {
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      Serial.println(F("waiting for a valid frame..."));
    }
    return;
  }

  /* track extremes */
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t v = ps2x.Analog(axisIndex[i]);
    if (v < liveMin[i]) liveMin[i] = v;
    if (v > liveMax[i]) liveMax[i] = v;
  }

  handleSerial();

  if (millis() - lastPrint > 250) {
    lastPrint = millis();
    Serial.print(F("live  "));
    for (uint8_t i = 0; i < 4; i++) {
      Serial.print(axisName[i]);
      Serial.print(F("="));
      Serial.print(ps2x.Analog(axisIndex[i]));
      Serial.print(F("("));
      Serial.print(liveMin[i]);
      Serial.print(F(".."));
      Serial.print(liveMax[i]);
      Serial.print(F(") "));
    }
    Serial.println();
  }
}
