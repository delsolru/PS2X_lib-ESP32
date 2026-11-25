/******************************************************************
 *  PS2X_lib 2.0 — PlayStation 2 controller library
 *  LROB.RU edition                                   https://lrob.ru
 *
 *  DERIVATIVE WORK: based on the original "PS2X_lib" 1.9 by Shutter,
 *  Bill Porter (www.billporter.info), Eric Wetzel and Kurt Eckhardt
 *      https://github.com/madsci1016/Arduino-PS2X/tree/master/PS2X_lib
 *  Protocol, commands and API come from that original library; this
 *  file is the portable (ESP32-capable) re-implementation of it.
 *
 *  Copyright (C) 2009-2013  Shutter, Bill Porter, Eric Wetzel,
 *                           Kurt Eckhardt  — original PS2X_lib 1.x
 *  Copyright (C) 2026       LROB.RU (https://lrob.ru) — ESP32 port
 *
 *  Licence: GNU GPL v3 or later (see LICENSE), same as the original.
 *  See PS2X_lib.h for the full header and README.ru.md / README.md
 *  for the list of changes.
 ******************************************************************/

#include "PS2X_lib.h"

#include <string.h>

/* ------------------------------------------------------------------
 *  Architecture specific includes
 * ------------------------------------------------------------------ */
#if defined(PS2X_HW_REG)
  /* ESP32 family: raw GPIO registers (identical layout on
   * ESP32 / S2 / S3 / C3 / C6). */
  #include "soc/gpio_reg.h"

  /* Cycle counter for microsecond accurate delays.  Arduino-ESP32
   * based on IDF >= 4.3 provides esp_cpu.h; fall back to Arduino's
   * delayMicroseconds() otherwise. */
  #if defined(__has_include)
    #if __has_include("esp_cpu.h")
      #include "esp_cpu.h"
      #define PS2X_HAVE_CCOUNT 1
    #endif
  #endif

  #ifdef PS2X_HAVE_CCOUNT
    #if defined(__has_include)
      #if __has_include("esp_idf_version.h")
        #include "esp_idf_version.h"
      #endif
    #endif
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
      #define PS2X_CCOUNT() esp_cpu_get_cycle_count()
    #else
      #define PS2X_CCOUNT() esp_cpu_get_ccount()
    #endif
  #endif
#endif

/* ------------------------------------------------------------------
 *  Controller commands
 * ------------------------------------------------------------------ */
static const uint8_t enter_config[]    = {0x01, 0x43, 0x00, 0x01, 0x00};
static const uint8_t set_mode[]        = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
static const uint8_t set_bytes_large[] = {0x01, 0x4F, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
static const uint8_t exit_config[]     = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
static const uint8_t enable_rumble[]   = {0x01, 0x4D, 0x00, 0x00, 0x01};
static const uint8_t type_read[]       = {0x01, 0x45, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};

/* Answer of a controller that is in "digital" mode (never configured
 * into analog mode).  0x41 = no data yet, 0x73/0x79 = analog. */
#define PS2X_MODE_DIGITAL 0x41

/* ==================================================================
 *  Pin validity helpers
 *
 *  A wrong pin is the #1 reason for "no controller found", so the
 *  library refuses obviously unusable pins and reports PS2X_ERR_BAD_PIN
 *  instead of silently failing.  Define PS2X_SKIP_PIN_CHECK to disable.
 * ================================================================== */
#if !defined(PS2X_SKIP_PIN_CHECK)

static bool ps2x_pin_is_flash(uint8_t pin) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  /* classic ESP32: GPIO6..11 are wired to the SPI flash */
  return (pin >= 6 && pin <= 11);
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  /* flash (and often octal PSRAM) */
  return (pin >= 26 && pin <= 37);
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
      defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32H2)
  return (pin >= 11 && pin <= 17);
#else
  (void)pin;
  return false;
#endif
}

