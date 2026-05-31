// APALCDGUI — Multiple home screen pages
//
// Shows how to register more than one home page using addHomeScreen().
// The operator scrolls between pages by rotating knob2 (left knob) on the home
// screen. The library draws the page indicator (1/3, 2/3, 3/3) automatically at
// row 3 cols 17–19 — no user code required.
//
// Three home pages are registered:
//   Page 1 — live sensor readings (pH, ORP, chlorine, temperature)
//   Page 2 — weekly dose totals
//   Page 3 — system status (uptime, dose today, firmware version)
//
// The submenu screens are identical to 02_8screens — all 6 field types across
// 4 right and 4 left screens. See 02_8screens.ino for full commentary on fields.
//
// ---- Layout constraint -------------------------------------------------------
//   Row 2 cols 17–19: passive alert indicator — ALWAYS library-owned; leave blank.
//   Row 3 cols 17–19: page indicator — library-owned when > 1 page registered.
//   Write ONLY to cols 0–16 on rows 2 and 3.
//
// ---- Build flag required -----------------------------------------------------
//   PlatformIO: build_flags = -DAPA_LCD_MAX_SCREENS=8
//   Arduino IDE: add the line below BEFORE the #include:
//     #define APA_LCD_MAX_SCREENS 8
//
// ---- Controls ----------------------------------------------------------------
//   Right knob (KB1) — rotate: navigate between screens
//                      hold + rotate: adjust backlight brightness
//   Left  knob (KB2) — rotate on HOME: scroll between home pages
//                      rotate on menu: move cursor / change value
//                      press: enter edit mode or confirm
//                      long press (800 ms): show passive alert text, then dismiss

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Simulated sensor readings (replace with real sensor reads in your project)
float   g_ph        = 7.24f;
int16_t g_orp       = 682;
float   g_cl        = 1.2f;
float   g_temp      = 26.3f;

// ---- Simulated running totals
float   g_uptime    = 0.0f;   // hours
float   g_doseToday = 0.0f;   // mL
int16_t g_dosesWeekPh = 14;
int16_t g_dosesWeekCl = 22;
float   g_doseMlWeek  = 136.0f;

// ---- User-settable parameters ------------------------------------------------

// RIGHT screen 1: pool chemistry targets
float   r1_phSetpoint  = 7.20f;
int16_t r1_orpSetpoint = 680;

// RIGHT screen 2: filtration
int16_t r2_filterHours = 8;
int16_t r2_poolVolume  = 25;

// RIGHT screen 3: temperature limits
float r3_maxPoolTemp   = 30.0f;
float r3_solarSetpoint = 45.0f;

// RIGHT screen 4: dosing rates
float r4_phDoseRate = 15.0f;
float r4_clDoseRate = 20.0f;

// LEFT screen 1: CHOICE + BOOL
uint8_t l1_phDirection = 0;
uint8_t l1_clMode      = 0;
bool    l1_autoAdjust  = true;
static const char* phDirChoices[]  = {"MINS", "PLUS", nullptr};
static const char* clModeChoices[] = {"AUTO", "MANU", "OFF ", nullptr};

// LEFT screen 2: CHOICE + READONLY
uint8_t l2_dispMode  = 0;
float   l2_fwVersion = 1.14f;
static const char* dispModeChoices[] = {"NORM", "EXT ", nullptr};

// ---- Save callbacks ----------------------------------------------------------
void onChemSave() { /* write r1_* to EEPROM / APADOSE */ }
void onFiltSave() { /* write r2_* */ }
void onTempSave() { /* write r3_* */ }
void onDoseSave() { /* write r4_* */ }
void onModeSave() { /* write l2_dispMode */ }

// ---- ACTION callbacks --------------------------------------------------------
void runPhPumpTest() { gui.showMessage(F("pH pump TEST"), F("2 s pulse fired"), 2000); }
void runClPumpTest() { gui.showMessage(F("Cl pump TEST"), F("2 s pulse fired"), 2000); }

// ---- Gesture callbacks -------------------------------------------------------
void onKb2LongPress() {
    gui.clearAlert();
    gui.showMessage(F("Alert cleared"), nullptr, 1000);
}
void onBothPressed() {
    gui.showMessage(F("Both pressed!"), F("200ms gesture"), 1500);
}

// =============================================================================
// Home screen pages
//
// Each page callback must write ONLY to cols 0–16 on row 2 and row 3.
// The library overwrites cols 17–19 on both rows after the callback returns:
//   Row 2 cols 17–19 — passive alert indicator ([i] / [*] / [!])
//   Row 3 cols 17–19 — page indicator (1/3, 2/3, 3/3)
// =============================================================================

