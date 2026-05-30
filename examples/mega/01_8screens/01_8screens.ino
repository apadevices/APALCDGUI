// APALCDGUI — Mega 2560 comprehensive demo — all field types, 8 screens
//
// This sketch shows every feature the library offers and is the best starting
// point for understanding the full API.
//
// ---- Field types demonstrated -----------------------------------------------
//   FIELD_INT      — integer value scrolled between min and max  (R+1 ORP, R+2 both)
//   FIELD_FLOAT    — float value with configurable decimal places (R+1 pH, R+3, R+4)
//   FIELD_CHOICE   — cycles a string array of named options       (L-1, L-2)
//   FIELD_READONLY — display only, cursor skips it automatically  (L-2 FW ver, L-3 both)
//   FIELD_ACTION   — button that fires a callback when SAVE is pressed (L-4)
//
// ---- Callback types used ----------------------------------------------------
//   setHomeCallback(fn)         — fn(LiquidCrystal& lcd)  ← THE ONLY callback with a parameter
//   setMenuRow2Callback(fn)     — fn()  called on every menu update to draw live sensor data
//   addScreen(..., onSave)      — onSave() called after the operator presses SAVE
//   postActiveAlert(..., ackCb) — ackCb() called when the operator acknowledges the alarm
//   setLongPressCallback(n, fn) — fn() called when an encoder button is held 800 ms
//   setBothPressedCallback(fn)  — fn() called when both buttons are pressed together
//   fieldAction(label, fn)      — fn() called when SAVE is pressed on that ACTION field
//
// ---- SAVE vs BACK -----------------------------------------------------------
//   SAVE — commits every field edit on this screen, calls onSave(), shows "Settings saved!".
//   BACK — discards any in-progress edit and returns to navigation. onSave is NOT called.
//
// ---- Wiring -----------------------------------------------------------------
//   APA Devices HMI board v1.0 — all default pins, no arguments needed to begin().
//
// ---- Controls ---------------------------------------------------------------
//   Right knob (knob1 / KB1) — rotate: navigate between screens
//                               hold KB1 + rotate: adjust brightness (row 3 shows hint)
//   Left  knob (knob2 / KB2) — rotate: move cursor / change value when editing
//                               press: enter edit mode or confirm
//                               long press (800 ms): show passive alert text then dismiss it

// ---- Capacity override — MUST come before #include --------------------------
// The default maximum is 4 screens. This sketch uses 8.
// PlatformIO: set via build_flags = -D APA_LCD_MAX_SCREENS=8 in platformio.ini.
// Arduino IDE: add the line below BEFORE the #include line in your sketch.

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Simulated sensor readings (in a real system these come from APADOSE/APAPHX2) ----
float  g_ph       = 7.24f;  // current pH
int16_t g_orp     = 682;    // current ORP mV
float  g_temp     = 26.3f;  // water temperature °C
float  g_cl       = 1.2f;   // chlorine ppm
float  g_uptime   = 0.0f;   // hours since power-on
float  g_doseToday = 0.0f;  // ml dosed today

// ---- User-settable parameters (knob2 edits these in the menu) ----
// RIGHT screen 1: pool chemistry targets
float   r1_phSetpoint  = 7.20f; // pH target
int16_t r1_orpSetpoint = 680;   // ORP target mV

// RIGHT screen 2: filtration & pool volume
int16_t r2_filterHours = 8;     // filtration hours per day (1-24)
int16_t r2_poolVolume  = 25;    // pool volume in m3 (10-90)

// RIGHT screen 3: temperature limits
float r3_maxPoolTemp    = 30.0f; // max pool temperature °C
float r3_solarSetpoint  = 45.0f; // solar absorber setpoint °C

// RIGHT screen 4: dosing parameters
float r4_phDoseRate = 15.0f;  // pH solution dose ml/m3
float r4_clDoseRate = 20.0f;  // Cl solution dose ml/m3

// LEFT screen 1: pH direction (plus or minus) — CHOICE field
uint8_t l1_phDirection = 0;  // 0=MINUS, 1=PLUS
uint8_t l1_clMode      = 0;  // 0=AUTO,  1=MANUAL, 2=OFF
// CHOICE strings MUST be exactly 4 characters — the LCD value area is 4 columns wide.
// Pad shorter strings with trailing spaces so they fill all 4 columns cleanly.
// "OFF" (3 chars) becomes "OFF " (4 chars with one trailing space).
static const char* phDirChoices[] = {"MINS", "PLUS", nullptr};
static const char* clModeChoices[]= {"AUTO", "MANU", "OFF ", nullptr};

// LEFT screen 2: display mode — CHOICE + READONLY placeholder
uint8_t l2_dispMode = 0;      // 0=NORMAL, 1=EXTENDED
static const char* dispModeChoices[] = {"NORM", "EXT ", nullptr};
float   l2_fwVersion = 1.00f; // shown as read-only (not editable)

