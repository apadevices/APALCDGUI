// APALCDGUI — Mega 2560 — DS3231 RTC modal example
//
// Builds on the 8-screen example (01_8screens) and adds a DS3231 real-time
// clock module. The operator sets the time and date through a built-in modal
// that opens when both buttons are pressed at the same time.
//
// ---- What the RTC modal looks like -----------------------------------------
//
//   TIME sub-screen:          DATE sub-screen:
//   ► 14 : 32 : 07            ► 31 / 05 / 2026
//   BACK              SAVE    BACK              SAVE
//
//   Knob1 (right) switches between TIME and DATE sub-screens.
//   Knob2 (left) moves the cursor between fields.
//   Knob2 press enters / exits edit mode.
//   Cursor on SAVE + press writes all fields to the DS3231 chip.
//   Cursor on BACK + press discards changes and returns to HOME.
//
// ---- Enabling the RTC modal -------------------------------------------------
//
//   The DS3231 modal is compiled in by defining APA_LCD_USE_DS3231.
//   In PlatformIO add it to build_flags in platformio.ini:
//
//     build_flags = -DAPA_LCD_USE_DS3231
//
//   In Arduino IDE add before the #include:
//
//     #define APA_LCD_USE_DS3231
//     #include <APALCDGUI.h>
//
//   When the flag is set, APALCDGUI.h automatically includes Wire.h and
//   DS3231.h — no manual includes needed in the sketch.
//   You still need Wire.begin() in setup().
//
// ---- Wiring (Mega 2560) -----------------------------------------------------
//   DS3231 SDA → Mega pin 20
//   DS3231 SCL → Mega pin 21
//   DS3231 VCC → 3.3 V (some modules also work at 5 V — check your module)
//   DS3231 GND → GND
//
// ---- PlatformIO lib_deps ----------------------------------------------------
//   arduino-libraries/LiquidCrystal @ ^1.0.7
//   NorthernWidget/DS3231
//
// ---- Controls ---------------------------------------------------------------
//   Right knob (KB1) — rotate: navigate between screens
//                      rotate while held: adjust brightness
//   Left  knob (KB2) — rotate: move cursor / change value during edit
//                      press:  enter edit / confirm / switch RTC sub-screen
//   Both buttons simultaneously → open RTC time/date modal

// APA_LCD_USE_DS3231 must be set in build_flags — see comment above.
// APALCDGUI.h pulls in Wire.h and DS3231.h automatically when the flag is set.
#include <APALCDGUI.h>

APALCDGUI gui;
DS3231    rtc;

// ---- Simulated sensor readings (same as 01_8screens) -----------------------
float   g_ph      = 7.24f;
int16_t g_orp     = 682;
float   g_temp    = 26.3f;
float   g_cl      = 1.2f;
float   g_uptime  = 0.0f;

// ---- User-settable parameters -----------------------------------------------
float   r1_phSetpoint  = 7.20f;
int16_t r1_orpSetpoint = 680;
int16_t r2_filterHours = 8;
int16_t r2_poolVolume  = 25;
float   r3_maxPoolTemp   = 30.0f;
float   r3_solarSetpoint = 45.0f;

uint8_t l1_phDirection = 0;
uint8_t l1_clMode      = 0;
static const char* phDirChoices[]  = {"MINS", "PLUS", nullptr};
static const char* clModeChoices[] = {"AUTO", "MANU", "OFF ", nullptr};

// ---- Save callbacks ---------------------------------------------------------
void onChemSave()  {}
void onFiltSave()  {}
void onTempSave()  {}

// ---- Home screen callback ---------------------------------------------------
// Shows current sensor readings AND the live time from the DS3231.
// AVR snprintf does not support %f — always use dtostrf for float values.
void drawHome(LiquidCrystal& lcd) {
    char fa[5], fb[5];
    char buf[21];

    // Row 0: pH and ORP
    dtostrf(g_ph, 4, 2, fa);
    snprintf(buf, sizeof(buf), "pH%s  ORP%4dmV  ", fa, g_orp);
    lcd.setCursor(0, 0); lcd.print(buf);

    // Row 1: Cl and temperature
    dtostrf(g_cl,   4, 1, fa);
    dtostrf(g_temp, 4, 1, fb);
    snprintf(buf, sizeof(buf), "Cl%sppm Tmp%s", fa, fb);
    lcd.setCursor(0, 1); lcd.print(buf);
    lcd.setCursor(17, 1); lcd.write(CC_DEGREE);
    lcd.setCursor(18, 1); lcd.print(F("  "));

    // Row 2: live time from DS3231 — HH:MM:SS
    // getHour() returns 12/24-hour depending on the chip mode register.
    // getHour(h12, pm) — pass two bool refs; we ignore them (24-hour mode).
    bool h12, pm;
    uint8_t h = rtc.getHour(h12, pm);
    uint8_t m = rtc.getMinute();
    uint8_t s = rtc.getSecond();
    snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d       ", h, m, s);
    lcd.setCursor(0, 2); lcd.print(buf);

    // Row 3: gesture hints
    lcd.setCursor(0, 3); lcd.print(F("K1:screens *:settime"));
}

// ---- setup() ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Wire.begin();   // required before any DS3231 call

    gui.begin();    // all defaults match APA Devices HMI board v1.0

    // Wire both-buttons gesture to the built-in RTC modal.
    // Do NOT call setBothPressedCallback() after this — it would override the modal.
    gui.setRTC(&rtc);

    gui.setHomeCallback(drawHome);

    // RIGHT screens
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

    // LEFT screens
    gui.addScreen(SCREEN_LEFT,
        APALCDGUI::fieldChoice(F("pH direction"), &l1_phDirection, phDirChoices),
        APALCDGUI::fieldChoice(F("Cl mode"),      &l1_clMode,      clModeChoices)
    );

    Serial.println(F("Ready. Press both buttons to set time/date."));
}

// ---- loop() -----------------------------------------------------------------
void loop() {
    // Simulate slowly drifting sensor values
    static uint32_t lastSim = 0;
    if (millis() - lastSim >= 5000) {
        lastSim    = millis();
        g_uptime  += 5.0f / 3600.0f;
        g_ph      += random(-5, 6) * 0.001f;
        g_orp     += random(-3, 4);
        g_ph       = constrain(g_ph,  6.5f, 7.8f);
        g_orp      = constrain(g_orp, 500,  800);
    }

    gui.update();
}
