# PS2X_lib 2.0 — PlayStation 2 controller for Arduino AVR and ESP32

[![CI](https://github.com/delsolru/PS2X_lib-ESP32/actions/workflows/build.yml/badge.svg)](https://github.com/delsolru/PS2X_lib-ESP32/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![Arduino library](https://img.shields.io/badge/Arduino-library-00979D.svg)](library.properties)
[![Upstream](https://img.shields.io/badge/upstream-PS2X_lib%201.9-lightgrey.svg)](https://github.com/madsci1016/Arduino-PS2X)

**LROB.RU edition** · maintained by [lrob.ru](https://lrob.ru) · GPLv3

🇷🇺 Русская версия этого документа: **[README.ru.md](README.ru.md)**

---

## Authorship and licence — read this first

This is **not a new library**. It is a derivative work, a port and a clean-up of the
classic **PS2X_lib 1.9** ("Super amazing PS2 controller Arduino Library"):

| | |
|---|---|
| Original authors | **Shutter** (Arduino forums), **Bill Porter** ([billporter.info](http://www.billporter.info), [@madsci1016](https://github.com/madsci1016)), with **Eric Wetzel** and **Kurt Eckhardt** |
| Upstream project | <https://github.com/madsci1016/Arduino-PS2X/tree/master/PS2X_lib> — the repository of [@madsci1016](https://github.com/madsci1016) (Bill Porter) |
| Licence | **GNU GPL v3 or later** — the same as the original (full text in [LICENSE](LICENSE)) |
| What came from the original | the PS/2 protocol implementation, the configuration command set, the button/analog constant tables, the class name `PS2X`, all method names, the examples |
| What is new here | the portable hardware layer (AVR / ESP32 / generic), the ESP32 timing, the diagnostics, the hot-plug recovery, the documentation — see *What changed* below |
| Maintainer of this edition | **LROB.RU** — <https://lrob.ru> |

Because it is derived from PS2X_lib 1.9, this edition **must stay under the GPLv3**:
if you redistribute or modify it, keep the attribution above and state your changes.
`CHANGELOG.md` / `CHANGELOG.ru.md` record every modification made by LROB.RU.

---

## What changed

### Why the original 1.9 does not work on ESP32

| Original code | What happens on ESP32 |
|---|---|
| `#include <avr/io.h>`, `SREG`, `cli()` | file and registers do not exist → **compile error** |
| `portOutputRegister()`, `digitalPinToBitMask()`, pointer arithmetic `_lport + 1/+2` | AVR/PIC32 only → compile error or writes into the wrong registers |
| `byte`, `boolean` in the public API | depend on the core's typedefs |
| `bitSet()` | AVR core macro, missing in the ESP32 core |
| `pinMode(dat, INPUT)` + `digitalWrite(dat, HIGH)` (enable pull-up) | on AVR this enables the pull-up; on ESP32 `digitalWrite` on an input is a **no-op** → DAT floats and reads garbage |
| `delayMicroseconds(4)` for the bus | on ESP32 it polls `esp_timer_get_time()` in a loop: expensive and very jittery at 1–5 µs, which is exactly the PS/2 bit window |
| `readType()` prints to `Serial` unconditionally | breaks the application's serial output |
| no pin validation | GPIO 6–11 is SPI flash, GPIO 34–39 are input only → the library silently "finds no controller" |

### What this edition does instead

1. **Portable GPIO layer.** One small `PS2X_HwPin` abstraction with three back-ends:
   AVR `PORTx/PINx` registers (unchanged, same speed as the original), ESP32
   `GPIO_OUT_W1TS/OUT1_W1TS` + `GPIO_IN/IN1` registers, and plain
   `digitalWrite()/digitalRead()` for everything else (ESP8266, RP2040, SAMD, …).
   A clock edge is a single register store, so the bus timing does not depend on
   the CPU.
2. **Real microsecond timing on ESP32.** Delays spin on the CPU cycle counter
   (`esp_cpu_get_ccount()`), i.e. they are counted in ticks, not in function calls;
   the calibration follows `setCpuFrequencyMhz()` automatically.
3. **Same protocol, same API.** Bit-banging order, ACK slot, command bytes,
   retry logic, `read_delay` growth — all kept identical to 1.9. Existing sketches
   compile unchanged (see *Compatibility*).
4. **`INPUT_PULLUP` on DAT**, pin validation with a dedicated error code
   (`PS2X_ERR_BAD_PIN`) instead of silence.
5. **Diagnostics**: `lastError()`, `ok()`, `failedReads()`, `frameMicros()`,
   `debugPrint(Print&)`, `errorText()`, `typeText()`.
6. **Hot-plug recovery**: no valid frame for 500 ms → the full initialisation runs
   again, so a controller can be unplugged and plugged back in without a reset.
7. **Runtime tuning**: `setTiming(clkLow, clkHigh, byteGap)` and `setReadDelay()`
   for long wires and picky 2.4 GHz receivers.
8. **No junk state**: on a failed frame the button state is not updated, and the
   first good frame after (re)connect does not report every held button as
   "just pressed".
9. **`read_gamepad()` now returns `bool`** (was `void`) so you can write
   `if (!ps2x.read_gamepad()) …`; old code that ignores the value still compiles.
10. **Docs**: this file, `README.ru.md`, `CHANGELOG.md`, `CHANGELOG.ru.md`,
    `library.json` for PlatformIO, full GPLv3 text.

---

## Compatibility

* Same header `PS2X_lib.h`, same class `PS2X`, same method names and constants —
  a sketch written for PS2X_lib 1.9 compiles without a single edit (verified with a
  deliberately "old style" sketch using `byte`, `boolean`, `int error`, both
  `config_gamepad()` forms).
* Remove the old `PS2X_lib` from your libraries folder before installing this one:
  both provide `PS2X_lib.h` and the IDE would have to choose.

---

## Wiring

```
     PS2 receiver / pad                ESP32            Arduino UNO
     ------------------               -----            -----------
     DAT  (data)                      GPIO27             D13
     CMD  (command)                   GPIO23             D11
     ATT  (attention / select)        GPIO19             D10
     CLK  (clock)                     GPIO18             D12
     VCC                               3.3V               3.3V
     GND                               GND                GND
```

* **Power it from 3.3 V.** Practically all receivers (including the cheap
  "wireless" clones) work fine from 3.3 V; a wired DualShock 2 does too, only its
  vibration gets weaker.
* **DAT is an ESP32 input** and ESP32 inputs are not 5 V tolerant. If your
  receiver really drives 5 V on DAT, add a divider (2.2 kΩ + 3.3 kΩ) or a level
  shifter. 5 V straight into a GPIO damages the pin.
* If DAT is unstable, add an external **4.7–10 kΩ pull-up to 3.3 V** (the ESP32
  internal pull-up is ~45 kΩ).
* Common GND is mandatory. Keep the wires short, twisted pair is better; 150 kHz
  over 30 cm of loose jumper wires already glitches.
* Wireless receiver: **pair** it with the pad first (button on the receiver), then
  power the ESP32; give it `delay(300)` before `config_gamepad()`.

Pins you must not use on ESP32:

| Pins | Why |
|---|---|
| 6–11 | connected to the SPI flash |
| 34–39 | input only (fine for DAT, not for CLK/CMD/ATT) |
| 0, 2, 12, 15 | strapping pins — they change the boot mode |
| 1, 3 | UART0 (`Serial`) |
| 16, 17 | used by PSRAM on WROVER boards |

Default set used in the examples: **CLK=18, CMD=23, ATT=19, DAT=27**.

---

## Installation

### Arduino IDE 2.x
1. Remove the old `PS2X_lib` first (both provide `PS2X_lib.h`).
2. Install this edition in any of these ways:
   * `Sketch → Include Library → Add .ZIP Library…` with `PS2X_lib-2.0.0.zip`
     from the [releases page](https://github.com/delsolru/PS2X_lib-ESP32/releases);
     GitHub's "Code → Download ZIP" works too (the IDE renames the folder itself);
   * `git clone https://github.com/delsolru/PS2X_lib-ESP32.git Documents/Arduino/libraries/PS2X_lib`
   * or just copy the repository folder into `Documents/Arduino/libraries/`.
3. Open `File → Examples → PS2X_lib → PS2X_Example`.

### PlatformIO
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = https://github.com/delsolru/PS2X_lib-ESP32.git
```

---

## Quick start

```cpp
#include <PS2X_lib.h>

PS2X ps2x;

void setup() {
  Serial.begin(115200);
  delay(300);                                    // let a wireless receiver boot

  uint8_t err = ps2x.config_gamepad(18, 23, 19, 27, false, false);
  //                              CLK CMD ATT DAT
  Serial.println(PS2X::errorText(err));          // "ok" or the reason
}

void loop() {
  if (!ps2x.read_gamepad(false, 0)) {            // small motor, big motor 0..255
    delay(100);
    return;
  }

  if (ps2x.Button(PSB_START))        Serial.println("START");
  if (ps2x.ButtonPressed(PSB_CROSS)) Serial.println("X");

  int lx = ps2x.Analog(PSS_LX);   // 0..255, ~128 at rest
  int ly = ps2x.Analog(PSS_LY);
}
```

---

## API

### As in the original (unchanged)

| Method | Meaning |
|---|---|
| `uint8_t config_gamepad(clk, cmd, att, dat, pressures=false, rumble=false)` | initialise; returns 0 on success |
| `bool read_gamepad()` / `bool read_gamepad(bool motor1, uint8_t motor2)` | read one frame; `motor1` small motor on/off, `motor2` big motor 0..255 |
| `bool Button(uint16_t)` | true when **any** of the given buttons is held |
| `bool ButtonPressed(uint16_t)` / `ButtonReleased(uint16_t)` | just pressed / just released |
| `bool NewButtonState()` / `NewButtonState(uint16_t)` | state changed (any / this one) |
| `unsigned int ButtonDataByte()` | inverted 16-bit button word |
| `uint8_t Analog(uint8_t)` | sticks `PSS_LX/LY/RX/RY`, pressure buttons `PSAB_*`, `WHAMMY_BAR` |
| `uint8_t readType()` | 1 DualShock, 2 Guitar Hero, 3 wireless, 4 DualShock in digital mode, 0 unknown |
| `void enableRumble()` / `bool enablePressures()` | enable at runtime |
| `void reconfig_gamepad()` | resend the configuration |

### New in 2.0

| Method | Meaning |
|---|---|
| `lastError()`, `ok()`, `connected()` | error code of the last operation / last frame valid |
| `failedReads()`, `lastOkAge()`, `lastReadAge()` | link quality |
| `frameMicros()` | duration of the last `read_gamepad()` call, µs |
| `setTiming(clkLow, clkHigh, byteGap)` | bus timing in µs |
| `setReadDelay(ms)` / `readDelay()` | minimum gap between frames (1..10 ms) |
| `setAutoReconnect(bool)` | automatic re-initialisation (on by default) |
| `rawPacket()`, `rawPacketSize()` | raw 21-byte answer |
| `debugPrint(Print&)` | one-line state dump |
| `PS2X::errorText(code)`, `PS2X::typeText(type)` | human readable strings |

---

## How it works inside

* **The host owns the clock.** The library generates CLK and knows when to sample
  DAT, so interrupts (Wi-Fi, BT, timers) do **not** need to be disabled: a GPIO
  register write is a single atomic instruction, and if an interrupt does stretch
  a clock phase the data does not get corrupted — the controller changes DAT only
  on the clock edges that we generate.
* A corrupted frame is rejected by its mode byte (`0x7x` = analog) and the library
  retries up to 5 times.
* **Default timing**: 4 µs low + 2 µs high + 3 µs inter-byte gap → SCK ≈ 166 kHz
  (the PS/2 interface allows up to 250 kHz). One 9-byte frame ≈ **0.45 ms**, a full
  21-byte frame (with pressures) ≈ **1.05 ms**.
* **Recovery**: if no valid frame arrives for 500 ms, the full initialisation runs
  again. If the library is not polled for 1.5 s the controller falls back to
  digital mode — this is detected and the configuration is re-sent (as in the
  original).

---

## Tuning

Some cheap wireless receivers cannot keep up with 150 kHz. Symptoms: error 1 or 2,
or every other frame invalid.

```cpp
ps2x.setTiming(6, 3, 5);   // slower and safer (AVR original ≈ 4/1/3)
ps2x.setTiming(2, 1, 2);   // faster — short wires only
ps2x.setReadDelay(4);      // no more than one frame per 4 ms
```

Debug output (with readable error text, unlike the original):

```cpp
#define PS2X_DEBUG      1   // before #include <PS2X_lib.h> — setup/errors
#define PS2X_COM_DEBUG  1   // per-byte bus log (very verbose)
```

Raw packet layout (`rawPacket()`, i.e. `Analog(i) == rawPacket()[i]`):

| Index | Content |
|---|---|
| 0 | 0x01 (header) |
| 1 | mode: `0x73` analog / `0x79` analog + pressures / `0x41` digital |
| 2 | 0x5A (always) |
| 3–4 | buttons, low/high byte (inverted) |
| 5–8 | RX, RY, LX, LY |
| 9–20 | pressure data (only with `pressures = true`) |

---

## Error codes

| Code | `errorText()` | What to do |
|---|---|---|
| 0 | ok | all good |
| 1 | no controller / invalid frame | 3.3 V power, GND, correct pins, receiver paired, DAT pull-up, `delay(300)` in setup |
| 2 | controller refuses configuration | controller answers but rejects commands: usually too fast/noisy a bus → `setTiming()` slower, shorter wires |
| 3 | no pressure support | pad has no pressure sensitive buttons — call `config_gamepad(..., false, ...)` |
| 4 | invalid pin assignment | GPIO 6–11, input-only pin used as output, or pin does not exist |
| 5 | config_gamepad() not called | call it in `setup()` |

---

## Examples

| Example | Purpose |
|---|---|
| `PS2X_Example` | full demo (like the original: buttons, sticks, Guitar Hero, rumble) |
| `PS2X_ESP32_Diagnose` | **start here**: prints errors, controller type, frame time, link quality; commands `c/d/r/s/f/?` (re-init, dump, rumble test, slower/faster) |
| `PS2X_Minimal` | smallest working sketch + stick-to-`-100..+100` tank mixing |
| `PS2X_TankDrive` | two DC motors (L298N / L293D / TB6612): tank mixing, dead zone, slew limiting, L2/R2 speed trigger, emergency stop, motors stop on link loss |
| `PS2X_ServoPanTilt` | camera / arm pan-tilt: the right stick *rate-controls* the servos, homing on TRIANGLE, slow mode on SQUARE — runs without the Servo library on ESP32 (uses LEDC) |
| `PS2X_Calibrate` | measures min/max/centre and dead zone of every stick axis and stores them in EEPROM (AVR) / NVS (ESP32); includes a calibrated `raw → -1000..+1000` helper |
| `PS2X_Telemetry` | 50 Hz machine-readable stream: CSV for spreadsheets/plotting or JSON for scripts, including link quality (`ok`, `fails`, frame time) |
| `PS2X_DualControllers` | two pads on one MCU (two receivers): pad 1 drives, pad 2 controls an arm/turret, both survive a missing pad thanks to hot-plug |
| `PS2X_PressureRumble` | pressure-sensitive buttons (21-byte frame) and vibration feedback driven by the L2/R2 triggers; falls back to on/off pads |

All examples are written for both ESP32 and AVR (pin tables are in the header of
each sketch) and only use libraries that ship with the cores.

---

## Verified

Compiled with arduino-cli (Arduino IDE 2.x tooling), no errors and no warnings
from the library sources. Every push and pull request is rebuilt by GitHub Actions
(.github/workflows/build.yml: arduino-lint + the matrix below):

* **ESP32 Arduino core 2.0.8** — all nine examples on `esp32:esp32:esp32`;
  cross-checked on `esp32s3` and `esp32c3` for the sketches with platform
  specific code (motors, servos, EEPROM, telemetry, two pads);
* **AVR core 1.8.6** — all nine examples on `arduino:avr:uno`; cross-checked on
  `nano`, `mega`, `leonardo`;
* build variants: `PS2X_FORCE_SOFT_GPIO`, `PS2X_DEBUG=1`, `PS2X_COM_DEBUG=1`,
  `PS2X_SKIP_PIN_CHECK`, and an old-style sketch written for PS2X_lib 1.9 that
  compiles unchanged.

Hardware checklist for you (this part cannot be done on the build machine):

1. `PS2X_ESP32_Diagnose` → `config_gamepad() -> 0 (ok)`, `controller type: DualShock`;
2. button presses show changing `buttons 0x…`, sticks are smooth 0..255
   (~128 at rest, 0/255 at the extremes);
3. `frame=…us` is stable (±20 %), `fails=0`;
4. unplug and replug the pad — the link comes back within ~0.5 s;
5. rumble test (command `r`) if your pad supports it.

If something is off: first `s` (slower bus) in the diagnostic sketch, then an
external 4.7 kΩ pull-up on DAT, then the power supply.

---

## Limitations

* No hardware SPI/DMA: the PS/2 protocol needs per-byte ACK handling and a
  host-driven clock, so the bus is bit-banged (as in the original).
* Two pads = two `PS2X` objects on **different** pins (two pads cannot share one bus).
* Not compatible with `light sleep` / aggressive power management — the bus timing
  and pull-ups are lost (irrelevant for the usual robot use case).
* Guitar Hero support is the same as in the original (type 2); pressure data only
  on compatible DualShock pads.

---

## Credits

* **Shutter**, **Bill Porter**, **Eric Wetzel**, **Kurt Eckhardt** — original
  PS2X_lib 1.9: protocol, command set, button tables, examples.
* **LROB.RU** — ESP32 port, portable GPIO layer, diagnostics, documentation.
* Licence: **GPLv3 or later** (see [LICENSE](LICENSE)). Changes are listed in
  [CHANGELOG.md](CHANGELOG.md).
