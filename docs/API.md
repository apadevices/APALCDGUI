# APALCDGUI API Reference — v1.2.3

## Quick-start examples

### Minimal (1 screen, 2 fields)
```cpp
#include <APALCDGUI.h>

APALCDGUI gui;                       // default pins = APA HMI board v1.0

float   phSetpoint  = 7.20f;
int16_t orpSetpoint = 680;

void onSave() { /* write to EEPROM or send to APADOSE */ }

void setup() {
    gui.begin();
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH setpoint"),  F("pH"), &phSetpoint,  6.8f, 7.8f, 0.01f, 2),
        APALCDGUI::fieldInt(  F("ORP setpoint"), F("mV"), &orpSetpoint, 400,  850,  10),
        onSave
    );
}

void loop() { gui.update(); }
```

### Home screen with live sensor data
```cpp
void drawHome(LiquidCrystal& lcd) {
    char buf[21];
    char fa[5];
    dtostrf(g_ph, 4, 2, fa);                    // AVR: use dtostrf, not %f
    snprintf(buf, sizeof(buf), "pH%s  ORP%3dmV ", fa, g_orp);
    lcd.setCursor(0, 0); lcd.print(buf);
    lcd.setCursor(0, 3); lcd.print(F("K1:screens K2:edit  "));
    // Note: do NOT write to row 2 cols 17–19 (alert indicator) or
    //       row 3 cols 17–19 (page indicator when > 1 page) — library overwrites them.
}
// ...
gui.addHomeScreen(drawHome);  // or gui.setHomeCallback(drawHome) — identical
```

---

## Setup order

| Step | Call | Notes |
|------|------|-------|
| 1 | `APALCDGUI gui;` | Global — constructs with LCD pin defaults |
| 2 | `gui.begin()` | In `setup()` — init LCD, encoders, EEPROM; timer data auto-loaded |
| 3 | `gui.addHomeScreen(fn)` | Optional — register home page(s); call multiple times for scrollable dashboard |
| 3a | `gui.setHomeCallback(fn)` | Alias for `addHomeScreen()` — single-page sketches need no changes |
| 4 | `gui.setMenuRow2Callback(fn)` | Optional — live data on menu row 2 |
| 5 | `gui.addScreen(...)` | Register as many screens as needed |
| 5a | `gui.addTimerScreen(side, onSave)` | **After** all `addScreen()` on the same side — always last |
| 6 | `gui.setLongPressCallback(...)` | Optional — override KB2 long-press |
| 7 | `gui.setBothPressedCallback(fn)` | Optional — both-buttons gesture |
| 8 | `gui.setRTC(&rtc)` | Optional — DS3231 time/date modal |
| — | `gui.update()` | In `loop()` — call every iteration |

---

## Constructor

```cpp
APALCDGUI(uint8_t rs=26, uint8_t en=27,
          uint8_t d4=22, uint8_t d5=23, uint8_t d6=24, uint8_t d7=25);
```

Sets LCD pin assignments. All defaults match the APA Devices HMI board v1.0.
The LCD is not started until `begin()`.

---

## `begin()`

```cpp
void begin(
    uint8_t blPin     = 4,
    uint8_t enc1Clk   = 2,  uint8_t enc1Dt   = 3,  uint8_t enc1Btn   = 10,
    uint8_t enc2Clk   = 19, uint8_t enc2Dt   = 18, uint8_t enc2Btn   = 11,
    uint8_t enc1Detents = 4,
    uint8_t enc2Detents = 4
);
```

Initialises the LCD, attaches encoder interrupts, loads custom characters, and restores brightness from EEPROM.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `blPin` | 4 | PWM pin for backlight MOSFET gate |
| `enc1Clk/Dt/Btn` | 2, 3, 10 | Right knob (knob1 / KB1) — screen navigation |
| `enc2Clk/Dt/Btn` | 19, 18, 11 | Left knob (knob2 / KB2) — cursor and editing |
| `enc1Detents` | 4 | Pulses per physical click — 4 is correct for PEC11R |
| `enc2Detents` | 4 | Same |

---

## `update()`

```cpp
void update();
```

