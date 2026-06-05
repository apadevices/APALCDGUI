# Changelog

## [1.4.0] — 2026-06-05

### Added

- `setStatusIndicator(char c)` / `clearStatusIndicator()` — show a single-character status indicator `[c]` in cols 17–19 row 2 when no passive alert is pending. Alert indicators always take priority: `[!]` > `[*]` > `[i]` > status indicator > blank. SRAM cost: 1 byte. Intended use: external libraries set e.g. `'M'` for pump manual-mode indication.
- `getTimerTotalMinutes()` — returns the sum of all enabled timer slot durations in minutes. Used as a `dailyTargetCb` bridge for APAPUMP: `pump.begin(scheduleActive, nullptr, []() { return gui.getTimerTotalMinutes(); })`

## [1.3.0] — 2026-06-02

### Added

- Scrollable timer list: `APA_LCD_MAX_TIMERS` now supports up to 6 slots (previously fixed at 3)
- Viewport scrolling: LCD shows 3 slots at a time; rotating KB2 past the visible window scrolls the list automatically
- Scroll indicators: ↑ at col 19 row 0 when slots exist above the viewport; ↓ at col 19 row 1 when slots exist below
- `viewTop` packed into existing `_timerBits` byte (3 bits, zero SRAM cost)
- Auto-advance after editing the last visible slot scrolls the viewport to keep the next slot in view

### Changed

- `APA_LCD_MAX_TIMERS` default remains 3; define it as up to 6 before `#include` to use more slots
- EEPROM timer area scales automatically: `2 × APA_LCD_MAX_TIMERS + 1` bytes starting at `APA_LCD_TIMER_EEPROM_ADDR` (13 bytes for 6 slots vs 7 bytes for 3)
- Entering the timer screen always resets the viewport to slot 1 (top of list)
- License changed from MIT to dual license: free for personal/educational/hobby use; commercial use requires a separate written license (contact [kecup@vazac.eu](mailto:kecup@vazac.eu))
- Logo added to README, centered; README version badge corrected

## [1.2.3] — 2026-06-02

### Changed

- Added logo image to README (`extras/apalcdgui.png`, 600 px wide)
- Fixed README version badge URL — was showing 1.2.0 instead of the current version

## [1.2.2] — 2026-06-01

### Added

- `ST_FLASH_ACTION` state: pressing KB2 on an ACTION field now flashes `►` on that row for 300 ms before firing the callback, giving the same click-confirmation feedback as SAVE and BACK
- Cursor convention now consistent across all interactive elements: `>` = pointer (where cursor is), `►` = click confirmed (action in progress)

### Fixed

- Brightness gesture now requires KB1 to be held for 800 ms before rotation activates brightness adjust; previously any rotation while KB1 was held immediately entered brightness mode, causing accidental triggers during normal navigation
- ACTION field cursor advances to SAVE after the flash, matching the commit behaviour of editable fields
- `mega/02_alerts` example: home screen incorrectly said "KB1 long" for the gesture that clears passive alerts — corrected to "KB2 long"
- `mega/02_alerts` example: active alert trigger functions now show a `showMessage` overlay ("Alert N queued / Go HOME to ACK") so the operator has immediate feedback that the alert was posted
- `mega/01_8screens` example: `platformio.ini` was missing `build_flags = -DAPA_LCD_MAX_SCREENS=8`; left screens were silently dropped and CCW rotation did nothing

## [1.2.1] — 2026-06-01

### Fixed

- Backlight brightness mode now activates immediately at the 800 ms KB1 long-press threshold, showing the brightness screen before any rotation is detected (previously the screen only appeared after the first encoder click)
- Timer screen cursor now shows `>` in navigation mode and `►` only during active field editing, consistent with regular menu screens
- Row 3 content from the timer screen ("Total … SAVE") and brightness screen no longer lingers on the home screen after returning; `_renderHome` clears row 3 before calling the user callback
- KB1 cancel in timer edit now correctly restores both the start and end slot to their pre-edit values when pressing KB1 after the start time was already committed
- `_stateBrightness` now calls `_touchInput()` on rotation so the menu timeout is properly reset during brightness adjustment
- Dead else-branch removed from `_renderActionBar` (`_scrPos == 0` is unreachable in that context)
- `pgbuf[4]` in `_renderHome` widened to `pgbuf[8]` to cover the full `uint8_t` range when `APA_LCD_MAX_HOME_SCREENS` > 9
- `showMessage()` documentation corrected: covers rows 0 and 1 (not rows 1 and 2), and blanks all four rows

