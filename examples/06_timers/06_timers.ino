// APALCDGUI — Timer schedule screen
//
// Shows how to add a built-in timer schedule screen that lets the operator
// set up to 3 on/off time slots directly on the LCD — no extra screens or
// menus needed.
//
// What the timer screen looks like:
//   Row 0:  T1: 08:00-09:30
//   Row 1: ►T2: 13:00-15:00    ← cursor is on this row
//   Row 2:  T3: 00:00-00:00    ← 00:00-00:00 means disabled
//   Row 3: Total: 3h30m  >SAVE
//
// Editing a timer (operator presses KB2 on any row):
//   Row 1: ►T2:[08:00]09:00    ← rotating KB2 changes the start time
//   Row 1: ►T2: 08:00[09:00]   ← after confirming start, end is selected
//   Times change in 30-minute steps. Press KB1 to cancel, KB2 to confirm.
//
// Controls on the timer screen:
//   KB2 rotate:   move cursor between T1 / T2 / T3 / SAVE
//   KB2 press on timer row: enter inline edit (start time first, then end)
//   KB2 press on SAVE:      write to EEPROM, fire optional callback, return HOME
//   KB1 press:              return HOME and discard uncommitted edits
//   (Brightness adjust and backlight timeout work the same as on any other screen)
//
// ---- Registration order (important) ----------------------------------------
//   Call addTimerScreen() AFTER all addScreen() calls on the same side.
//   The timer screen always sits at the end of that side's rotation sequence.
//
// ---- EEPROM -----------------------------------------------------------------
//   Times are auto-saved on SAVE and auto-loaded on begin().
//   Default address: APA_LCD_TIMER_EEPROM_ADDR = 502 (7 bytes, addr 502–508).
//   Do not overlap with other libraries that use EEPROM addresses near 500.
//
// ---- Extending to up to 6 timer slots --------------------------------------
//   Add before #include:
//     #define APA_LCD_MAX_TIMERS 6
//   The EEPROM layout and scroll render expand automatically.
//   The LCD shows 3 slots at a time; ↑/↓ indicators appear when the list scrolls.
//
// ---- Controls (hardware) ----------------------------------------------------
//   Right knob (KB1) — rotate: navigate between screens
//                      hold + rotate: adjust backlight brightness
//   Left  knob (KB2) — rotate on HOME: (not used, single home page)
//                      rotate on timer: move cursor or change time
//                      press: enter edit or confirm
//                      long press (800 ms): show passive alert text, then dismiss

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Simulated RTC (replace with real RTC reads in your project) ----------------
uint8_t g_hour   = 14;
uint8_t g_minute = 23;

// ---- Pump control state --------------------------------------------------------
bool g_pumpOn = false;

// ---- Home screen ---------------------------------------------------------------
void drawHome(LiquidCrystal& lcd) {
    char buf[24];  // 24 bytes: headroom for ESP8266 -Wformat-truncation

    // Row 0: clock
    snprintf(buf, sizeof(buf), "  Time: %02d:%02d       ", g_hour, g_minute);
    lcd.setCursor(0, 0); lcd.print(buf);

    // Row 1: pump state
    lcd.setCursor(0, 1);
    lcd.print(g_pumpOn ? "  Pump: ON          " : "  Pump: OFF         ");

    // Row 2: active timer hint (cols 0-16; alert indicator owns 17-19)
    lcd.setCursor(0, 2);
    lcd.print("  KB1: settings   ");

    // Row 3: nothing — page indicator would appear here if > 1 home page
}

// ---- Settings screen: clock ---------------------------------------------------
int16_t g_setHour   = 14;
int16_t g_setMinute = 23;

void onClockSave() {
    g_hour   = (uint8_t)g_setHour;
    g_minute = (uint8_t)g_setMinute;
}

// ---- Optional: onSave callback fires after operator presses SAVE on timer screen
// Use it to immediately update pump state from the new schedule.
// Not required — times are in EEPROM and readable via getTimerStart/End at any time.
void onTimerSave() {
    // Re-evaluate pump state immediately after new schedule is saved
    uint16_t nowMin = (uint16_t)g_hour * 60 + g_minute;
    g_pumpOn = false;
    for (uint8_t i = 0; i < APA_LCD_MAX_TIMERS; i++) {
        if (gui.isTimerEnabled(i) &&
            nowMin >= gui.getTimerStart(i) &&
            nowMin <  gui.getTimerEnd(i)) {
            g_pumpOn = true;
        }
    }
    gui.markDirty();
}

// ---- Pump control loop — call this periodically (e.g. every minute) ------------
void checkPumpSchedule() {
    uint16_t nowMin = (uint16_t)g_hour * 60 + g_minute;
    bool shouldRun  = false;
    for (uint8_t i = 0; i < APA_LCD_MAX_TIMERS; i++) {
        if (gui.isTimerEnabled(i) &&
            nowMin >= gui.getTimerStart(i) &&
            nowMin <  gui.getTimerEnd(i)) {
            shouldRun = true;
        }
    }
    if (shouldRun != g_pumpOn) {
        g_pumpOn = shouldRun;
        gui.markDirty();
        // digitalWrite(PUMP_PIN, g_pumpOn ? HIGH : LOW);
    }
}

void setup() {
    gui.begin();

    gui.addHomeScreen(drawHome);

    // Regular submenu screen: clock adjustment (right side, screen 1)
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldInt(F("Hour"),   F("h"),  &g_setHour,   0, 23, 1),
        APALCDGUI::fieldInt(F("Minute"), F("m"),  &g_setMinute, 0, 59, 1),
        onClockSave,
        F("Clock"));

    // Timer screen: always register AFTER addScreen() calls on the same side.
    // The onTimerSave callback is optional — the schedule is still saved to EEPROM
    // and readable via getTimerStart/End even without it.
    gui.addTimerScreen(SCREEN_RIGHT, onTimerSave);

    // Check schedule on startup so pump state is correct before first minute tick.
    checkPumpSchedule();
}

void loop() {
    gui.update();

    // Check schedule once per simulated minute
    static uint32_t lastMinMs = 0;
    if (millis() - lastMinMs >= 60000UL) {
        lastMinMs = millis();
        g_minute++;
        if (g_minute >= 60) { g_minute = 0; g_hour = (g_hour + 1) % 24; }
        checkPumpSchedule();
    }
}