Runs all GUI logic: reads encoders, processes button presses, handles timeouts, redraws LCD when needed. Must be called on every `loop()` iteration. Never blocks, never calls `delay()`.

---

## Screen registration

### `addHomeScreen()`
```cpp
bool addHomeScreen(void (*fn)(LiquidCrystal& lcd));
```
Registers a home screen page. Call once for a single page, or multiple times for a scrollable dashboard — the operator scrolls between pages by rotating **knob2 (left knob)** while on the home screen.

`fn` is called on every `update()` while that page is shown. Keep it fast — no `delay()`, no blocking calls. **The only callback that receives a parameter** — all others are `void(void)`.

Returns `false` if `APA_LCD_MAX_HOME_SCREENS` is already reached (default 4). To increase the limit, define it before `#include`:
```cpp
#define APA_LCD_MAX_HOME_SCREENS 6
#include <APALCDGUI.h>
```

**Automatic page indicator:** when more than one page is registered, the library draws `n/N` at **row 3 cols 17–19** after the callback returns. Do not write to that position in your callbacks — the library overwrites it on every `update()`. When only one page is registered, no indicator is drawn and all 20 columns of row 3 are yours.

**Example — two-page dashboard:**
```cpp
void drawPage1(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("pH  7.24  ORP 680mV "));
    lcd.setCursor(0, 1); lcd.print(F("Cl  1.2   Temp  26  "));
    lcd.write(CC_DEGREE);
    lcd.setCursor(0, 2); lcd.print(F("Filter ON   12:34   "));
    lcd.setCursor(0, 3); lcd.print(F("System OK           "));
    // row 3 cols 17–19: written by library as "1/2"
}

void drawPage2(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("Acid doses this week"));
    lcd.setCursor(0, 1); lcd.print(F("pH-:  14  CL+:  22  "));
    lcd.setCursor(0, 2); lcd.print(F("Last dose:   12:10  "));
    lcd.setCursor(0, 3); lcd.print(F("Total this week 136 "));
    // row 3 cols 17–19: written by library as "2/2"
}

void setup() {
    gui.begin();
    gui.addHomeScreen(drawPage1);
    gui.addHomeScreen(drawPage2);
}
```

### `setHomeCallback()`
```cpp
void setHomeCallback(void (*fn)(LiquidCrystal& lcd));
```
Alias for `addHomeScreen()`. Existing single-page sketches that call `setHomeCallback()` compile and run unchanged.

### `setMenuRow2Callback()`
```cpp
void setMenuRow2Callback(void (*fn)());
```
Registers a live-data callback drawn on row 2 of every 1- and 2-field submenu screen. Write to cols 0–16 only (cols 17–19 are the alert indicator). Not called during the 10 s countdown before menu timeout. Not called on 3-field screens.

### `addScreen()` — 1-field
```cpp
bool addScreen(ScreenSide side,
               const FieldDef& field1,
               void (*onSave)() = nullptr,
               const __FlashStringHelper* title = nullptr);
```

### `addScreen()` — 2-field (most common)
```cpp
bool addScreen(ScreenSide side,
               const FieldDef& field1,
               const FieldDef& field2,
               void (*onSave)() = nullptr,
               const __FlashStringHelper* title = nullptr);
```

### `addScreen()` — 3-field
```cpp
bool addScreen(ScreenSide side,
               const FieldDef& field1,
               const FieldDef& field2,
               const FieldDef& field3,
               void (*onSave)() = nullptr,
               const __FlashStringHelper* title = nullptr);
```

| Parameter | Description |
|-----------|-------------|
| `side` | `SCREEN_RIGHT` (knob1 clockwise) or `SCREEN_LEFT` (counter-clockwise) |
| `field1..3` | Built with factory helpers — see below |
| `onSave` | Called after operator presses SAVE; `nullptr` = no callback |
| `title` | Optional text on row 2 (suppressed if `setMenuRow2Callback` is set) |

Returns `false` if `APA_LCD_MAX_SCREENS` is already reached.

**SAVE vs BACK:** SAVE commits all edits and calls `onSave()`. BACK discards any in-progress edit — `onSave` is NOT called.