static bool ps2x_pin_is_input_only(uint8_t pin) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  return (pin >= 34 && pin <= 39);   /* GPIO34..39 are input only */
#else
  (void)pin;
  return false;
#endif
}

/* Highest usable GPIO number of the running ESP32 variant. */
#if defined(ARDUINO_ARCH_ESP32)
static uint8_t ps2x_max_pin(void) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  return 39;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  return 46;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  return 48;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return 21;
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
  return 20;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
  return 30;
#else
  return 48;
#endif
}
#endif /* ARDUINO_ARCH_ESP32 */

static bool ps2x_pin_valid(uint8_t pin, bool output) {
#if defined(PS2X_HW_AVR)
  if (digitalPinToPort(pin) == NOT_A_PORT) return false;
#endif
#if defined(ARDUINO_ARCH_ESP32)
  if (pin > ps2x_max_pin()) return false;
#endif
  if (ps2x_pin_is_flash(pin)) return false;
  if (output && ps2x_pin_is_input_only(pin)) return false;
  return true;
}

#else

static bool ps2x_pin_valid(uint8_t pin, bool output) {
  (void)pin; (void)output;
  return true;
}

#endif /* PS2X_SKIP_PIN_CHECK */

#if defined(PS2X_HW_REG)
/* Translate an Arduino pin number into the GPIO register bank it lives
 * in.  ESP32 / S2 / S3 have two banks (pins 0..31 and 32..63), while
 * ESP32-C3/C2/H2 only have the first one. */
static bool ps2x_map_gpio(uint8_t pin, PS2X_HwPin &p) {
  if (pin < 32) {
    p.mask = (uint32_t)1 << pin;
    p.set  = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
    p.clr  = (volatile uint32_t *)GPIO_OUT_W1TC_REG;
    p.in   = (volatile uint32_t *)GPIO_IN_REG;
    return true;
  }
#if defined(GPIO_OUT1_W1TS_REG)
  if (pin < 64) {
    p.mask = (uint32_t)1 << (pin - 32);
    p.set  = (volatile uint32_t *)GPIO_OUT1_W1TS_REG;
    p.clr  = (volatile uint32_t *)GPIO_OUT1_W1TC_REG;
    p.in   = (volatile uint32_t *)GPIO_IN1_REG;
    return true;
  }
#endif
  return false;   /* no such GPIO on this chip */
}
#endif

/* ==================================================================
 *  Constructor
 * ================================================================== */
PS2X::PS2X() {
  memset(_data, 0, sizeof(_data));
  memset(_pins, 0, sizeof(_pins));
  _pinsValid      = false;
  _buttons        = 0;
  _lastButtons    = 0;
  _controllerType = 0;
  _haveFrame      = false;
  _error          = PS2X_ERR_NOT_CONFIGURED;
  _ok             = false;
  _failCount      = 0;
  _minGapMs       = PS2X_MIN_READ_DELAY;
  _enRumble       = false;
  _enPressures    = false;
  _wantRumble     = false;
  _wantPressures  = false;
  _inConfig       = false;
  _autoReconnect  = (PS2X_RECONNECT_MS > 0);
  _lastReadMs     = 0;
  _lastOkMs       = 0;
  _lastCfgAttemptMs = 0;
  _clkLowUs       = PS2X_DEFAULT_CLK_LOW_US;
  _clkHighUs      = PS2X_DEFAULT_CLK_HIGH_US;
  _byteGapUs      = PS2X_DEFAULT_BYTE_GAP_US;
  _cyclesPerUs    = 0;
  _frameUs        = 0;
}

/* ==================================================================
 *  Microsecond delay.
 *
 *  On ESP32 the Arduino delayMicroseconds() polls the 64 bit system
 *  timer (a function call per iteration), which is both slow and very
 *  imprecise for the 1..5 us windows used by the PS/2 bus.  Instead we
 *  spin on the free running CPU cycle counter, which is a single
 *  register read, giving an exact and CPU-frequency independent delay.
 * ================================================================== */