## [1.2.0] — 2026-06-01

### Added

- Timer schedule screen: `addTimerScreen(side, onSave)` registers an inline on/off schedule screen with up to `APA_LCD_MAX_TIMERS` (default 3) time slots
- Each timer slot stores a start and end time in 30-minute steps (00:00–23:30); both at 00:00 means disabled
- `getTimerStart(i)` / `getTimerEnd(i)` return minutes from midnight (0–1410) for pump/light control logic
- `isTimerEnabled(i)` returns true when a slot has a non-zero start or end time
- Timer data auto-loaded from EEPROM on `begin()` and auto-saved to EEPROM when operator presses SAVE — no callback required for basic use
- Optional `onSave` callback fires after each SAVE press for immediate application-side response
- `APA_LCD_MAX_TIMERS` define (default 3) controls slot count — override before `#include`
- `APA_LCD_TIMER_EEPROM_ADDR` define (default 502) sets the 7-byte EEPROM block address
- Two new states: `ST_TIMER` (cursor navigation) and `ST_TIMER_EDIT` (inline field editing)
- `isEditActive()` now returns true in `ST_TIMER_EDIT`
- New example `examples/06_timers/` — timer screen with pump schedule control loop
- `keywords.txt` updated: `addTimerScreen`, `getTimerStart`, `getTimerEnd`, `isTimerEnabled`, `APA_LCD_MAX_TIMERS`, `APA_LCD_TIMER_EEPROM_ADDR`

### Fixed

- KB2 (left knob) rotation on the home screen now wakes the backlight regardless of home page count — previously `_touchInput()` was only called when `_nHomeCbs > 1`, so on single-page setups KB2 rotation did not restore brightness from DIM or OFF

### Changed

- `APALCDGUI_VERSION` bumped to `"1.2.0"`
- State count: 9 → 11
- EEPROM footprint: +7 bytes at address 502–508 (timer schedule)

---

## [1.1.6] — 2026-05-31

### Added

- `keywords.txt` — IDE keyword colouring for all public methods, types, and constants

### Fixed

- `library.properties` `sentence` and `paragraph` trailing periods removed — Arduino Library Manager appends its own period; double period was appearing in the registry

### Changed

- `APALCDGUI_VERSION` bumped to `"1.1.6"`

---

## [1.1.5] — 2026-05-31

### Added

- `examples/05_multi_home/` — three-page scrollable home screen example; demonstrates `addHomeScreen()`, automatic page indicator, and the column 17–19 layout constraint across all 6 field types and 8 screens

### Changed

- `APALCDGUI_VERSION` bumped to `"1.1.5"`
- Platform Verification table in README updated from 02_8screens to 05_multi_home numbers

---

## [1.1.4] — 2026-05-31

### Added

- Multiple home screen pages: `addHomeScreen(fn)` — call multiple times to register up to `APA_LCD_MAX_HOME_SCREENS` (default 4) pages; KB2 rotation on HOME cycles between them
- Automatic page indicator at row 3 cols 17–19: the library draws `n/N` after the home callback returns when more than one page is registered — no user code required
- `setHomeCallback(fn)` unchanged as an alias for `addHomeScreen()` — existing single-page sketches compile and run without modification
- `currentHomePage()` / `homePageCount()` query getters added
- `APA_LCD_MAX_HOME_SCREENS` compile-time capacity define (default 4)
- Author attribution added to `README.md` and `docs/API.md`
- `APALCDGUI_VERSION` bumped to `"1.1.4"`

---

## [1.1.3] — 2026-05-31

### Fixed