### `addTimerScreen()`
```cpp
bool addTimerScreen(ScreenSide side, void (*onSave)() = nullptr);
```
Registers an inline timer schedule screen with `APA_LCD_MAX_TIMERS` (default 3) on/off time slots.

**Important:** call `addTimerScreen()` AFTER all `addScreen()` calls on the same side. The timer screen always appears last in that side's rotation sequence.

| Parameter | Description |
|-----------|-------------|
| `side` | `SCREEN_RIGHT` or `SCREEN_LEFT` |
| `onSave` | Optional callback fired after SAVE is pressed. `nullptr` = EEPROM-only. |

Returns `false` if a timer screen is already registered.

**Screen layout:**
```
      01234567890123456789
Row 0:  T1: 08:00-09:30
Row 1: ►T2: 13:00-15:00    ← cursor on this row
Row 2:  T3: 00:00-00:00    ← 00:00-00:00 = disabled
Row 3: Total: 3h30m  >SAVE
```

**Editing a timer** (KB2 press on a timer row enters inline edit):
```
Row 1: ►T2:[08:00]09:00    ← rotating KB2 changes start time
Row 1: ►T2: 08:00[09:00]   ← KB2 press confirmed start; now editing end
```
Times change in 30-minute steps (00:00 → 23:30). KB2 press confirms; KB1 press cancels.

**SAVE:** writes all slots to EEPROM, fires optional `onSave()`, returns HOME.
**KB1 press:** returns HOME and discards all uncommitted edits.

**Timer EEPROM address:** `APA_LCD_TIMER_EEPROM_ADDR` (default 502), 7 bytes (addr 502–508).  
Timer data is loaded automatically on `begin()` — no user code required.

**Beginner path (no callback needed):**
```cpp
void setup() {
    gui.begin();
    gui.addHomeScreen(drawHome);
    gui.addScreen(SCREEN_RIGHT, ...);    // regular screens first
    gui.addTimerScreen(SCREEN_RIGHT);   // timer screen last; onSave = nullptr is fine
}
```

**Advanced path (callback for immediate response):**
```cpp
void onTimerSave() {
    uint16_t startMin = gui.getTimerStart(0);  // minutes from midnight
    uint16_t endMin   = gui.getTimerEnd(0);
    // update pump control state immediately
}
gui.addTimerScreen(SCREEN_RIGHT, onTimerSave);
```

**Pump control loop:**
```cpp
uint16_t nowMin = (uint16_t)hour * 60 + minute;
for (uint8_t i = 0; i < APA_LCD_MAX_TIMERS; i++) {
    if (gui.isTimerEnabled(i) &&
        nowMin >= gui.getTimerStart(i) &&
        nowMin <  gui.getTimerEnd(i)) {
        // run pump or light
    }
}
```

### `getTimerStart()` / `getTimerEnd()`
```cpp
uint16_t getTimerStart(uint8_t index) const;
uint16_t getTimerEnd(uint8_t index)   const;
```
Returns the start or end time of timer slot `index` as **minutes from midnight** (0 = 00:00, 1410 = 23:30).
Returns 0 if `index >= APA_LCD_MAX_TIMERS`.

### `isTimerEnabled()`
```cpp
bool isTimerEnabled(uint8_t index) const;
```
Returns `true` if timer slot `index` has a non-zero start or end time.
A slot where both start and end are 00:00 is considered disabled.
Returns `false` if `index >= APA_LCD_MAX_TIMERS`.

### `setRTC()`
```cpp
void setRTC(DS3231* rtc);   // only compiled when -DAPA_LCD_USE_DS3231 is set in build_flags
```
Wires the 800 ms both-buttons-hold gesture to the built-in two-step time/date edit modal.

Requires the `APA_LCD_USE_DS3231` build flag (PlatformIO `build_flags = -DAPA_LCD_USE_DS3231`; Arduino IDE `#define APA_LCD_USE_DS3231` before `#include <APALCDGUI.h>`). The library then pulls in `Wire.h` and `DS3231.h` automatically.

**RTC modal flow:**

