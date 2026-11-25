# Changelog

All notable changes to this **LROB.RU edition** of `PS2X_lib`.

This edition is a derivative work of the original **PS2X_lib 1.9** by Shutter,
Bill Porter, Eric Wetzel and Kurt Eckhardt
(<https://github.com/madsci1016/Arduino-PS2X/tree/master/PS2X_lib>), released under
the GNU GPLv3. Everything below is a modification made by LROB.RU on top of that
original; unmodified behaviour is credited to the original authors.
Russian version: [CHANGELOG.ru.md](CHANGELOG.ru.md).

## 2.0.0 — LROB.RU edition: ESP32 port (and any other MCU)

### Added
* Works on the whole ESP32 family (ESP32, S2, S3, C3, C6), on ESP8266 and on any
  other Arduino core thanks to a portable GPIO abstraction.
* Exact microsecond timing on ESP32: delays spin on the CPU cycle counter
  (`esp_cpu_get_ccount()`) instead of `delayMicroseconds()`, so the bus timing does
  not depend on the core clock and follows `setCpuFrequencyMhz()` automatically.
* `setTiming(clkLow, clkHigh, byteGap)` — retune the bus speed at runtime.
* Status reporting: `lastError()`, `ok()`/`connected()`, `failedReads()`,
  `frameMicros()`, `lastOkAge()`, `errorText()`, `typeText()`.
* `debugPrint(Print&)` — state dump without recompiling with `PS2X_DEBUG`.
* `rawPacket()` / `rawPacketSize()` — access to the raw 21-byte answer.
* Pin validation → `PS2X_ERR_BAD_PIN` instead of silently "finding no controller".
* Automatic recovery when the controller is unplugged/replugged (hot-plug) or drops
  out of analog mode; can be disabled with `setAutoReconnect(false)`.
* `setReadDelay()` / `readDelay()`.
* `read_gamepad()` now returns `bool` (was `void`); old code still compiles.
* README in English and Russian, full GPLv3 text in `LICENSE`, `library.json` for
  PlatformIO, `PS2X_ESP32_Diagnose` example.
* Six more examples for real tasks: `PS2X_TankDrive` (two DC motors, L298N style),
  `PS2X_ServoPanTilt` (rate-controlled servos, no Servo library needed on ESP32),
  `PS2X_Calibrate` (stick calibration stored in EEPROM/NVS), `PS2X_Telemetry`
  (CSV/JSON stream with link quality), `PS2X_DualControllers` (two pads on one MCU)
  and `PS2X_PressureRumble` (pressure buttons + vibration feedback).

### Fixed
* Removed AVR-only `#include <avr/io.h>`, `SREG`, `cli()`, `portOutputRegister()`,
  `digitalPinToBitMask()` from the common path — these were the reason the library
  did not compile on ESP32.
* `byte` / `boolean` replaced by `uint8_t` / `bool` in the public API (sketches
  written for the original compile unchanged).
* `bitSet()` replaced by a plain shift (it is an AVR-core-only macro).
* On ESP32 the DAT line is set to `INPUT_PULLUP`; the original relied on
  `digitalWrite(HIGH)` on an input pin, which is a no-op on ESP32.
* `readType()` no longer prints to `Serial` unconditionally.
* `Analog()` index is bounds-checked.
* A failed frame no longer overwrites the button state with garbage.
* The first good frame after (re)connect no longer reports every held button as
  "just pressed".

### Compatibility
* Class, method and constant names, header name and file layout are those of
  PS2X_lib 1.9 — existing sketches only need the library reinstalled.