- Active alert queue: `postActiveAlert()` now resets `_alertHead` to the new slot when the queue was previously empty — prevents stale head from causing `_renderAlertScreen()` to display from a wrong (freed) slot after all alerts were acknowledged
- `showMessage()` — added `const __FlashStringHelper*` overload; internal call `showMessage("Settings saved!")` changed to `showMessage(F("Settings saved!"))` — saves 16 bytes of SRAM on AVR (string literal no longer copied to data segment)
- 4 × `char blank[21]; memset(...)` stack allocations replaced with `_rowWrite(r, "")` — `_rowWrite` already space-pads to COLS, so an empty string produces an identical blank row with no buffer needed
- Custom character slot comment in header corrected: slots 0–6 are library-internal (slot 6 ↓ is used by `_renderActionBar` for the first-screen navigation indicator); only slot 7 is free for user characters
- `APALCDGUI_VERSION` bumped to `"1.1.3"`

---

## [1.1.2] — 2026-05-31

### Added

- `examples/04_rtc/04_rtc.ino` — DS3231 RTC modal example with live clock on home screen
- `examples/02_8screens/02_8screens.ino` — enhanced to demonstrate all 6 field types: `fieldBool` added to L-1 (CHOICE + CHOICE + BOOL), `fieldReadonly` and `fieldAction` already present in later screens

### Fixed

- DS3231 integration now uses explicit build flag `-DAPA_LCD_USE_DS3231` (PlatformIO `build_flags` / Arduino IDE `#define` before `#include`) — the previous include-order approach failed in PlatformIO because `APALCDGUI.cpp` is compiled in a separate translation unit and cannot see the sketch's includes
- RTC modal gesture changed to 800 ms both-buttons hold (`APALCDGUI_LONG_PRESS_MS`) — the previous 200 ms threshold (`APALCDGUI_BOTH_PRESS_MS`) fired too easily during normal navigation; `_bothFired` flag prevents double-fire against individual long-press callbacks
- RTC modal UX redesigned: TIME screen → SAVE → DATE screen → SAVE (write all, return HOME) sequential flow; BACK on DATE returns to TIME; BACK on TIME discards everything; removed Knob1 sub-screen toggle which was unreliable in practice
- RTC cursor indicator: single ↑ below selected field only (slot 5); all other positions blank — previous design showed ↑ on every unselected field and ↓ on the selected one, which operators found confusing

### Changed

- Example folder restructured: flat `examples/01_minimal`, `02_8screens`, `03_alerts`, `04_rtc` — replaces the `examples/mega/` category subfolder which caused Arduino IDE and GitHub to hide the sketches behind an extra navigation level
- All 4 examples updated with platform-neutral wiring notes (removed "Arduino Mega 2560" from sketch titles and wiring tables where not strictly necessary)

---

## [1.1.1] — 2026-05-30

### Added

- `fieldBool()` — on/off toggle field; shows `" ON "` / `"OFF "`, rotate to preview, press to commit
- `fieldAction()` — button field; shows `"STRT"` when action is set, `"-no-"` when nullptr; optional `confirm=true` adds full-screen confirmation prompt (KB1=NO, KB2=YES) before firing the callback
- Variable field count per screen: `addScreen()` now accepts 1, 2, or 3 fields via overloads
- `CONFIRM` state (9th state) for `fieldAction(confirm=true)` — press KB1 to cancel, KB2 to execute
- Screen title parameter on `addScreen()` — optional `const __FlashStringHelper*` shown on row 2 when no `setMenuRow2Callback` is registered
- `setMenuRow2Callback()` — live-data callback drawn on row 2 of every 1- and 2-field screen (cols 0–16)
- `markDirty()` — force a full LCD redraw on the next `update()` when application data changes outside a user edit
- `currentScreen()` — returns 0 (HOME), +N (right screen N), or -N (left screen N)
- `getBrightness()` — returns current backlight PWM level (0–255)
- `fieldReadonly()` now skips the cursor automatically — knob2 jumps straight to BACK / SAVE
- `CC_DEGREE` constant (slot 4) documented and exported — use `lcd.write(CC_DEGREE)` in home callback for ° symbol

### Fixed