void PS2X::_delayUs(uint16_t us) {
  if (us == 0) return;
#if defined(PS2X_HAVE_CCOUNT)
  uint32_t ticks = (uint32_t)us * _cyclesPerUs;
  if (_cyclesPerUs == 0) {          /* not initialised yet */
    delayMicroseconds(us);
    return;
  }
  uint32_t start = PS2X_CCOUNT();
  while ((uint32_t)(PS2X_CCOUNT() - start) < ticks) {
    /* spin */
  }
#else
  delayMicroseconds(us);
#endif
}

/* ==================================================================
 *  Bus primitives
 * ================================================================== */

/* Clock out one byte (LSB first) and clock in the answer, exactly like
 * the original library — including the ACK slot after the 8th clock. */
uint8_t PS2X::_shiftInOut(uint8_t out) {
  uint8_t in = 0;

  for (uint8_t i = 0; i < 8; i++) {
    if (out & (uint8_t)(1u << i)) _cmdHigh();
    else                          _cmdLow();

    _clkLow();
    _delayUs(_clkLowUs);              /* data is valid while clock is low */
    if (_datRead()) in |= (uint8_t)(1u << i);
    _clkHigh();
    _delayUs(_clkHighUs);
  }

  _cmdHigh();
  _delayUs(_byteGapUs);               /* ACK slot / inter byte gap */

  return in;
}

