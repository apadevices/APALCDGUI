// APALCDGUI — Alert system demonstration
//
// Shows how to use both alert tiers:
//   Passive alerts  — non-blocking; a small indicator appears in the top-right
//                     corner of every screen while the operator continues to work.
//   Active alerts   — blocking; the home screen is replaced by a full-screen alarm
//                     that the operator must explicitly acknowledge with KB2.
//
// ---- Screen layout ----------------------------------------------------------
//   HOME     : shows which passive alert is active (if any)
//   RIGHT +1 : trigger passive alerts — INFO / WARNING / CRITICAL
//   RIGHT +2 : trigger active alerts — single alert
//   RIGHT +3 : trigger active alerts — queue test (fire 3 in a row, ACK one by one)
//
// ---- Passive alert indicator (cols 17–19 row 2 on every screen) -------------
//   [i] = ALERT_INFO      — informational, steady
//   [*] = ALERT_WARNING   — warning, steady
//   [!] = ALERT_CRITICAL  — critical, flashing
//
// ---- Passive alert gestures -------------------------------------------------
//   KB2 long press (default built-in): shows the alert text as a 3-second overlay,
//   then clears the indicator. No callback needed in your sketch.
//   Override by calling setLongPressCallback(1, yourFn) — see onKb2Long below.
//
// ---- Active alert flow ------------------------------------------------------
//   1. postActiveAlert() is called (from any trigger below, or from your sensor code).
//   2. HOME is replaced by a full-screen alarm display.
//   3. The operator presses KB2 to acknowledge.
//   4. ackCallback fires (you can show a message, log the event, etc.).
//   5. If more alerts are queued, the next one appears immediately.
//
// ---- Queue test (RIGHT +3 and +4) -------------------------------------------
//   Fire Active 1, 2, and 3 in quick succession.
//   Navigate to HOME — three alerts are shown one at a time.
//   Press KB2 three times to clear them one by one.
//   Each ACK fires its own individual ackCallback.

#include <APALCDGUI.h>

APALCDGUI gui;

// ---- Passive alert triggers -------------------------------------------------
// Each of these posts an indicator in the top-right corner of every screen.
// line1 = heading (up to 16 chars), line2 = detail (up to 16 chars, optional).
// KB2 long press shows the text overlay then clears the indicator automatically.

void trigInfo() {
    gui.postAlert(F("Low pH detected"), F("pH < 7.0  check"), ALERT_INFO);
}

void trigWarning() {
    gui.postAlert(F("High ORP"), F("ORP > 800 mV"), ALERT_WARNING);
}

void trigCritPassive() {
    gui.postAlert(F("Pump fault"), F("pH pump offline"), ALERT_CRITICAL);
}

// ---- Active alert triggers --------------------------------------------------
// Each postActiveAlert() call adds one alarm to the queue.
// The ackCallback (lambda here) fires when the operator presses KB2 on that alarm.
// Use it to log the acknowledgment, restart a process, or show a confirmation message.

void trigActive1() {
    gui.postActiveAlert(
        F("CHLORINE LOW"),       // row 0: heading shown on the alarm screen
        F("Add chlorine now"),   // row 1: detail / instruction for the operator
        ALERT_CRITICAL,
        []() { gui.showMessage("Alarm 1 ACK'd", "Dosing resumed", 1500); }
    );
}

void trigActive2() {
    gui.postActiveAlert(
        F("FILTER BLOCKED"),
        F("Check filter"),
        ALERT_CRITICAL,
        []() { gui.showMessage("Alarm 2 ACK'd", "Filter checked", 1500); }
    );
}

void trigActive3() {
    gui.postActiveAlert(
        F("TANK EMPTY"),
        F("Refill tank"),
        ALERT_CRITICAL,
        []() { gui.showMessage("Alarm 3 ACK'd", "Tank refilled", 1500); }
    );
}

// ---- Home screen ------------------------------------------------------------

void drawHome(LiquidCrystal& lcd) {
    lcd.setCursor(0, 0); lcd.print(F("  ALERT TEST BENCH  "));
    lcd.setCursor(0, 1); lcd.print(F("R+1: passive alerts "));
    lcd.setCursor(0, 2); lcd.print(F("R+2: active alerts  "));
    lcd.setCursor(0, 3); lcd.print(F("KB1 long: clr pasv  "));
}

// ---- Gestures ---------------------------------------------------------------
// This callback overrides the default KB2 long-press behaviour.
// Default (when no callback is set): show passive alert text then clear it.
// Here we clear the alert silently and show a short confirmation message instead.

void onKb2Long() {
    gui.clearAlert();
    gui.showMessage("Passive cleared", nullptr, 800);
}

// ---- setup ------------------------------------------------------------------

void setup() {
    gui.begin();
    gui.setHomeCallback(drawHome);
    // Override KB2 long press — pass index 1 for the left knob (KB2).
    // Remove this line to use the built-in behaviour (show alert text, then clear).
    gui.setLongPressCallback(1, onKb2Long);

    // RIGHT +1: passive alert triggers — fire and observe the corner indicator
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldAction(F("Info alert"),  trigInfo),
        APALCDGUI::fieldAction(F("Warn alert"),  trigWarning)
    );
    // RIGHT +2: critical passive alert + manual clear
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldAction(F("Crit passive"), trigCritPassive),
        APALCDGUI::fieldAction(F("Clear pasv"),   []() { gui.clearAlert(); })
    );

    // RIGHT +3 and +4: active alert queue test
    // Fire Active 1, 2, 3 in any order, then navigate to HOME and ACK them one by one.
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldAction(F("Active 1"), trigActive1),
        APALCDGUI::fieldAction(F("Active 2"), trigActive2)
    );
    gui.addScreen(SCREEN_RIGHT,
        APALCDGUI::fieldAction(F("Active 3"), trigActive3),
        APALCDGUI::fieldAction(F("(no action)"), nullptr)  // nullptr = shows -no-, does nothing
    );
}

// ---- loop -------------------------------------------------------------------

void loop() {
    gui.update();
}
