---
name: Bug report
about: Something does not work — no controller found, wrong values, compile error
title: ''
labels: bug
assignees: ''
---

## What happens

<!-- A clear description: what you expected and what you got. -->

## Environment

| | |
|---|---|
| Board | <!-- e.g. ESP32-WROOM-32, ESP32-S3, Arduino Uno --> |
| Arduino core | <!-- arduino-cli core list --> |
| Library version | <!-- 2.0.0 or the commit hash --> |
| Controller | <!-- wired DualShock 2 / 2.4 GHz wireless receiver / Guitar Hero / clone --> |

## Diagnostics

<!-- Output of File → Examples → PS2X_lib → PS2X_ESP32_Diagnose.
     It prints the config result, controller type, frame time and link quality. -->

```
paste here
```

## Wiring

* Pins: CLK=… CMD=… ATT=… DAT=…
* Receiver supply voltage: <!-- 3.3 V / 5 V -->
* External pull-up on DAT: <!-- none / 4.7 kΩ to 3.3 V -->
* Cable length: <!-- … cm -->
* Paired the wireless receiver before powering the MCU: <!-- yes / no / n/a -->

## Anything else

<!-- Photos of the wiring, scope captures, what you already tried
     (setTiming(), PS2X_DEBUG, another receiver, ...). -->