// LEFT screen 3: system status — two READONLY fields
// (values updated in loop() from the simulated sensors)

// LEFT screen 4: pump manual test — two ACTION fields

// ---- Save callbacks — called when operator presses SAVE on that screen ----

void onChemSave() {
    // r1_phSetpoint and r1_orpSetpoint are already updated in the variables.
    // In a real system: APADOSE.setSetpoint(r1_phSetpoint); EEPROM.write(...);
}

void onFiltSave() {
    // r2_filterHours and r2_poolVolume updated.
    // In a real system: APADOSE.setPoolVolume(r2_poolVolume);
}

void onTempSave() {
    // r3_maxPoolTemp and r3_solarSetpoint updated.
}

void onDoseSave() {
    // r4_phDoseRate and r4_clDoseRate updated.
}

void onModeSave() {
    // l2_dispMode updated.
}

// ---- ACTION callbacks — fired immediately when SAVE is pressed on an ACTION field ----

void runPhPumpTest() {
    // In a real system: pulse the pH pump for 2 seconds.
    // Here: show a temporary message so we can verify ACTION fires correctly.
    gui.showMessage("pH pump TEST", "2 s pulse fired", 2000);
}

void runClPumpTest() {
    gui.showMessage("Cl pump TEST", "2 s pulse fired", 2000);
}

void triggerPassiveAlert() {
    // Post a passive alert — shows corner indicator on all screens, doesn't block navigation.
    gui.postAlert(F("Test warning"), F("Passive alert OK"), ALERT_WARNING);
}

void triggerActiveAlert() {
    // Post an active alert — replaces home screen, requires KB2 press to acknowledge.
    gui.postActiveAlert(
        F("TEST: Active alarm"),
        F("Press KB2 to ACK"),
        ALERT_CRITICAL,
        []() {
            // ackCallback: called when operator presses KB2 on the alert screen.
            gui.showMessage("Alert ACK'd", "System OK", 1500);
        }
    );
}

// ---- Long-press and both-press gestures ------------------------------------

void onKb2LongPress() {
    // KB2 long press: dismiss passive alert without leaving the current screen.
    gui.clearAlert();
    gui.showMessage("Alert cleared", nullptr, 1000);
}

void onBothPressed() {
    // Both buttons simultaneously: show a message confirming the gesture fires.
    // In production this is where you'd wire the RTC modal (gui.setRTC(&rtc)).
    gui.showMessage("Both pressed!", "Use setRTC() here", 1500);
}

// ---- Home screen callback --------------------------------------------------
// setHomeCallback() is the only callback that receives a parameter (LiquidCrystal&).
// All other callbacks — onSave, ackCallback, long-press, action — are void(void).
//
// This function is called on every update() while the operator is at HOME.
// Keep it fast: no delay(), no blocking calls. It runs every loop() iteration.
//
// The lcd parameter is a reference to the library's LiquidCrystal object.
// You can write directly to it without needing a separate global variable.
//
// AVR note: snprintf() on AVR does NOT support %f for floats.
// Always use dtostrf() to convert floats to strings on Arduino Uno / Mega.

void drawHome(LiquidCrystal& lcd) {
    // AVR snprintf does not support %f — use dtostrf for all float values.
    char fa[5], fb[5]; // dtostrf scratch (4 chars + NUL)
    char buf[21];

    // Row 0: pH and ORP  "pH 7.24  ORP 682mV  "
    dtostrf(g_ph, 4, 2, fa);
    snprintf(buf, sizeof(buf), "pH%s  ORP%4dmV  ", fa, g_orp);
    lcd.setCursor(0, 0); lcd.print(buf);

    // Row 1: Cl and temp  "Cl 1.2ppm Tmp26.3°  "
    // ° (custom char slot 4) placed at col 17; cols 18-19 left as spaces
    dtostrf(g_cl,   4, 1, fa);
    dtostrf(g_temp, 4, 1, fb);
    snprintf(buf, sizeof(buf), "Cl%sppm Tmp%s", fa, fb); // 17 chars
    lcd.setCursor(0, 1); lcd.print(buf);
    lcd.setCursor(17, 1); lcd.write(CC_DEGREE);  // ° custom char slot defined in APALCDGUI.h
    lcd.setCursor(18, 1); lcd.print(F("  "));

    // Row 2: uptime (integer hours) — col 19 left free for passive alert indicator
    uint32_t upH = (uint32_t)g_uptime;
    snprintf(buf, sizeof(buf), "Filt OK  Up%6luh  ", (unsigned long)upH);
    lcd.setCursor(0, 2); lcd.print(buf);

    // Row 3: quick guide for new operators
    lcd.setCursor(0, 3); lcd.print(F("K1:screens K2:edit  "));
}

