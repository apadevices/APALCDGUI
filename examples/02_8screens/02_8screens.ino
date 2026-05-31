// APALCDGUI — Comprehensive demo — all 6 field types, 8 screens
//
// The best starting point for understanding the full API. Shows every feature
// the library offers in a realistic pool automation context.
//
// ---- Field types demonstrated -----------------------------------------------
//   FIELD_INT      — integer scrolled between min and max          (R+1 ORP, R+2)
//   FIELD_FLOAT    — decimal value with configurable places        (R+1 pH, R+3, R+4)
//   FIELD_CHOICE   — cycles a named-option string array            (L-1, L-2)
//   FIELD_BOOL     — on/off toggle — shows " ON " / "OFF "        (L-1 third field)
//   FIELD_READONLY — display only, cursor skips automatically      (L-2 FW, L-3)
//   FIELD_ACTION   — button that fires a callback on SAVE          (L-4)
//
// ---- Callback types used ----------------------------------------------------
//   setHomeCallback(fn)         — fn(LiquidCrystal& lcd) — THE ONLY callback with a parameter
//   addScreen(..., onSave)      — called after the operator presses SAVE
//   postActiveAlert(..., ackCb) — called when the operator acknowledges the alarm
//   setLongPressCallback(n, fn) — called after an encoder button is held 800 ms
//   setBothPressedCallback(fn)  — called when both buttons are pressed within 200 ms
//   fieldAction(label, fn)      — called when SAVE is pressed on that ACTION field
//
// ---- SAVE vs BACK -----------------------------------------------------------
//   SAVE — commits every field edit on this screen, calls onSave().
//   BACK — discards any in-progress edit. onSave is NOT called.
//
// ---- Wiring -----------------------------------------------------------------
//   APA Devices HMI board v1.0 — all default pins, no arguments needed to begin().
//
// ---- Controls ---------------------------------------------------------------
//   Right knob (knob1 / KB1) — rotate: navigate between screens
//                               hold KB1 + rotate: adjust brightness
//   Left  knob (knob2 / KB2) — rotate: move cursor / change value when editing
//                               press: enter edit mode or confirm
//                               long press (800 ms): show passive alert text then dismiss it

// The default maximum is 4 screens. This sketch uses 8.
// PlatformIO: set via build_flags = -DAPA_LCD_MAX_SCREENS=8 in platformio.ini.
// Arduino IDE: add the #define below BEFORE the #include line in your sketch.

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Simulated sensor readings ----------------------------------------------
float   g_ph       = 7.24f;
int16_t g_orp      = 682;
float   g_temp     = 26.3f;
float   g_cl       = 1.2f;
float   g_uptime   = 0.0f;
float   g_doseToday = 0.0f;

// ---- User-settable parameters -----------------------------------------------
// RIGHT screen 1: pool chemistry targets
float   r1_phSetpoint  = 7.20f;
int16_t r1_orpSetpoint = 680;

// RIGHT screen 2: filtration & pool volume
int16_t r2_filterHours = 8;
int16_t r2_poolVolume  = 25;

// RIGHT screen 3: temperature limits
float r3_maxPoolTemp   = 30.0f;
float r3_solarSetpoint = 45.0f;

// RIGHT screen 4: dosing rates
float r4_phDoseRate = 15.0f;
float r4_clDoseRate = 20.0f;

// LEFT screen 1: CHOICE + BOOL fields
uint8_t l1_phDirection = 0;  // 0=MINUS, 1=PLUS
uint8_t l1_clMode      = 0;  // 0=AUTO,  1=MANUAL, 2=OFF
bool    l1_autoAdjust  = true;
// CHOICE strings MUST be exactly 4 characters — pad shorter strings with trailing spaces.
static const char* phDirChoices[]  = {"MINS", "PLUS", nullptr};
static const char* clModeChoices[] = {"AUTO", "MANU", "OFF ", nullptr};

// LEFT screen 2: CHOICE + READONLY
uint8_t l2_dispMode  = 0;
static const char* dispModeChoices[] = {"NORM", "EXT ", nullptr};
float   l2_fwVersion = 1.10f;

// ---- Save callbacks ---------------------------------------------------------

void onChemSave()  { /* write r1_phSetpoint, r1_orpSetpoint to EEPROM / APADOSE */ }
void onFiltSave()  { /* write r2_filterHours, r2_poolVolume */ }
void onTempSave()  { /* write r3_maxPoolTemp, r3_solarSetpoint */ }
void onDoseSave()  { /* write r4_phDoseRate, r4_clDoseRate */ }
void onModeSave()  { /* write l2_dispMode */ }

// ---- ACTION callbacks -------------------------------------------------------

void runPhPumpTest() { gui.showMessage("pH pump TEST", "2 s pulse fired", 2000); }
void runClPumpTest() { gui.showMessage("Cl pump TEST", "2 s pulse fired", 2000); }

// ---- Gesture callbacks ------------------------------------------------------