- `EEPROM.update()` is AVR-only — ESP32 and ESP8266 now use `EEPROM.write()` + `EEPROM.commit()` via `#if defined(ESP32) || defined(ESP8266)` guard in `_saveEEPROM()`
- `vbuf` buffer in `_drawField()` enlarged from 5 to 8 bytes — `snprintf` with `%4d` into `int16_t` worst case (`-32768`) overflowed the previous 5-byte buffer
- Alert screen `hdr` buffer enlarged from `COLS+1` to `COLS+4` — format string `"!! ALARM  [1 of %d] !"` produces 21 chars + NUL, overflowing the previous 21-byte buffer
- `(unsigned long)` cast on `uint32_t` uptime variable in 01_8screens example — `uint32_t` is `unsigned int` (not `unsigned long`) on ESP8266; `%lu` format triggered a compiler warning
- Private member comment for `_enc[2]` corrected: `[0]` is the right knob (KB1, navigation), `[1]` is the left knob (KB2, edit) — was reversed

### Changed

- `LiquidCrystal` is now a direct class member initialised in the constructor (no heap allocation, no `<new>` dependency)
- `library.properties` paragraph reworded to "Arduino-compatible boards (AVR, ESP32, ESP8266, STM32)" — removed platform-specific "Arduino Mega 2560" language
- `docs/API.md` completely rewritten — previous content described an obsolete I2C 16×2 prototype; new document covers the full v1.1.1 API
- `README.md` updated: Platform Verification table added with RAM/Flash numbers for all 5 supported platforms; "Understanding Field Factory Parameters" section added with annotated diagram and parameter table; 8 factual corrections (state count, knob direction labels, missing methods)
- Verified zero errors, zero warnings on all 5 supported platforms with `APA_LCD_MAX_SCREENS=8`:
  Mega 2560 (1 678 B RAM / 19 446 B Flash), Uno (1 666 B / 17 546 B), ESP32 (23 916 B / 297 729 B), ESP8266 D1 Mini (30 536 B / 282 267 B), STM32 Bluepill (3 988 B / 36 800 B)

---

## [1.0.0] — 2026-05-27

### Added

- Parallel 4-bit LiquidCrystal driver for 20×4 LCD — replaces old I2C 16×2 prototype entirely
- Native quadrature ISR decoding for two PEC11R rotary encoders — 4-bit state machine, no library dependency
- Non-blocking 8-state machine: HOME, NAV, EDIT, FLASH_SAVE, FLASH_BACK, BRIGHTNESS, RTC_NAV, RTC_EDIT
- `begin()` with configurable pin defaults matching APA Devices HMI board v1.0
- `addScreen()` — register up to 4 submenu screens (configurable via `APA_LCD_MAX_SCREENS`)
- `setHomeCallback()` — user-supplied home screen draw function (v1.1+: multiple/scrolled dashboards)
- Factory functions: `fieldInt()`, `fieldFloat()`, `fieldChoice()`, `fieldAction()`, `fieldReadonly()`
- Two-tier alert system:
  - Passive (corner indicator): `postAlert()`, `clearAlert()`, `hasAlert()`
  - Active (home screen takeover, queue of 3): `postActiveAlert()`, `cancelActiveAlert()`, `hasActiveAlert()`, `activeAlertCount()`
- PWM backlight with ACTIVE → DIM → OFF stages; dim at 40% of off timeout
- Brightness adjusted via KB1-hold + knob1 rotate, persisted in EEPROM at address 500–501
- `setBacklightTimeout()`, `setBacklight()`, `setMenuTimeout()`
- Optional DS3231 RTC modal (compiled out if `DS3231.h` not included): inline HH:MM:SS and DD/MM/YYYY edit
- `setLongPressCallback()`, `setBothPressedCallback()`
- `showMessage()` / `clearMessage()` — timed overlay
- `isMenuActive()`, `isEditActive()` — state queries for user code
- 7 custom LCD characters: ► (EDIT cursor), → ← ↕ (navigation), ° (degree), ↑ ↓ (RTC cursor)
- `F()` macro throughout — all string literals in flash; `_fstr()` helper with `#ifdef __AVR__` guard
- EEPROM platform guards for ESP32 / ESP8266 (`EEPROM.begin()`, `EEPROM.commit()`)
- Verified clean compile (zero errors, zero warnings) on Mega 2560, Uno, ESP32
  - Mega: 708 B RAM (8.6%), 14430 B Flash (5.7%) for minimal example