```
Hold KB1 + KB2 for 800 ms  →  TIME screen  (HH : MM : SS)
  Knob2 rotates cursor between fields; press = enter edit; rotate = change; press = confirm
  SAVE  →  DATE screen  (DD / MM / YYYY)
    SAVE  →  writes all values to DS3231, returns HOME
    BACK  →  returns to TIME screen
  BACK  →  discards all changes, returns HOME
```

> Do NOT call `setBothPressedCallback()` when using `setRTC()` — the user callback always takes priority and will suppress the RTC modal.

---

## Field factory helpers

All factory functions return a `FieldDef` value. Pass directly inside `addScreen()`.

### `fieldInt()`
```cpp
static FieldDef fieldInt(const __FlashStringHelper* label,
                         const __FlashStringHelper* unit,
                         int16_t* val,
                         int16_t minVal, int16_t maxVal,
                         int16_t step = 1);
```
Integer field. Operator rotates to scroll between `minVal` and `maxVal` by `step`. Example: `fieldInt(F("ORP setpoint"), F("mV"), &orp, 400, 850, 10)`.

### `fieldFloat()`
```cpp
static FieldDef fieldFloat(const __FlashStringHelper* label,
                           const __FlashStringHelper* unit,
                           float* val,
                           float minVal, float maxVal,
                           float step, uint8_t decimals);
```
Float field displayed with a fixed number of decimal places.

| Parameter | Description |
|-----------|-------------|
| `label` | Field name, up to 12 chars, wrapped in `F()` |
| `unit` | 2-char suffix, wrapped in `F()`, or `nullptr` |
| `val` | Pointer to your `float` variable |
| `minVal` | Minimum allowed value (scroll floor) |
| `maxVal` | Maximum allowed value (scroll ceiling) |
| `step` | Change per encoder click — `0.01f` fine, `0.1f` medium, `1.0f` coarse |
| `decimals` | Digits after decimal point shown on LCD: `2` → `7.24` |

### `fieldChoice()`
```cpp
static FieldDef fieldChoice(const __FlashStringHelper* label,
                            uint8_t* idx,
                            const char** choices);
```
Cycles a `uint8_t` index through a null-terminated string array. Each string **must be exactly 4 characters** — the LCD value area is 4 columns wide. Pad shorter strings with trailing spaces.
```cpp
static const char* modes[] = {"AUTO", "MANU", "OFF ", nullptr};
fieldChoice(F("Cl mode"), &clMode, modes)
```

### `fieldBool()`
```cpp
static FieldDef fieldBool(const __FlashStringHelper* label, bool* val);
```
On/off toggle. Shows `" ON "` (true) or `"OFF "` (false). Operator presses to enter edit, rotates to toggle preview, presses again to commit.

### `fieldAction()`
```cpp
static FieldDef fieldAction(const __FlashStringHelper* label,
                            void (*action)() = nullptr,
                            bool confirm = false);
```
Button field. Shows `"STRT"` when `action` is non-null, `"-no-"` when nullptr. When the operator presses KB2 on an ACTION field, the cursor changes to `►` for 300 ms (click confirmation flash) then `action()` fires and the cursor jumps to SAVE. `confirm=true` shows a full-screen "Confirm action?" prompt (KB1=NO, KB2=YES) before calling `action` — use for irreversible operations.

### `fieldReadonly()`
```cpp
static FieldDef fieldReadonly(const __FlashStringHelper* label,
                              const __FlashStringHelper* unit,
                              float* val, uint8_t decimals);
```
Display-only float. Cursor skips this field automatically — knob2 moves straight to BACK or SAVE.

---

## Gesture callbacks

### `setLongPressCallback()`
```cpp
void setLongPressCallback(uint8_t encoder, void (*fn)());
```
Fires after 800 ms button hold.
- `encoder = 0` → right knob button (KB1 / knob1)
- `encoder = 1` → left knob button (KB2 / knob2)

**Built-in KB2 behaviour** (when no callback is registered for encoder 1): if a passive alert is active, shows the alert text as a 3-second overlay then clears it.

### `setBothPressedCallback()`
```cpp
void setBothPressedCallback(void (*fn)());
```
Fires when both buttons are pressed within 200 ms. Always takes priority over the RTC modal.

---

## Backlight

