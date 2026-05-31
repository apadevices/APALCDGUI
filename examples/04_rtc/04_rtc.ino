// APALCDGUI — DS3231 RTC modal example
//
// Adds a DS3231 real-time clock to the 8-screen base sketch. The operator
// sets the time and date through a built-in two-step modal.
//
// ---- RTC modal flow ---------------------------------------------------------
//
//   Trigger : hold both buttons (KB1 + KB2) simultaneously for 800 ms.
//
//   Step 1 — TIME screen:
//     ► HH  :  MM  :  SS
//       14     32     07
//        ↑
//     BACK              SAVE  →  advances to DATE screen
//
//   Step 2 — DATE screen:
//     ► DD  /  MM  / YYYY
//       31     05    2026
//              ↑
//     BACK (returns to TIME)   SAVE  →  writes all fields, returns HOME
//
//   ↑ marks the field the cursor is on. Knob2 moves between fields.
//   Knob2 press enters edit for that field; rotate to change, press to confirm.
//   BACK on TIME discards everything and returns to HOME.
//
// ---- Enabling the RTC modal -------------------------------------------------
//
//   Define APA_LCD_USE_DS3231 before or via build_flags.
//   APALCDGUI.h then includes Wire.h and DS3231.h automatically.
//
//   PlatformIO (platformio.ini):
//     lib_deps  = ... NorthernWidget/DS3231
//     build_flags = -DAPA_LCD_USE_DS3231
//
//   Arduino IDE (before #include):
//     #define APA_LCD_USE_DS3231
//     #include <APALCDGUI.h>
//
// ---- Wiring -----------------------------------------------------------------
//   DS3231 SDA → board SDA pin (Mega: 20, Uno: A4, ESP32: 21)
//   DS3231 SCL → board SCL pin (Mega: 21, Uno: A5, ESP32: 22)
//   DS3231 VCC → 3.3 V  (check your module — some tolerate 5 V)
//   DS3231 GND → GND
//
// ---- Note: do NOT call setBothPressedCallback() when using setRTC() ---------
//   setBothPressedCallback takes priority and will suppress the RTC modal.

// APA_LCD_USE_DS3231 must be defined (see above).
// APALCDGUI.h includes Wire.h and DS3231.h automatically when the flag is set.
#include <APALCDGUI.h>

APALCDGUI gui;
DS3231    rtc;

// ---- Simulated sensor readings ----------------------------------------------
float   g_ph    = 7.24f;
int16_t g_orp   = 682;
float   g_temp  = 26.3f;
float   g_cl    = 1.2f;
float   g_uptime = 0.0f;

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
void onChemSave() {}
void onFiltSave() {}
void onTempSave() {}

// ---- Home screen callback ---------------------------------------------------
// Displays live sensor readings and the current time from the DS3231.
// AVR: snprintf() does NOT support %f — use dtostrf() for all float values.

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

    // Row 2: live clock — read directly from DS3231 every frame
    // getHour(h12, pm) takes two bool refs; we pass locals and ignore them (24-hour mode)
    bool h12, pm;
    uint8_t h = rtc.getHour(h12, pm);
    uint8_t m = rtc.getMinute();
    uint8_t s = rtc.getSecond();
    snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d       ", h, m, s);
    lcd.setCursor(0, 2); lcd.print(buf);

    // Row 3: hint for the RTC gesture
    lcd.setCursor(0, 3); lcd.print(F("K1:screens *:settime"));
}

// ---- setup() ----------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    Wire.begin();   // initialise I2C before any DS3231 call

    gui.begin();    // defaults match APA Devices HMI board v1.0
    gui.setRTC(&rtc);           // hold both buttons 800 ms → RTC modal
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

    Serial.println(F("Ready. Hold both buttons 800 ms to set time/date."));
}

// ---- loop() -----------------------------------------------------------------

void loop() {
    static uint32_t lastSim = 0;
    if (millis() - lastSim >= 5000) {
        lastSim   = millis();
        g_uptime += 5.0f / 3600.0f;
        g_ph     += random(-5, 6) * 0.001f;
        g_orp    += random(-3, 4);
        g_ph      = constrain(g_ph,  6.5f, 7.8f);
        g_orp     = constrain(g_orp, 500,  800);
    }

    gui.update();
}
