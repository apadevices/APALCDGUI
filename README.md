# APALCDGUI

**Parallel 20×4 LCD menu system with dual rotary encoders for APA Devices water treatment automation**
· ![v1.1.1](https://img.shields.io/badge/version-1.1.1-blue)
· ![Platforms](https://img.shields.io/badge/platforms-AVR%20ESP8266%20ESP32%20STM32-brightgreen)

---

## Key Features

### Display and navigation
- Parallel 4-bit LiquidCrystal 20×4 LCD — no I2C module required
- Two PEC11R quadrature rotary encoders, native ISR decoding — no external library
- 9-state non-blocking machine: HOME, NAV, EDIT, FLASH_SAVE, FLASH_BACK, BRIGHTNESS, CONFIRM, RTC_NAV, RTC_EDIT
- Up to 4 submenu screens per side (left / right), configurable via `APA_LCD_MAX_SCREENS`
- 1, 2, or 3 fields per screen: INT, FLOAT, CHOICE, BOOL, ACTION, or READONLY

### Alert system
- Passive alerts: corner indicator (`[i]` / `[*]` / `[!]` flashing) — navigation not blocked
- Active alerts: home screen takeover, queue of 3 — each requires operator acknowledgment
- Automatic alert routing from APADOSE via user-supplied callback (libraries stay independent)

### Backlight
- PWM brightness: ACTIVE → DIM → OFF with configurable timeout
- Brightness persisted in EEPROM, adjusted via gesture (hold KB1 + rotate knob1)
- Off-timeout suspended while active alerts are pending

### Flexibility
- Factory functions for all field types — beginners never fill a struct by hand
- BOOL toggle field and ACTION confirmation dialog added in v1.1
- Optional DS3231 RTC modal compiled out if DS3231.h not included
- Long-press and both-buttons-pressed gesture callbacks
- Beginner defaults: all pin numbers match APA Devices HMI board v1.0 — `begin()` takes no arguments

### Engineering
- No heap allocation — `LiquidCrystal` is a direct class member, initialised in the constructor
- ISR singleton: one static instance pointer, two static ISR stubs — fully portable
- `F()` macro on all string literals — zero SRAM cost for labels on AVR
- Zero `delay()` calls — `update()` always returns within one loop iteration

---

## Installation

**Arduino IDE:** Sketch → Include Library → Add .ZIP Library → select the APALCDGUI folder.

**PlatformIO:**
```ini
lib_deps =
    arduino-libraries/LiquidCrystal @ ^1.0.7
    https://github.com/apadevices/APALCDGUI
```

---

## What It Does

APALCDGUI drives a 20-column × 4-row parallel LCD and two rotary encoders as a complete menu system for pool automation hardware. The operator turns the right knob (knob1) to switch between parameter screens, turns the left knob (knob2) to move the cursor between fields, and presses the left knob to enter edit mode or confirm actions. Alarms appear either as a quiet corner indicator (passive) or as a full-screen takeover requiring acknowledgment (active). The backlight dims and extinguishes after inactivity, remembers the operator's preferred brightness, and wakes immediately on any knob movement.

---

## How It Works

```
HOME ──── knob1 (right) rotates ─────────────────► NAV (submenu screen)
     ◄─── menu timeout (60 s) ─────────────────────
     ◄─── BACK confirmed ──────────────────────────

NAV  ──── knob2 (left) moves cursor   0 → 1 → BACK → SAVE → 0
     ──── knob2 press on field ──────────────────► EDIT
EDIT ──── knob2 press ───────────────────────────► NAV (cursor jumps to SAVE)

NAV  ──── cursor on SAVE, press ─────────────────► FLASH_SAVE → onSave() → NAV
NAV  ──── cursor on BACK, press ─────────────────► FLASH_BACK → HOME

Both buttons pressed ────────────────────────────► RTC_NAV (if setRTC() called)
Hold KB1 + rotate knob1 ─────────────────────────► BRIGHTNESS adjust
```

### 20×4 screen layout

```
      0         1         2
      01234567890123456789
Row0: ► pH setpoint  7.24pH
Row1: ► ORP setpoint  680mV
Row2: Filter ON            [!]   ← cols 17–19: passive alert indicator
Row3: ►BACK    →2/4    ►SAVE
```

- Col 0: cursor marker (space or `►`)
- Cols 1–12: field label (max 12 chars)
- Cols 14–17: value (4 chars)
- Cols 18–19: unit (2 chars)
- Row 2 cols 0–16: `setMenuRow2Callback` output or optional screen title
- Row 2 cols 17–19: `[i]` info / `[*]` warning / `[!]` critical alert indicator

---

## Quick Start

```cpp
#include <APALCDGUI.h>

APALCDGUI gui;

float   phSetpoint = 7.20f;
int16_t orpSetpoint = 680;

void drawHome(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("pH  7.24  ORP 680mV "));
    lcd.setCursor(0, 1); lcd.print(F("Cl  1.2   Temp  26C "));
    lcd.setCursor(0, 2); lcd.print(F("Filter  ON   12:34  "));
    lcd.setCursor(0, 3); lcd.print(F("System OK           "));
}

void onSave() { /* write to EEPROM or send to APADOSE here */ }

void setup() {
    gui.begin(); // all pin defaults match HMI board v1.0

    gui.setHomeCallback(drawHome);

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH setpoint"),  F("pH"), &phSetpoint,  6.8f, 7.8f, 0.01f, 2),
        APALCDGUI::fieldInt(  F("ORP setpoint"), F("mV"), &orpSetpoint, 400,  850,  10),
        onSave
    );
}

void loop() {
    gui.update(); // non-blocking — call every loop()
}
```

---

## Understanding Field Factory Parameters

Every field on a screen is created by a factory function. Here is a full breakdown of `fieldFloat`:

```
                ┌── the function ────────────────────────────────────────────────────────────────┐
APALCDGUI::fieldFloat( F("pH setpoint"),  F("pH"),  &phSetpoint,  6.8f,  7.8f,  0.01f,  2 )
                       └── param 1 ───┘  └─ p2 ┘  └── p3 ─────┘ └─p4┘  └─p5┘  └─ p6┘  └p7
```

| # | Example | What it does |
|---|---------|--------------|
| Function | `APALCDGUI::fieldFloat(...)` | Creates a float editing field. Never called alone — always passed as an argument to `addScreen()`. Use `fieldInt` for whole numbers, `fieldChoice` for named options, etc. |
| 1 | `F("pH setpoint")` | Label shown on the left side of the row, up to 12 characters. `F(...)` keeps the text in flash memory — always use it for string literals. |
| 2 | `F("pH")` | Unit suffix shown after the value, up to 2 characters. Pass `nullptr` if no unit is needed. |
| 3 | `&phSetpoint` | Address of your `float` variable. The `&` gives the library direct access — it reads the current value and writes the new one when the operator presses SAVE. |
| 4 | `6.8f` | Minimum value. Rotating below this has no effect. The `f` suffix marks it as a float constant. |
| 5 | `7.8f` | Maximum value. Rotating above this has no effect. |
| 6 | `0.01f` | Change per encoder click. `0.01f` = fine, `0.1f` = medium, `1.0f` = coarse. |
| 7 | `2` | Decimal places shown on the LCD. `2` → `7.24`, `1` → `7.2`, `0` → `7`. |

**What the operator sees during editing:**
```
►pH setpoint  7.24pH
```

### All field factories at a glance

```cpp
// Integer value — operator scrolls between min and max by step
fieldInt(label, unit, int16_t* val, min, max, step = 1)

// Decimal value — same as INT but displays with a fixed number of decimal places
fieldFloat(label, unit, float* val, min, max, step, decimals)

// Named options — operator cycles through the list; each string MUST be exactly 4 chars
// Pad shorter strings with trailing spaces: {"AUTO", "MANU", "OFF ", nullptr}
fieldChoice(label, uint8_t* index, const char* choices[])

// On/off toggle — shows " ON " or "OFF "; rotate to preview, press to commit
fieldBool(label, bool* val)

// Button — shows "STRT" on SAVE; confirm=true adds a "Confirm action?" prompt first
fieldAction(label, void (*fn)(), confirm = false)

// Display only — cursor skips this field; no editing possible
fieldReadonly(label, unit, float* val, decimals)
```

---

## API Reference

### Initialisation

| Method | Description |
|--------|-------------|
| `APALCDGUI(rs, en, d4..d7)` | Constructor — sets LCD pin assignments. All defaults match HMI board v1.0. LCD pins are fixed here; not in `begin()`. |
| `begin(blPin, enc1Clk, enc1Dt, enc1Btn, enc2Clk, enc2Dt, enc2Btn, det1, det2)` | Initialise LCD, attach encoder ISRs, load custom characters, restore brightness from EEPROM. All defaults match HMI board v1.0. |
| `update()` | Process encoders, buttons, timeouts, redraw LCD. Call every `loop()` — never blocks. |

### Screen registration

| Method | Description |
|--------|-------------|
| `setHomeCallback(fn)` | Home screen draw function. Receives `LiquidCrystal&` — the only callback with a parameter. |
| `setMenuRow2Callback(fn)` | Draw live sensor data on row 2 of every 1- and 2-field screen (cols 0–16 only). |
| `addScreen(side, field1, onSave, title)` | Register a 1-field screen. |
| `addScreen(side, field1, field2, onSave, title)` | Register a 2-field screen (most common). |
| `addScreen(side, field1, field2, field3, onSave, title)` | Register a 3-field screen. |
| `setRTC(DS3231*)` | Wire both-pressed gesture to built-in time/date modal. Requires `DS3231.h` before `APALCDGUI.h`. |

### Field factories

| Factory | Description |
|---------|-------------|
| `fieldInt(label, unit, int16_t*, min, max, step=1)` | Integer field, scrolls by step. |
| `fieldFloat(label, unit, float*, min, max, step, decimals)` | Float field, displayed with N decimal places. |
| `fieldChoice(label, uint8_t*, const char*[])` | Cycles through null-terminated string array. Each string must be exactly 4 chars. |
| `fieldBool(label, bool*)` | Toggle — shows `" ON "` / `"OFF "`. |
| `fieldAction(label, fn, confirm=false)` | Button — shows `"STRT"`. `confirm=true` adds confirmation prompt. |
| `fieldReadonly(label, unit, float*, decimals)` | Display only — cursor skips this field. |

### Alerts

| Method | Description |
|--------|-------------|
| `postAlert(l1, l2, level)` | Show passive corner indicator. Levels: `ALERT_INFO`, `ALERT_WARNING`, `ALERT_CRITICAL`. |
| `clearAlert()` | Dismiss passive alert. |
| `hasAlert()` | True if passive alert is active. |
| `postActiveAlert(l1, l2, level, ackCb)` | Add to active alert queue (max 3). Replaces home screen until acknowledged. |
| `cancelActiveAlert()` | Remove current active alert silently (for auto-cleared alarms). |
| `hasActiveAlert()` / `activeAlertCount()` | Query active alert state. |

### Backlight and timeouts

| Method | Description |
|--------|-------------|
| `setBacklight(bool)` | Force on or off immediately. |
| `setBacklightTimeout(seconds)` | Off timeout; dim fires at 40% of this value. `0` = always on. Default 300 s. |
| `setMenuTimeout(seconds)` | Return to HOME after idle. `0` = disabled. Default 60 s. |

### Gestures and overlays

| Method | Description |
|--------|-------------|
| `setLongPressCallback(enc, fn)` | 800 ms hold: `0` = right knob (KB1), `1` = left knob (KB2). |
| `setBothPressedCallback(fn)` | Both buttons within 200 ms — always wins over RTC modal. |
| `showMessage(l1, l2, ms)` | Timed 2-line overlay covering rows 1–2. Default 1500 ms. |
| `clearMessage()` | Dismiss overlay early. |
| `markDirty()` | Schedule a full LCD redraw on the next `update()`. |
| `isMenuActive()` | True when not at HOME. |
| `isEditActive()` | True during EDIT or RTC_EDIT state. |
| `currentScreen()` | Screen position: `0` = HOME, `+N` = right screen N, `-N` = left screen N. |
| `getBrightness()` | Current backlight level (0–255, EEPROM-persisted). |

---

## Wiring APADOSE Alarms to APALCDGUI

Libraries are independent — user code is the bridge:

```cpp
dose1.setAlarmCallback([](AlarmType alarm) {
    if (alarm == ALARM_NONE) {
        gui.cancelActiveAlert();
        return;
    }
    if (alarm == ALARM_INEFFECTIVE || alarm == ALARM_WRONG_DIRECTION
     || alarm == ALARM_TANK_EMPTY  || alarm == ALARM_OFA) {
        gui.postActiveAlert(F("ALARM: Ineffective"), F("pH-minus pump #1"),
                            ALERT_CRITICAL, []() { dose1.acknowledgeAlarm(); });
    } else {
        gui.postAlert(F("Warning: Safety band"), F("pH-minus"), ALERT_WARNING);
    }
});
```

---

## DS3231 RTC — Time and Date Setting

The library includes a built-in time/date edit modal for the DS3231 module. Enable it by defining `APA_LCD_USE_DS3231` — the library then pulls in `Wire.h` and `DS3231.h` automatically.

**Wiring (Mega 2560):** SDA → pin 20, SCL → pin 21, VCC → 3.3 V, GND → GND.

**`platformio.ini`:**
```ini
lib_deps =
    arduino-libraries/LiquidCrystal @ ^1.0.7
    NorthernWidget/DS3231
build_flags = -DAPA_LCD_USE_DS3231
```

**Arduino IDE** — add before `#include`:
```cpp
#define APA_LCD_USE_DS3231
#include <APALCDGUI.h>
```

**Sketch:**
```cpp
#include <APALCDGUI.h>   // Wire.h and DS3231.h are included automatically

APALCDGUI gui;
DS3231    rtc;

void setup() {
    Wire.begin();        // required — initialise I2C bus
    gui.begin();
    gui.setRTC(&rtc);   // wires both-buttons gesture to the modal
    // addScreen() calls ...
}
```

**Operation:** press both buttons within 200 ms → the modal opens. Knob1 switches between the TIME and DATE sub-screens. Knob2 moves the cursor; press to edit, rotate to change, press to confirm. Cursor on SAVE + press writes to the chip. Cursor on BACK + press discards.

> `setBothPressedCallback()` takes priority over the RTC modal — do not set it if you want the modal to work.

See `examples/mega/03_rtc/03_rtc.ino` for a full working example with live clock display on the home screen.

---

## Configurable Limits

Define these **before** `#include <APALCDGUI.h>`:

```cpp
#define APA_LCD_MAX_SCREENS        4    // total submenu screens left+right (default 4)
#define APA_LCD_ACTIVE_ALERT_QUEUE 3    // simultaneous active alerts (default 3)
#define APA_LCD_EEPROM_ADDR      500    // EEPROM base address (default 500, uses 2 bytes)
```

---

## Platform Verification

Compiled and size-checked with the 8-screen example (`APA_LCD_MAX_SCREENS=8`) on all supported platforms. Zero errors, zero library warnings.

| Platform | Board | Clock | RAM used | RAM total | Flash used | Flash total |
|----------|-------|-------|----------|-----------|------------|-------------|
| Arduino Mega 2560 | ATmega2560 | 16 MHz | 1 678 B | 8 192 B (20%) | 19 446 B | 253 952 B (8%) |
| Arduino Uno | ATmega328P | 16 MHz | 1 666 B | 2 048 B (81%) | 17 546 B | 32 256 B (54%) |
| ESP32 DevKit | ESP32 | 240 MHz | 23 916 B | 327 680 B (7%) | 297 729 B | 1 310 720 B (23%) |
| ESP8266 D1 Mini | ESP8266 | 80 MHz | 30 536 B | 81 920 B (37%) | 282 267 B | 1 044 464 B (27%) |
| STM32 Bluepill | STM32F103C8 | 72 MHz | 3 988 B | 20 480 B (19%) | 36 800 B | 65 536 B (56%) |

> The Uno row shows 81% RAM with the 8-screen example — that is the example sketch overhead including 8 `float` / `int16_t` globals and 8 screen registrations. The library core alone compiles cleanly. For production Uno use, a simpler 2–4 screen sketch will sit well below 50%.
>
> ESP32 and ESP8266 totals include the full Arduino framework (WiFi stack etc.) regardless of whether it is used.

---

## License

MIT — see LICENSE file.