/* Send a complete command packet to the controller. */
void PS2X::_sendCommand(const uint8_t *cmd, uint8_t len) {
  _attLow();
  _delayUs(_byteGapUs);

#if PS2X_COM_DEBUG
  uint8_t answer[16];
  if (len > sizeof(answer)) len = sizeof(answer);
  for (uint8_t i = 0; i < len; i++) answer[i] = _shiftInOut(cmd[i]);
  _attHigh();
  delay(_minGapMs);

  Serial.print(F("PS2X OUT:IN "));
  for (uint8_t i = 0; i < len; i++) {
    Serial.print(cmd[i], HEX);
    Serial.print(F(":"));
    Serial.print(answer[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
#else
  for (uint8_t i = 0; i < len; i++) _shiftInOut(cmd[i]);
  _attHigh();
  delay(_minGapMs);
#endif
}

/* ==================================================================
 *  Configuration
 * ================================================================== */
bool PS2X::_configurePins(uint8_t clk, uint8_t cmd, uint8_t att, uint8_t dat) {
  if (!ps2x_pin_valid(clk, true) || !ps2x_pin_valid(cmd, true) ||
      !ps2x_pin_valid(att, true) || !ps2x_pin_valid(dat, false)) {
#if PS2X_DEBUG
    Serial.print(F("PS2X: invalid pin assignment ("));
    Serial.print(clk); Serial.print(',');
    Serial.print(cmd); Serial.print(',');
    Serial.print(att); Serial.print(',');
    Serial.print(dat); Serial.println(')');
#endif
    return false;
  }

  _pins[0] = clk;
  _pins[1] = cmd;
  _pins[2] = att;
  _pins[3] = dat;

#if defined(PS2X_HW_AVR)
  pinMode(clk, OUTPUT);
  pinMode(cmd, OUTPUT);
  pinMode(att, OUTPUT);
  pinMode(dat, INPUT_PULLUP);

  _clk.out  = portOutputRegister(digitalPinToPort(clk));
  _clk.in   = portInputRegister(digitalPinToPort(clk));
  _clk.mask = digitalPinToBitMask(clk);

  _cmd.out  = portOutputRegister(digitalPinToPort(cmd));
  _cmd.in   = portInputRegister(digitalPinToPort(cmd));
  _cmd.mask = digitalPinToBitMask(cmd);

  _att.out  = portOutputRegister(digitalPinToPort(att));
  _att.in   = portInputRegister(digitalPinToPort(att));
  _att.mask = digitalPinToBitMask(att);

  _dat.out  = portOutputRegister(digitalPinToPort(dat));
  _dat.in   = portInputRegister(digitalPinToPort(dat));
  _dat.mask = digitalPinToBitMask(dat);

#elif defined(PS2X_HW_REG)
  /* resolve all four pins first, so a bad pin cannot leave the bus half
   * configured */
  if (!ps2x_map_gpio(clk, _clk) || !ps2x_map_gpio(cmd, _cmd) ||
      !ps2x_map_gpio(att, _att) || !ps2x_map_gpio(dat, _dat)) {
    return false;
  }

  pinMode(clk, OUTPUT);
  pinMode(cmd, OUTPUT);
  pinMode(att, OUTPUT);
  pinMode(dat, INPUT_PULLUP);   /* the controller only pulls DAT low */

  _cyclesPerUs = getCpuFrequencyMhz();

#else
  pinMode(clk, OUTPUT);
  pinMode(cmd, OUTPUT);
  pinMode(att, OUTPUT);
  pinMode(dat, INPUT_PULLUP);

  _clk.pin = clk;
  _cmd.pin = cmd;
  _att.pin = att;
  _dat.pin = dat;
#endif

  /* idle bus state */
  _clkHigh();
  _cmdHigh();
  _attHigh();

  _pinsValid      = true;
  _haveFrame      = false;
  _lastReadMs     = millis();
  _lastOkMs       = millis();
  _lastCfgAttemptMs = 0;
  return true;
}

uint8_t PS2X::config_gamepad(uint8_t clk, uint8_t cmd, uint8_t att, uint8_t dat,
                             bool pressures, bool rumble) {
  _wantPressures = pressures;
  _wantRumble    = rumble;

  if (!_configurePins(clk, cmd, att, dat)) {
    _error     = PS2X_ERR_BAD_PIN;
    _pinsValid = false;
    _ok        = false;
    return _error;
  }
  return _begin();
}

/* The whole "get the controller into analog mode" state machine.
 * Same sequence as the original library, with the delays clamped so
 * that a missing controller can never block for long. */
uint8_t PS2X::_begin() {
  uint8_t temp[sizeof(type_read)];
  uint8_t result = PS2X_ERR_NOT_ACCEPTING;

  _inConfig  = true;
  _haveFrame = false;
  _minGapMs  = PS2X_MIN_READ_DELAY;

  _cmdHigh();
  _clkHigh();
  _attHigh();
  _delayUs(_byteGapUs);

  /* "is anybody there?": read the controller twice, a controller that is
   * talking answers with 0x41 / 0x42 / 0x73 / 0x79 in byte 1. */
  read_gamepad();
  read_gamepad();

  if (_data[1] != 0x41 && _data[1] != 0x42 &&
      _data[1] != 0x73 && _data[1] != 0x79) {
#if PS2X_DEBUG
    Serial.print(F("PS2X: no controller, got mode 0x"));
    Serial.println(_data[1], HEX);
#endif
    _error            = PS2X_ERR_NO_CONTROLLER;
    _ok               = false;
    _lastCfgAttemptMs = millis();
    _inConfig         = false;
    return _error;
  }

  /* Try to switch the controller into analog mode.  Some (mostly
   * wireless) receivers need a little more pause between frames, so the
   * delay is increased on every failed attempt — same as before. */
  for (uint8_t attempt = 0; attempt <= 10; attempt++) {
    _sendCommand(enter_config, sizeof(enter_config));

    /* read the controller type */
    _delayUs(_byteGapUs);
    _cmdHigh();
    _clkHigh();
    _attLow();
    _delayUs(_byteGapUs);
    for (uint8_t i = 0; i < sizeof(type_read); i++) {
      temp[i] = _shiftInOut(type_read[i]);
    }
    _attHigh();

    _controllerType = temp[3];

    _sendCommand(set_mode, sizeof(set_mode));
    if (_wantRumble) {
      _sendCommand(enable_rumble, sizeof(enable_rumble));
      _enRumble = true;
    }
    if (_wantPressures) {
      _sendCommand(set_bytes_large, sizeof(set_bytes_large));
      _enPressures = true;
    }
    _sendCommand(exit_config, sizeof(exit_config));

    read_gamepad();

    if (_wantPressures) {
      if (_data[1] == 0x79) { result = PS2X_OK; break; }
      if (_data[1] == 0x73) { result = PS2X_ERR_NO_PRESSURE; break; }
    }
    if (_data[1] == 0x73) { result = PS2X_OK; break; }

    if (attempt == 10) break;

    if (_minGapMs < PS2X_MAX_READ_DELAY) _minGapMs++;
  }

#if PS2X_DEBUG
  if (result != PS2X_OK) {
    Serial.print(F("PS2X: configuration failed, error "));
    Serial.print(result);
    Serial.print(F(", mode 0x"));
    Serial.println(_data[1], HEX);
  }
#endif

  _error            = result;
  _ok               = (result == PS2X_OK);
  _lastCfgAttemptMs = millis();
  if (_ok) _lastOkMs = millis();
  _inConfig = false;

  return result;
}

/* ==================================================================
 *  Reading
 * ================================================================== */
bool PS2X::read_gamepad() {
  return read_gamepad(false, 0x00);
}

bool PS2X::read_gamepad(bool motor1, uint8_t motor2) {
  if (!_pinsValid) {
    _error = PS2X_ERR_NOT_CONFIGURED;
    _ok    = false;
    return false;
  }

#if defined(PS2X_HAVE_CCOUNT)
  /* keep the delay calibration in sync with the CPU clock (the sketch
   * may change it with setCpuFrequencyMhz()) */
  _cyclesPerUs = getCpuFrequencyMhz();
#endif

  uint32_t now = millis();

  /* Not polled for a long time: the controller falls back to digital
   * mode, so re-send the configuration (original behaviour). */
  if (!_inConfig && (now - _lastReadMs) > 1500) {
    reconfig_gamepad();
  }

  /* keep the requested frame rate */
  uint32_t gap = now - _lastReadMs;
  if (gap < _minGapMs) delay(_minGapMs - gap);

  if (motor2 != 0x00) {
    motor2 = (uint8_t)map(motor2, 0, 255, 0x40, 0xFF);  /* <0x40 won't spin */
  }

  uint8_t dword[9]   = {0x01, 0x42, 0x00, (uint8_t)(motor1 ? 1 : 0), motor2,
                        0x00, 0x00, 0x00, 0x00};
  uint8_t zeros[12]  = {0};

  uint32_t t0 = micros();
  bool ok = false;

  for (uint8_t attempt = 0; attempt < 5; attempt++) {
    _cmdHigh();
    _clkHigh();
    _attLow();
    _delayUs(_byteGapUs);

    for (uint8_t i = 0; i < 9; i++) {
      _data[i] = _shiftInOut(dword[i]);
    }

    /* 0x79 = full data block (pressures / rumble enabled) */
    if (_data[1] == 0x79) {
      for (uint8_t i = 0; i < 12; i++) {
        _data[i + 9] = _shiftInOut(zeros[i]);
      }
    }

    _attHigh();

    /* analog mode answers are 0x7x — anything else is not usable */
    if ((_data[1] & 0xF0) == 0x70) {
      ok = true;
      break;
    }

    /* we are not in analog mode: try to get back */
    reconfig_gamepad();
    delay(_minGapMs);
  }

  _frameUs = (uint16_t)(micros() - t0);

  /* Still nothing after 5 attempts: some receivers just need a slower
   * bus, so raise the frame delay step by step. */
  if (!ok && _minGapMs < PS2X_MAX_READ_DELAY) _minGapMs++;

#if PS2X_COM_DEBUG
  Serial.print(F("PS2X OUT:IN "));
  for (uint8_t i = 0; i < 9; i++) {
    Serial.print(dword[i], HEX);
    Serial.print(F(":"));
    Serial.print(_data[i], HEX);
    Serial.print(F(" "));
  }
  for (uint8_t i = 0; i < 12; i++) {
    Serial.print(zeros[i], HEX);
    Serial.print(F(":"));
    Serial.print(_data[i + 9], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
#endif

  if (ok) {
    uint16_t newButtons = (uint16_t)(((uint16_t)_data[4] << 8) | _data[3]);

    if (!_haveFrame) {
      /* first good frame: do not report every held button as "pressed" */
      _lastButtons = newButtons;
      _haveFrame   = true;
    } else {
      _lastButtons = _buttons;
    }
    _buttons   = newButtons;
    _failCount = 0;
    _lastOkMs  = millis();
    _error     = PS2X_OK;
  } else {
    if (_failCount < 0xFFFF) _failCount++;
    _error = PS2X_ERR_NO_CONTROLLER;
  }

  _ok         = ok;
  _lastReadMs = millis();

  /* ---------------- hot-plug / recovery ---------------- */
#if (PS2X_RECONNECT_MS > 0)
  if (!ok && _autoReconnect && !_inConfig) {
    uint32_t t = millis();
    if ((t - _lastOkMs) >= PS2X_RECONNECT_MS &&
        (t - _lastCfgAttemptMs) >= PS2X_RECONNECT_MS) {
      _lastCfgAttemptMs = t;
      _begin();      /* full initialisation, restores analog mode */
    }
  }
#endif

  return ok;
}

/* ==================================================================
 *  Button helpers
 * ================================================================== */
bool PS2X::NewButtonState() {
  return ((_lastButtons ^ _buttons) != 0);
}

bool PS2X::NewButtonState(unsigned int button) {
  return (((_lastButtons ^ _buttons) & (uint16_t)button) != 0);
}

bool PS2X::ButtonPressed(unsigned int button) {
  return NewButtonState(button) && Button((uint16_t)button);
}

bool PS2X::ButtonReleased(unsigned int button) {
  return NewButtonState(button) && ((_buttons & (uint16_t)button) == 0);
}

bool PS2X::Button(uint16_t button) {
  return ((~_buttons & button) != 0);
}

unsigned int PS2X::ButtonDataByte() {
  return (unsigned int)(~_buttons);
}

uint8_t PS2X::Analog(uint8_t button) {
  if (button >= sizeof(_data)) return 0;
  return _data[button];
}

/* ==================================================================
 *  Controller type / reconfiguration
 * ================================================================== */
uint8_t PS2X::readType() {
  if (_controllerType == 0x03) return 1;                       /* DualShock (pressure)     */
  if (_controllerType == 0x01 && _data[1] == 0x42) return 4;   /* dual shock digital mode  */
  if (_controllerType == 0x01 && _data[1] != 0x42) return 2;   /* Guitar Hero              */
  if (_controllerType == 0x0C) return 3;                       /* 2.4 GHz wireless DS      */
  return 0;
}

void PS2X::enableRumble() {
  if (!_pinsValid) return;
  _sendCommand(enter_config, sizeof(enter_config));
  _sendCommand(enable_rumble, sizeof(enable_rumble));
  _sendCommand(exit_config, sizeof(exit_config));
  _wantRumble = true;
  _enRumble   = true;
}

bool PS2X::enablePressures() {
  if (!_pinsValid) return false;
  _sendCommand(enter_config, sizeof(enter_config));
  _sendCommand(set_bytes_large, sizeof(set_bytes_large));
  _sendCommand(exit_config, sizeof(exit_config));

  read_gamepad();
  read_gamepad();

  if (_data[1] != 0x79) return false;

  _wantPressures = true;
  _enPressures   = true;
  return true;
}

void PS2X::reconfig_gamepad() {
  if (!_pinsValid) return;
  _sendCommand(enter_config, sizeof(enter_config));
  _sendCommand(set_mode, sizeof(set_mode));
  if (_enRumble || _wantRumble)    _sendCommand(enable_rumble, sizeof(enable_rumble));
  if (_enPressures || _wantPressures) _sendCommand(set_bytes_large, sizeof(set_bytes_large));
  _sendCommand(exit_config, sizeof(exit_config));
}

/* ==================================================================
 *  Diagnostic API
 * ================================================================== */
void PS2X::setTiming(uint8_t clkLowUs, uint8_t clkHighUs, uint8_t byteGapUs) {
  _clkLowUs  = clkLowUs  ? clkLowUs  : 1;
  _clkHighUs = clkHighUs ? clkHighUs : 1;
  _byteGapUs = byteGapUs ? byteGapUs : 1;
}

void PS2X::setReadDelay(uint8_t ms) {
  if (ms < PS2X_MIN_READ_DELAY) ms = PS2X_MIN_READ_DELAY;
  if (ms > PS2X_MAX_READ_DELAY) ms = PS2X_MAX_READ_DELAY;
  _minGapMs = ms;
}

uint32_t PS2X::lastReadAge() const {
  return millis() - _lastReadMs;
}

uint32_t PS2X::lastOkAge() const {
  return millis() - _lastOkMs;
}

void PS2X::debugPrint(Print &out) {
  out.print(F(PS2X_LIB_NAME));
  out.print(' ');
  out.print(F(PS2X_LIB_VERSION));
  out.print(F(" | clk="));  out.print(_pins[0]);
  out.print(F(" cmd="));    out.print(_pins[1]);
  out.print(F(" att="));    out.print(_pins[2]);
  out.print(F(" dat="));    out.print(_pins[3]);
  out.print(F(" | t="));    out.print(_clkLowUs);
  out.print('/');           out.print(_clkHighUs);
  out.print('/');           out.print(_byteGapUs);
  out.print(F("us | gap=")); out.print(_minGapMs);
  out.print(F("ms | type=0x")); out.print(_controllerType, HEX);
  out.print(F(" | mode=0x")); out.print(_data[1], HEX);
  out.print(F(" | ok="));   out.print(_ok ? 1 : 0);
  out.print(F(" fails="));  out.print(_failCount);
  out.print(F(" frame="));  out.print(_frameUs);
  out.print(F("us"));

  out.print(F(" | data:"));
  for (uint8_t i = 0; i < sizeof(_data); i++) {
    out.print(' ');
    if (_data[i] < 0x10) out.print('0');
    out.print(_data[i], HEX);
  }
  out.println();
}

const __FlashStringHelper *PS2X::errorText(uint8_t code) {
  switch (code) {
    case PS2X_OK:                return PS2X_FSTR("ok");
    case PS2X_ERR_NO_CONTROLLER: return PS2X_FSTR("no controller / invalid frame");
    case PS2X_ERR_NOT_ACCEPTING: return PS2X_FSTR("controller refuses configuration");
    case PS2X_ERR_NO_PRESSURE:   return PS2X_FSTR("no pressure support");
    case PS2X_ERR_BAD_PIN:       return PS2X_FSTR("invalid pin assignment");
    case PS2X_ERR_NOT_CONFIGURED:return PS2X_FSTR("config_gamepad() not called");
    default:                     return PS2X_FSTR("unknown error");
  }
}

const __FlashStringHelper *PS2X::typeText(uint8_t type) {
  switch (type) {
    case 1:  return PS2X_FSTR("DualShock");
    case 2:  return PS2X_FSTR("Guitar Hero");
    case 3:  return PS2X_FSTR("Wireless DualShock (2.4 GHz)");
    case 4:  return PS2X_FSTR("DualShock (digital mode)");
    default: return PS2X_FSTR("unknown");
  }
}