// ---- setup() ---------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    Serial.println(F("APALCDGUI 8-screen Mega test starting..."));

    // All pin defaults match APA Devices HMI board v1.0 — no arguments needed.
    gui.begin();

    gui.setHomeCallback(drawHome);

    // Optional: shorten timeouts for faster bench testing.
    // gui.setMenuTimeout(30);        // return to HOME after 30 s idle (default 60 s)
    // gui.setBacklightTimeout(60);   // dim at 24 s, off at 60 s (default 300 s)

    gui.setLongPressCallback(1, onKb2LongPress);  // 1 = KB2 (left knob) long press
    gui.setBothPressedCallback(onBothPressed);    // both buttons pressed within 200 ms

    // ---- RIGHT screens: rotate knob1 clockwise to reach these ----

    // RIGHT +1: Pool chemistry — pH setpoint (FLOAT) + ORP setpoint (INT)
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH setpoint"), F("pH"), &r1_phSetpoint, 6.8f, 7.8f, 0.01f, 2),
        APALCDGUI::fieldInt(  F("ORP setpoint"), F("mV"), &r1_orpSetpoint, 400, 850, 10),
        onChemSave
    );

    // RIGHT +2: Filtration — filter hours per day (INT) + pool volume (INT)
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldInt(F("Filt hrs/day"), F("h"),  &r2_filterHours, 1, 24, 1),
        APALCDGUI::fieldInt(F("Pool volume"),    F("m3"), &r2_poolVolume,  10, 90, 1),
        onFiltSave
    );

    // RIGHT +3: Temperature limits — two FLOAT fields
    // The degree symbol ° is a custom character at slot CC_DEGREE (slot 4).
    // In a unit string: F("\x04""C") prints °C on the LCD.
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("Max pool tmp"),  F("\x04""C"), &r3_maxPoolTemp,   15.0f, 40.0f, 0.5f, 1),
        APALCDGUI::fieldFloat(F("Solar setpt"),   F("\x04""C"), &r3_solarSetpoint,  20.0f, 70.0f, 1.0f, 1),
        onTempSave
    );

    // RIGHT +4: Dosing rates — two FLOAT fields
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH dose rate"),  F("ml"), &r4_phDoseRate, 1.0f, 50.0f, 0.5f, 1),
        APALCDGUI::fieldFloat(F("Cl dose rate"),  F("ml"), &r4_clDoseRate, 1.0f, 50.0f, 0.5f, 1),
        onDoseSave
    );

    // ---- LEFT screens: rotate knob1 counter-clockwise to reach these ----

    // LEFT -1: Chemical configuration — pH direction (CHOICE) + Cl mode (CHOICE)
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(F("pH direction"), &l1_phDirection, phDirChoices),
        APALCDGUI::fieldChoice(F("Cl mode"),      &l1_clMode,      clModeChoices)
        // no onSave: changes apply immediately when SAVE is pressed (no callback needed here)
    );

    // LEFT -2: CHOICE + READONLY on the same screen.
    // knob2 automatically skips the READONLY field — the cursor jumps straight to BACK/SAVE.
    // This is useful for showing a firmware version or calibration date alongside an editable field.
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(  F("Display mode"), &l2_dispMode, dispModeChoices),
        APALCDGUI::fieldReadonly(F("FW version"),    F("  "), &l2_fwVersion, 2),
        onModeSave
    );

    // LEFT -3: System status — both READONLY, cursor skips both.
    // No onSave callback needed because there is nothing to commit.
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldReadonly(F("Uptime"),     F("h"),  &g_uptime,    1),
        APALCDGUI::fieldReadonly(F("Dose today"), F("ml"), &g_doseToday, 0)
    );

    // LEFT -4: Pump manual test — ACTION fields.
    // action != nullptr shows "STRT" in the value area; pressing SAVE fires the callback.
    // action == nullptr shows "-no-" and does nothing on SAVE.
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldAction(F("pH pump test"), runPhPumpTest),
        APALCDGUI::fieldAction(F("Cl pump test"), runClPumpTest)
    );

    // Done — library is ready.
    Serial.println(F("Setup complete. Rotating knob1 shows 8 screens."));
    Serial.println(F("  knob1 right: pH chemistry / filtration / temperature / dosing"));
    Serial.println(F("  knob1 left:  chem config / display / status / pump tests"));
}

// ---- loop() ----------------------------------------------------------------

void loop() {
    // Simulate slowly changing sensor values so the home screen shows movement.
    static uint32_t lastSim = 0;
    if (millis() - lastSim >= 5000) {
        lastSim     = millis();
        g_uptime   += 5.0f / 3600.0f; // 5 seconds = 5/3600 h
        g_doseToday += 0.1f;
        // Drift pH and ORP slightly to simulate a live system.
        g_ph  += (random(-5, 6)) * 0.001f;
        g_orp += random(-3, 4);
        g_ph   = constrain(g_ph,  6.5f, 7.8f);
        g_orp  = constrain(g_orp, 500,  800);
    }

    // Non-blocking — this is the only thing that must run every loop().
    gui.update();
}
