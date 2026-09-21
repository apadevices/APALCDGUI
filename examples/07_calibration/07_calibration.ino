// 07_calibration — Generic two-point guided calibration screen
//
// Demonstrates addCalibrationScreen() / startCalibration() / setCalMessage() —
// a reusable two-point guided calibration wizard built into APALCDGUI itself.
// APALCDGUI has NO concept of what's actually being calibrated: this example
// simulates a simple sensor with a short (a few seconds) fake capture so the
// example is practical to actually run, but the identical pattern works for a
// real pH/ORP probe whose own calibration call blocks for several minutes —
// just swap simulateCapture() for your sensor library's own calibratePoint1()/
// calibratePoint2() calls, same as onPoint1()/onPoint2() below already show.
//
// ---- Recommended entry pattern: FIELD_CHOICE + onSave(), not FIELD_ACTION --
//   FIELD_ACTION fires immediately the instant the operator selects it, with
//   no toggle step at all. FIELD_CHOICE lets the operator select the row,
//   toggle its value, then commit via SAVE — the same select-toggle-then-SAVE
//   shape every other settings screen in this library already uses, so a
//   calibration screen doesn't feel like a special case to the operator.
//
// What you will learn:
//   - addCalibrationScreen() — register a generic two-point wizard
//   - startCalibration()     — launch it from your own screen's onSave()
//   - setCalMessage()        — route live progress text onto the wizard's screen
//
// ---- Controls -----------------------------------------------------------
//   Right knob (KB1) — rotate: navigate between screens
//   Left  knob (KB2) — rotate: move cursor / toggle a value
//                       press:  enter edit / commit / confirm

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Simulated sensor state ----------------------------------------------
// A real sensor library owns calibration state like this instead — APALCDGUI
// never sees or stores it itself.
bool  g_isCalibrated    = false;
float g_calibratedValue = 0.0f;

// ---- Calibration entry: FIELD_CHOICE toggle + onSave() -------------------
static const char* calChoices[] = {"-no-", "STRT", nullptr};
uint8_t calChoice = 0;
int8_t  calIdx    = -1;   // registered index, returned by addCalibrationScreen()

// ---- Simulated two-point capture ------------------------------------------
// A real sensor library blocks here for minutes (e.g. APAPHX2_ADS1115's own
// calibratePoint1()/calibratePoint2()); this fakes a short 3-second wait with
// two progress messages via setCalMessage(), so the example stays practical
// to run while still demonstrating the live-message hook.
float simulateCapture() {
    gui.setCalMessage(F("Soaking..."));
    delay(1500);
    gui.setCalMessage(F("Stabilizing..."));
    delay(1500);
    return (float)random(0, 1000) / 10.0f;   // fake measured value
}

float onPoint1(float knownValue) {
    (void)knownValue;
    return simulateCapture();
}

bool onPoint2(float knownValue) {
    simulateCapture();
    // A real sensor library validates the two points aren't too close before
    // reporting success, and persists the result itself (e.g. saveCalibration())
    // — persistence is always the CALLER's job, never this screen's.
    g_isCalibrated    = true;
    g_calibratedValue = knownValue;
    return true;
}

void onCalSave() {
    if (calChoice == 1) {
        calChoice = 0;
        gui.startCalibration(calIdx);
    }
}

// ---- Home screen ----------------------------------------------------------
void drawHome(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0);
    lcd.print(F("07_calibration demo "));

    lcd.setCursor(0, 1);
    if (g_isCalibrated) {
        char vbuf[8]; dtostrf(g_calibratedValue, 4, 2, vbuf);
        char buf[21]; snprintf(buf, sizeof(buf), "Value: %s        ", vbuf);
        lcd.print(buf);
    } else {
        lcd.print(F("Value: -- (not cal.)"));
    }

    lcd.setCursor(0, 2);
    lcd.print(F("K1:screens K2:edit  "));
}

void setup() {
    Serial.begin(115200);

    gui.begin();
    gui.setHomeCallback(drawHome);

    calIdx = gui.addCalibrationScreen(F("Demo Calibration"),
                 F("Place ref. 1"), 4.0f,
                 F("Place ref. 2"), 7.0f,
                 onPoint1, onPoint2);

    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldChoice(F("Calibrate"), &calChoice, calChoices),
        onCalSave, F("Calibration"));
}

void loop() {
    gui.update();
}
