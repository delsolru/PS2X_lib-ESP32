# Contributing to PS2X_lib (LROB.RU edition)

Thanks for taking the time to help! This project is a GPLv3 derivative of the
original [PS2X_lib](https://github.com/madsci1016/Arduino-PS2X) by Bill Porter,
Shutter, Eric Wetzel and Kurt Eckhardt — please keep that in mind when changing
the protocol or the public API.

## Reporting a bug

A useful report contains:

1. **What you expected** and **what happened**.
2. Your board (`ESP32-WROOM`, `ESP32-S3`, `Uno`, …), Arduino core version
   (`arduino-cli core list`) and the library version.
3. The output of `examples/PS2X_ESP32_Diagnose` — it prints the configuration
   result, controller type, frame time and link quality. That single log usually
   answers most questions.
4. Wiring: which GPIOs are used, the supply voltage of the receiver, whether DAT
   has an external pull-up. A photo of the wiring helps a lot.
5. Which controller you use: wired DualShock 2, a 2.4 GHz wireless receiver
   (and whether it was paired before power-up), Guitar Hero, etc.

Please do not paste secrets, and trim logs to the interesting part.

## Pull requests

* Keep the **public API stable**. Adding methods and overloads is fine;
  renaming or removing them breaks user sketches.
* Keep the code **portable**: anything AVR-specific must stay inside
  `#if defined(PS2X_HW_AVR)` (or `__AVR__`), anything ESP32-specific inside
  `#if defined(PS2X_HW_REG)`, with a `digitalWrite`/`digitalRead` fallback for
  every other core.
* No dynamic allocation in the hot path, no `String` in the library.
* Timing values are exposed as microseconds — do not hard-code delay loops that
  silently change with the CPU clock.
* If you add an example, make it compile for **ESP32 and AVR** and keep the pin
  table in the header comment.
* Update `CHANGELOG.md` and `CHANGELOG.ru.md` (both languages) for user-visible
  changes.
* Contributions are accepted under **GPLv3 or later**, the licence this project
  already uses.

## Local checks before opening a PR

```bash
# ESP32
arduino-cli core install esp32:esp32@2.0.8
for ex in examples/*/; do
  arduino-cli compile --fqbn esp32:esp32:esp32 --library "$PWD" -w all "$ex"
done

# AVR
arduino-cli core install arduino:avr@1.8.6
for ex in examples/*/; do
  arduino-cli compile --fqbn arduino:avr:uno --library "$PWD" -w all "$ex"
done
```

The same matrix runs on every push in GitHub Actions
(`.github/workflows/build.yml`), plus `arduino-lint`.

## Style

Two spaces, no tabs, `uint8_t`/`bool` in new code (the legacy `byte`/`boolean`
types stay only where the original API used them), comments in English in the
library and bilingual in the documentation.
