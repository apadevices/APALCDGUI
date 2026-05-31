// APALCDGUI — Minimal example
//
// The smallest possible working sketch: one settings screen, a home screen
// draw callback, and an onSave handler.
//
// Works on any Arduino-compatible board. Default pin arguments match the
// APA Devices HMI board v1.0 — call begin() with explicit pins for other hardware.
//
// ---- Controls ---------------------------------------------------------------
//   Right knob (KB1) — rotate: navigate to the settings screen
//                      hold + rotate: adjust backlight brightness
//   Left  knob (KB2) — rotate: move cursor between fields
//                      press: enter edit mode / confirm
//                      long press: show passive alert detail (if any), then clear

#include <APALCDGUI.h>

APALCDGUI gui;

int16_t setpoint = 72;    // target pH ×10 (7.2 = 72)
float   flowRate = 1.5f;  // L/min

// Called every update() while the home screen is shown.
// Draw sensor readings, status, time — keep fast, never block.
void drawHome(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("pH  7.24  ORP 680mV "));
    lcd.setCursor(0, 1); lcd.print(F("Cl  1.2   Temp  26C "));
    lcd.setCursor(0, 2); lcd.print(F("Filter  ON   12:34  "));
    lcd.setCursor(0, 3); lcd.print(F("System OK           "));
}

// Called when the operator presses SAVE on the settings screen.
void onSave() {
    // setpoint and flowRate are already updated — write to EEPROM or send
    // to APADOSE here.
}

void setup() {
    gui.begin();  // default pins match APA HMI board v1.0

    gui.setHomeCallback(drawHome);

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldInt(  F("pH setpoint"), F("x1"), &setpoint, 68, 82, 1),
        APALCDGUI::fieldFloat(F("Flow rate"),   F("Lm"), &flowRate, 0.5f, 5.0f, 0.1f, 1),
        onSave
    );
}

void loop() {
    gui.update();  // non-blocking — call every loop(), never add delay() here
}