// ---- Page 1: live sensor readings -------------------------------------------
void drawPage1(LiquidCrystal& lcd) {
    char fa[5], fb[5];
    char buf[21];

    // Row 0: pH and ORP
    dtostrf(g_ph, 4, 2, fa);
    snprintf(buf, sizeof(buf), "pH%s  ORP%4dmV  ", fa, g_orp);
    lcd.setCursor(0, 0); lcd.print(buf);

    // Row 1: chlorine and temperature — CC_DEGREE (slot 4) gives the ° symbol
    dtostrf(g_cl,   4, 1, fa);
    dtostrf(g_temp, 4, 1, fb);
    snprintf(buf, sizeof(buf), "Cl%sppm Tmp%s", fa, fb);
    lcd.setCursor(0, 1); lcd.print(buf);
    lcd.setCursor(17, 1); lcd.write(CC_DEGREE);
    lcd.setCursor(18, 1); lcd.print(F("  "));

    // Row 2: filter state (cols 0–16 only — 17–19 = alert indicator)
    uint32_t upH = (uint32_t)g_uptime;
    snprintf(buf, sizeof(buf), "Filt OK  Up%6luh", (unsigned long)upH);
    lcd.setCursor(0, 2); lcd.print(buf);

    // Row 3: hint (cols 0–16 only — 17–19 = page indicator)
    lcd.setCursor(0, 3); lcd.print(F("K1:screens K2:pg "));
}

// ---- Page 2: weekly dose totals ---------------------------------------------
void drawPage2(LiquidCrystal& lcd) {
    char buf[21];

    // Row 0: header
    lcd.setCursor(0, 0); lcd.print(F("-- This week -------"));

    // Row 1: dose counts
    snprintf(buf, sizeof(buf), "pH-: %3d  CL+: %3d  ", g_dosesWeekPh, g_dosesWeekCl);
    lcd.setCursor(0, 1); lcd.print(buf);

    // Row 2: last dose time (cols 0–16 only)
    lcd.setCursor(0, 2); lcd.print(F("Last dose:  12:10  "));

    // Row 3: total volume (cols 0–16 only — 17–19 = page indicator)
    char mlStr[6];
    dtostrf(g_doseMlWeek, 5, 0, mlStr);
    snprintf(buf, sizeof(buf), "Total acid: %5sml", mlStr);
    lcd.setCursor(0, 3); lcd.print(buf);
}

// ---- Page 3: system status --------------------------------------------------
void drawPage3(LiquidCrystal& lcd) {
    char buf[21];

    // Row 0: uptime
    uint32_t upH = (uint32_t)g_uptime;
    uint32_t upM = (uint32_t)((g_uptime - upH) * 60.0f);
    snprintf(buf, sizeof(buf), "Uptime: %4luh %2lum    ", (unsigned long)upH, (unsigned long)upM);
    lcd.setCursor(0, 0); lcd.print(buf);

    // Row 1: dose today
    char doseStr[6];
    dtostrf(g_doseToday, 5, 1, doseStr);
    snprintf(buf, sizeof(buf), "Dose today: %5sml  ", doseStr);
    lcd.setCursor(0, 1); lcd.print(buf);

    // Row 2: firmware version (cols 0–16 only)
    char fwStr[5];
    dtostrf(l2_fwVersion, 4, 2, fwStr);
    snprintf(buf, sizeof(buf), "Firmware:   %s     ", fwStr);
    lcd.setCursor(0, 2); lcd.print(buf);

    // Row 3: library version (cols 0–16 only — 17–19 = page indicator)
    lcd.setCursor(0, 3); lcd.print(F("Lib: " APALCDGUI_VERSION "            "));
}

// ---- setup() ----------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    gui.begin();  // all defaults match APA Devices HMI board v1.0

    // Register three home pages — KB2 rotation on HOME cycles between them.
    // The library draws "1/3", "2/3", "3/3" at row 3 cols 17–19 automatically.
    gui.addHomeScreen(drawPage1);  // page 1 — shown on power-on
    gui.addHomeScreen(drawPage2);  // page 2 — rotate KB2 once to reach
    gui.addHomeScreen(drawPage3);  // page 3 — rotate KB2 again

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

    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(F("pH direction"), &l1_phDirection, phDirChoices),
        APALCDGUI::fieldChoice(F("Cl mode"),      &l1_clMode,      clModeChoices),
        APALCDGUI::fieldBool(  F("Auto adjust"),  &l1_autoAdjust)
    );

    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(  F("Display mode"), &l2_dispMode,  dispModeChoices),
        APALCDGUI::fieldReadonly(F("FW version"),    F("  "), &l2_fwVersion, 2),
        onModeSave
    );

    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldReadonly(F("Uptime"),     F("h"),  &g_uptime,    1),
        APALCDGUI::fieldReadonly(F("Dose today"), F("ml"), &g_doseToday, 0)
    );

    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldAction(F("pH pump test"), runPhPumpTest),
        APALCDGUI::fieldAction(F("Cl pump test"), runClPumpTest)
    );
}

// ---- loop() -----------------------------------------------------------------

void loop() {
    // Simulate slow sensor drift — replace with real sensor reads in your project
    static uint32_t lastSim = 0;
    if (millis() - lastSim >= 5000) {
        lastSim        = millis();
        g_uptime      += 5.0f / 3600.0f;
        g_doseToday   += 0.1f;
        g_ph          += random(-5, 6) * 0.001f;
        g_orp         += random(-3, 4);
        g_ph           = constrain(g_ph,  6.5f, 7.8f);
        g_orp          = constrain(g_orp, 500,  800);
    }

    gui.update();  // non-blocking — call every loop(), never add delay() here
}
