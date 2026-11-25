PS2X_lib 2.0.0 — LROB.RU edition: ESP32 support for the classic PS2 pad library
================================================================================

A GPLv3 derivative of the original [PS2X_lib 1.9](https://github.com/madsci1016/Arduino-PS2X/tree/master/PS2X_lib)
by **Bill Porter** ([@madsci1016](https://github.com/madsci1016)), **Shutter**,
**Eric Wetzel** and **Kurt Eckhardt**. Same header, same class, same methods —
existing sketches compile without a single edit.

Что нового

* **ESP32 (ESP32 / S2 / S3 / C3 / C6)**, ESP8266 and any other Arduino core.
  The AVR path keeps the original direct-port access, so AVR performance is unchanged.
* **Exact microsecond timing on ESP32**: delays spin on the CPU cycle counter
  (`esp_cpu_get_ccount()`), not on `delayMicroseconds()`, so the bus timing does
  not drift with the core clock; `setCpuFrequencyMhz()` is tracked automatically.
* **Diagnostics**: `lastError()`, `ok()`, `failedReads()`, `frameMicros()`,
  `lastOkAge()`, `errorText()`, `typeText()`, `debugPrint(Print&)`, `rawPacket()`.
* **Hot-plug recovery**: no valid frame for 500 ms → full re-initialisation, so a
  pad can be unplugged and plugged back in without a reset.
* **Pin validation** → `PS2X_ERR_BAD_PIN` instead of silently "no controller".
* **Runtime tuning**: `setTiming(clkLow, clkHigh, byteGap)`, `setReadDelay()`.
* `read_gamepad()` returns `bool` now (source compatible).
* Nine examples: minimal, full demo, ESP32 diagnostics, tank drive (L298N),
  servo pan/tilt, stick calibration with EEPROM/NVS, CSV/JSON telemetry,
  two pads on one MCU, pressure buttons + rumble.
* Documentation in English and Russian, CI for ESP32 and AVR, `arduino-lint`.

Установка

* Arduino IDE: `Sketch → Include Library → Add .ZIP Library…` c `PS2X_lib-2.0.0.zip`
  из вложений, либо
  `git clone https://github.com/delsolru/PS2X_lib-ESP32.git Documents/Arduino/libraries/PS2X_lib`.
* PlatformIO: `lib_deps = https://github.com/delsolru/PS2X_lib-ESP32.git`
* Внимание: удалите старую `PS2X_lib`, иначе IDE увидит два одинаковых заголовка.

Wiring, power (3.3 V!), error codes and troubleshooting: see README.md /
README.ru.md. Full change list: CHANGELOG.md / CHANGELOG.ru.md.

Attribution: the PS/2 protocol, the command set, the button/analog tables and the
public API come from the original PS2X_lib 1.9; see NOTICE and LICENSE (GPLv3).