```cpp
void setBacklight(bool on);                  // force on/off immediately
void setBacklightTimeout(uint16_t seconds);  // 0 = always on; dim at 40% of this value
```

Default timeout: 300 s. Dim fires at 120 s, off at 300 s.

---

## Menu timeout

```cpp
void setMenuTimeout(uint16_t seconds);  // 0 = disabled; default 60 s
```

Returns to HOME after this many seconds without input. During the last 10 s, row 2 shows a countdown.

---

## Alerts — passive (non-blocking)

```cpp
void postAlert(const char* line1, const char* line2 = nullptr,
               AlertLevel level = ALERT_INFO);
void postAlert(const __FlashStringHelper* line1,
               const __FlashStringHelper* line2 = nullptr,
               AlertLevel level = ALERT_INFO);
void clearAlert();
bool hasAlert() const;
```

Shows a 3-character indicator in the top-right corner of every screen (cols 17–19 row 2):

| Level | Indicator | Behaviour |
|-------|-----------|-----------|
| `ALERT_INFO` | `[i]` | Steady |
| `ALERT_WARNING` | `[*]` | Steady |
| `ALERT_CRITICAL` | `[!]` | Flashing (500 ms) |

Navigation is not interrupted. `line1` / `line2` (up to 16 chars each) are stored: KB2 long-press (default) displays them as a 3-second overlay then calls `clearAlert()`. Use `F()` for both lines.

---

## Alerts — active (latching)

```cpp
void postActiveAlert(const char* line1, const char* line2,
                     AlertLevel level, void (*ackCallback)());
void postActiveAlert(const __FlashStringHelper* line1,
                     const __FlashStringHelper* line2,
                     AlertLevel level, void (*ackCallback)());
void cancelActiveAlert();
bool    hasActiveAlert()    const;
uint8_t activeAlertCount()  const;
```

Replaces the HOME screen with a full-screen alarm display until acknowledged:
```
row 0:  !!!!!  ALARM  !!!!!
row 1:  [line1 centred]
row 2:  [line2 centred]
row 3:           KB2:ACK !
```

KB2 press: fires `ackCallback()`, frees the slot, advances to the next queued alert. The operator can still navigate to submenus; returning to HOME always shows the alarm.

Queue holds `APA_LCD_ACTIVE_ALERT_QUEUE` slots (default 3). Excess alerts are silently dropped.

`cancelActiveAlert()` dismisses without calling `ackCallback` — use when the alarm condition cleared automatically.

---

## Message overlay

```cpp
void showMessage(const char* line1, const char* line2 = nullptr, uint16_t ms = 1500);
void clearMessage();
```

Covers all four rows with a timed message: `line1` is written to row 0, `line2` to row 1, rows 2–3 are blanked. Previous screen content restores automatically after `ms` milliseconds.

---

## Display control

```cpp
void markDirty();  // schedule a full LCD redraw on the next update()
```

Call when application data displayed on the current screen changes outside of a user edit.

---

## State queries

```cpp
bool    isMenuActive()    const;  // true when not at HOME
bool    isEditActive()    const;  // true during field, RTC, or timer edit
int8_t  currentScreen()   const;  // 0=HOME, +N=right screen N, -N=left screen N
uint8_t currentHomePage()  const;  // 0-based index of the currently displayed home page
uint8_t homePageCount()    const;  // number of home pages registered via addHomeScreen()
uint8_t getBrightness()    const;  // current backlight level (0–255, EEPROM-persisted)
```

`currentHomePage()` and `homePageCount()` let application code react to which dashboard page is visible — for example, to show a different indicator LED or enable a different data refresh. They are read-only; page selection is always by the operator (KB2 rotation).

---

## Enumerations

```cpp
enum ScreenSide  : uint8_t { SCREEN_RIGHT, SCREEN_LEFT };
enum AlertLevel  : uint8_t { ALERT_INFO, ALERT_WARNING, ALERT_CRITICAL };
enum FieldType   : uint8_t { FIELD_INT, FIELD_FLOAT, FIELD_CHOICE,
                              FIELD_BOOL, FIELD_ACTION, FIELD_READONLY };
```

---

## Custom character slots

