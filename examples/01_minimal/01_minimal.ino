// APALCDGUI — Minimal example
//
// The smallest possible working sketch: one settings screen, a home screen
// draw callback, and an onSave handler.
//
// Works on any Arduino-compatible board. Default pin arguments match the
// APA Devices HMI board v1.0 — call begin() with explicit pins for other hardware.
//
// ---- Controls ---------------------------------------------------------------
//   KB1 (knob 1)     — rotate: navigate to the settings screen
//                      hold + rotate: adjust backlight brightness
//   KB2 (knob 2)     — rotate: move cursor between fields
//                      press: enter edit mode / confirm
//                      long press: show passive alert detail (if any), then clear

#include <APALCDGUI.h>

APALCDGUI gui;

// Variables updated when the operator presses SAVE — read them in onSave().
float   phSetpoint  = 7.20f;  // target pH
int16_t orpSetpoint = 680;    // target ORP in mV

// Called every update() while the home screen is shown.
// Draw sensor readings, status, time — keep it fast, never call delay() here.
void drawHome(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("pH  7.24  ORP 680mV "));
    lcd.setCursor(0, 1); lcd.print(F("Cl  1.2   Temp  26C "));
    lcd.setCursor(0, 2); lcd.print(F("Filter ON   12:34   "));  // cols 17-19 = alert indicator
    lcd.setCursor(0, 3); lcd.print(F("K1:settings         "));
}

// Called after the operator presses SAVE on the settings screen.
// phSetpoint and orpSetpoint already hold the new values at this point.
void onSave() {
    // Write phSetpoint and orpSetpoint to EEPROM, or send to APADOSE here.
}

void setup() {
    gui.begin();  // default pins match APA HMI board v1.0

    gui.setHomeCallback(drawHome);

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldFloat(F("pH setpoint"),  F("pH"), &phSetpoint,  6.8f, 7.8f, 0.01f, 2),
        APALCDGUI::fieldInt(  F("ORP setpoint"), F("mV"), &orpSetpoint, 400,  850,  10),
        onSave
    );
}

void loop() {
    gui.update();  // non-blocking — call every loop(), never add delay() here
}