void onKb2LongPress() {
    gui.clearAlert();
    gui.showMessage("Alert cleared", nullptr, 1000);
}

void onBothPressed() {
    gui.showMessage("Both pressed!", "200ms gesture", 1500);
}

// ---- Home screen callback ---------------------------------------------------
// The only callback that receives a parameter (LiquidCrystal&).
// All other callbacks — onSave, ackCallback, long-press, action — are void(void).
// Runs every loop() while at HOME — keep fast, no delay(), no blocking calls.
// AVR: snprintf() does NOT support %f — always use dtostrf() for floats.

void drawHome(LiquidCrystal& lcd) {
    char fa[5], fb[5];
    char buf[21];

    dtostrf(g_ph, 4, 2, fa);
    snprintf(buf, sizeof(buf), "pH%s  ORP%4dmV  ", fa, g_orp);
    lcd.setCursor(0, 0); lcd.print(buf);

    dtostrf(g_cl,   4, 1, fa);
    dtostrf(g_temp, 4, 1, fb);
    snprintf(buf, sizeof(buf), "Cl%sppm Tmp%s", fa, fb);
    lcd.setCursor(0, 1); lcd.print(buf);
    lcd.setCursor(17, 1); lcd.write(CC_DEGREE);  // ° custom char slot 4
    lcd.setCursor(18, 1); lcd.print(F("  "));

    uint32_t upH = (uint32_t)g_uptime;
    snprintf(buf, sizeof(buf), "Filt OK  Up%6luh  ", (unsigned long)upH);
    lcd.setCursor(0, 2); lcd.print(buf);

    lcd.setCursor(0, 3); lcd.print(F("K1:screens K2:edit  "));
}

// ---- setup() ----------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    gui.begin();  // all defaults match APA Devices HMI board v1.0

    gui.setHomeCallback(drawHome);

    gui.setLongPressCallback(1, onKb2LongPress);
    gui.setBothPressedCallback(onBothPressed);

    // ---- RIGHT screens ----

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH setpoint"),  F("pH"), &r1_phSetpoint,  6.8f, 7.8f, 0.01f, 2),
        APALCDGUI::fieldInt(  F("ORP setpoint"), F("mV"), &r1_orpSetpoint, 400,  850,  10),
        onChemSave
    );

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldInt(F("Filt hrs/day"), F("h"),  &r2_filterHours, 1,  24, 1),
        APALCDGUI::fieldInt(F("Pool volume"),  F("m3"), &r2_poolVolume,  10, 90, 1),
        onFiltSave
    );

    // F("\x04""C") prints °C — CC_DEGREE is custom char slot 4
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("Max pool tmp"), F("\x04""C"), &r3_maxPoolTemp,   15.0f, 40.0f, 0.5f, 1),
        APALCDGUI::fieldFloat(F("Solar setpt"),  F("\x04""C"), &r3_solarSetpoint,  20.0f, 70.0f, 1.0f, 1),
        onTempSave
    );

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH dose rate"), F("ml"), &r4_phDoseRate, 1.0f, 50.0f, 0.5f, 1),
        APALCDGUI::fieldFloat(F("Cl dose rate"), F("ml"), &r4_clDoseRate, 1.0f, 50.0f, 0.5f, 1),
        onDoseSave
    );

    // ---- LEFT screens ----

    // 3-field screen: 2 CHOICE + 1 BOOL
    // knob2 visits all three fields in order before reaching BACK / SAVE.
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(F("pH direction"), &l1_phDirection, phDirChoices),
        APALCDGUI::fieldChoice(F("Cl mode"),      &l1_clMode,      clModeChoices),
        APALCDGUI::fieldBool(  F("Auto adjust"),  &l1_autoAdjust)
    );

    // READONLY field: cursor skips it, jumps straight to BACK / SAVE
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(  F("Display mode"), &l2_dispMode, dispModeChoices),
        APALCDGUI::fieldReadonly(F("FW version"),    F("  "), &l2_fwVersion, 2),
        onModeSave
    );

    // Both READONLY — nothing to commit, no onSave needed
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldReadonly(F("Uptime"),     F("h"),  &g_uptime,    1),
        APALCDGUI::fieldReadonly(F("Dose today"), F("ml"), &g_doseToday, 0)
    );

    // ACTION fields: shows "STRT" when fn != nullptr, "-no-" when nullptr
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldAction(F("pH pump test"), runPhPumpTest),
        APALCDGUI::fieldAction(F("Cl pump test"), runClPumpTest)
    );
}

// ---- loop() -----------------------------------------------------------------

void loop() {
    static uint32_t lastSim = 0;
    if (millis() - lastSim >= 5000) {
        lastSim      = millis();
        g_uptime    += 5.0f / 3600.0f;
        g_doseToday += 0.1f;
        g_ph        += random(-5, 6) * 0.001f;
        g_orp       += random(-3, 4);
        g_ph         = constrain(g_ph,  6.5f, 7.8f);
        g_orp        = constrain(g_orp, 500,  800);
    }

    gui.update();
}