| Slot | Constant | Character | Use |
|------|----------|-----------|-----|
| 0 | `CC_CURSOR_EDIT` | ► | Cursor in edit/confirm mode |
| 1–3 | — | →, ←, ↕ | Navigation indicators (internal) |
| 4 | `CC_DEGREE` | ° | Degree symbol — `lcd.write(CC_DEGREE)` in home callback |
| 5–6 | — | ↑, ↓ | RTC cursor indicators (internal) |
| 7 | — | free | Available for `lcd.createChar(7, ...)` |

---

## Configurable limits (define before `#include`)

```cpp
#define APA_LCD_MAX_SCREENS        4    // total submenu screens left+right (default 4)
#define APA_LCD_MAX_HOME_SCREENS   4    // home screen pages scrolled by KB2 (default 4)
#define APA_LCD_ACTIVE_ALERT_QUEUE 3    // simultaneous active alerts (default 3)
#define APA_LCD_MAX_TIMERS         3    // timer slots on the timer screen (default 3, max 3)
#define APA_LCD_EEPROM_ADDR      500    // brightness EEPROM base address (default 500, 2 bytes)
#define APA_LCD_TIMER_EEPROM_ADDR 502   // timer schedule EEPROM address (default 502, 7 bytes)
```

> **`APA_LCD_MAX_TIMERS`** — keep at 3 for this release. The 20×4 LCD has 4 rows; row 3 is always reserved for the SAVE row and total, leaving rows 0–2 for timer slots. A future version with scrolling support will lift this restriction.

**PlatformIO** — add to `build_flags`:
```ini
build_flags = -DAPA_LCD_MAX_SCREENS=8
```

**Arduino IDE** — add before `#include` in your sketch:
```cpp
#define APA_LCD_MAX_SCREENS 8
#include <APALCDGUI.h>
```

---

## Timing constants (defined in APALCDGUI.h — edit the library source to change)

| Constant | Default | Description |
|----------|---------|-------------|
| `APALCDGUI_MENU_TIMEOUT_MS` | 60 000 | Idle → HOME (ms) |
| `APALCDGUI_BL_OFF_MS` | 300 000 | Backlight off timeout (ms); dim at 40% |
| `APALCDGUI_LONG_PRESS_MS` | 800 | Long-press threshold (ms) |
| `APALCDGUI_BRIGHTNESS_DEFAULT` | 200 | Active PWM level (0–255) |
| `APALCDGUI_BRIGHTNESS_DIM` | 50 | Dim PWM level (0–255) |
| `APALCDGUI_BRIGHTNESS_STEP` | 10 | Change per click in brightness adjust |

---

## EEPROM layout

| Address | Bytes | Content |
|---------|-------|---------|
| 500 | 1 | Backlight brightness (0–255) |
| 501 | 1 | Validity marker (0xAE) |
| 502 | 1 | Timer 1 start slot (0–47; slot × 30 = minutes) |
| 503 | 1 | Timer 1 end slot |
| 504 | 1 | Timer 2 start slot |
| 505 | 1 | Timer 2 end slot |
| 506 | 1 | Timer 3 start slot |
| 507 | 1 | Timer 3 end slot |
| 508 | 1 | Timer validity marker (0xAF) |

Addresses 500–501 are always used. Addresses 502–508 are only written when `addTimerScreen()` is called.

On ESP32 / ESP8266: `EEPROM.begin()` and `EEPROM.commit()` are called automatically by the library.

---

*APALCDGUI — APA Devices · [kecup@vazac.eu](mailto:kecup@vazac.eu)*

---

## Platform notes

| Platform | `analogWrite()` | `EEPROM.update()` | `PROGMEM` / `pgm_read_byte()` |
|----------|----------------|-------------------|-------------------------------|
| AVR (Uno / Mega) | ✓ | ✓ | ✓ (strings in flash) |
| ESP32 | ✓ (LEDC) | ✗ — uses `write()` + `commit()` | No-op (strings in RAM) |
| ESP8266 | ✓ | ✗ — uses `write()` + `commit()` | No-op (strings in RAM) |
| STM32 (bluepill) | ✓ | ✓ | No-op (strings in RAM) |

The library handles all differences internally — no `#ifdef` needed in user code.
