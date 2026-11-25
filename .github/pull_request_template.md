## What does this PR change

<!-- One or two sentences. -->

## Type

- [ ] bug fix
- [ ] new example
- [ ] documentation
- [ ] new API (kept backwards compatible)
- [ ] build / CI / packaging

## Checklist

- [ ] The public API stays source compatible (existing sketches compile unchanged)
- [ ] Platform specific code stays inside the `PS2X_HW_*` guards, other cores fall back to `digitalWrite`/`digitalRead`
- [ ] New or changed examples were compiled for ESP32 **and** AVR
- [ ] `CHANGELOG.md` and `CHANGELOG.ru.md` updated for user-visible changes
- [ ] Attribution to the original PS2X_lib authors is preserved (GPLv3)
- [ ] I agree that my contribution is licensed under GPLv3 or later

## How it was tested

<!-- Board, core version, controller, result. Compile matrix output is fine too. -->
