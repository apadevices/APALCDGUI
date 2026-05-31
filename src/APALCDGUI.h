// APALCDGUI — Non-blocking 20×4 LCD menu and alert library
//
// Designed for the APA Devices HMI board v1.0:
//   hardware: 20×4 character LCD (HD44780, 4-bit), two PEC11R rotary encoders with push buttons
//
// Three-step usage:
//   APALCDGUI gui;                        // 1. global — construct (default pins = APA HMI board)
//   void setup() {                        // 2. setup — init hardware, register screens
//       gui.begin();
//       gui.setHomeCallback(drawHome);    //    optional: your home screen draw function
//       gui.addScreen(SCREEN_RIGHT, ...); //    add as many submenus as needed (default max 4)
//   }
//   void loop() { gui.update(); }         // 3. loop — handle all input, drawing, alerts
//
// Controls:
//   Right knob (knob1 / KB1) — rotate: navigate between screens
//                               hold KB1 + rotate: adjust backlight brightness
//   Left  knob (knob2 / KB2) — rotate: move cursor / change a value during editing
//                               press:      enter edit mode, or confirm a selection
//                               long press: show passive alert detail text, then dismiss it
//
// Submenu screen layout (20 columns × 4 rows):
//   row 0: [cursor][label (12 ch)][ ][value (4 ch)][unit (2 ch)]
//   row 1: [cursor][label (12 ch)][ ][value (4 ch)][unit (2 ch)]
//   row 2: live-data callback text, OR screen title, OR field 2 on 3-field screens
//          cols 17–19 always hold the passive alert indicator
//   row 3: [BACK] ············ [SAVE]
//
// To use more than 4 submenus, define the capacity constants BEFORE #include:
//   #define APA_LCD_MAX_SCREENS        8   // default 4  — total screens (left + right)
//   #define APA_LCD_ACTIVE_ALERT_QUEUE 5   // default 3  — max simultaneous active alerts

#pragma once

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

// ---- Optional DS3231 RTC support --------------------------------------------
// Add -DAPA_LCD_USE_DS3231 to build_flags (PlatformIO) or define it before
// this #include (Arduino IDE) to compile in the time/date edit modal.
// The library includes Wire.h and DS3231.h automatically when the flag is set.
#ifdef APA_LCD_USE_DS3231
#include <Wire.h>
#include <DS3231.h>
#endif

// ---- Version ----------------------------------------------------------------
#define APALCDGUI_VERSION "1.1.5"

// ---- Capacity — define BEFORE #include to override --------------------------
// These control compile-time array sizes; defining them after #include has no effect.
#ifndef APA_LCD_MAX_SCREENS
#define APA_LCD_MAX_SCREENS 4          // total submenus (left + right combined)
#endif

#ifndef APA_LCD_ACTIVE_ALERT_QUEUE
#define APA_LCD_ACTIVE_ALERT_QUEUE 3   // max simultaneous active alerts in the queue
#endif

#ifndef APA_LCD_MAX_HOME_SCREENS
#define APA_LCD_MAX_HOME_SCREENS 4     // max home screen pages scrolled by KB2
#endif

// ---- EEPROM base address — define before #include only if address 500 collides -----------
#ifndef APA_LCD_EEPROM_ADDR
#define APA_LCD_EEPROM_ADDR 500        // 2 bytes used: brightness(500) + validity marker(501)
#endif

// ---- Timing -----------------------------------------------------------------
constexpr uint32_t APALCDGUI_MENU_TIMEOUT_MS    = 60000UL;  // idle → HOME
constexpr uint32_t APALCDGUI_BL_OFF_MS          = 300000UL; // active → off (5 min); dim fires at 40% of this
constexpr uint32_t APALCDGUI_LONG_PRESS_MS      = 800UL;    // long-press threshold
constexpr uint32_t APALCDGUI_BOTH_PRESS_MS      = 200UL;    // both-pressed window
constexpr uint32_t APALCDGUI_FLASH_SAVE_MS      = 500UL;    // SAVE flash duration
constexpr uint32_t APALCDGUI_FLASH_BACK_MS      = 300UL;    // BACK flash duration
constexpr uint32_t APALCDGUI_ALERT_FLASH_MS     = 500UL;    // critical blink period
constexpr uint8_t  APALCDGUI_BTN_DEBOUNCE_MS    = 50;       // button debounce window
constexpr uint8_t  APALCDGUI_BRIGHTNESS_DEFAULT = 200;      // active PWM (0-255)
constexpr uint8_t  APALCDGUI_BRIGHTNESS_DIM     = 50;       // dim PWM
constexpr uint8_t  APALCDGUI_BRIGHTNESS_STEP    = 10;       // per click in brightness adjust
constexpr uint8_t  APALCDGUI_EEPROM_MARKER      = 0xAE;     // EEPROM valid marker

// ---- Custom character slot IDs -----------------------------------------------
// The library pre-loads slots 0–6 into the LCD's custom character memory.
// Use CC_DEGREE in your home screen callback: lcd.write(CC_DEGREE)
// Slot 7 is free for your own custom character via lcd.createChar(7, ...).
constexpr uint8_t CC_CURSOR_EDIT = 0;  // ► edit cursor shown while a field or action is selected
constexpr uint8_t CC_DEGREE      = 4;  // ° degree symbol — useful for temperature display

// ---- Public enums -----------------------------------------------------------

/** Field behaviour type.
 *  Do not fill FieldDef by hand — use the factory helpers (fieldInt, fieldFloat, etc.)
 *  which set all required fields to safe defaults for you. */
enum FieldType : uint8_t {
    FIELD_INT,       // int16_t value — operator rotates knob2 to scroll between min and max by step
    FIELD_FLOAT,     // float value   — same as INT but displayed with a chosen number of decimal places
    FIELD_CHOICE,    // uint8_t index — operator rotates to cycle through a string array (e.g. AUTO/MANU/OFF)
    FIELD_BOOL,      // bool toggle   — shown as " ON " or "OFF "; rotate to preview, press to commit
    FIELD_ACTION,    // button        — shows "STRT" on SAVE; use for pump tests, resets, etc.
    FIELD_READONLY   // display only  — cursor skips this field entirely; no editing possible
};

/** Which side of HOME a submenu screen sits on.
 *  SCREEN_RIGHT: operator reaches it by rotating knob1 clockwise.
 *  SCREEN_LEFT:  operator reaches it by rotating knob1 counter-clockwise. */
enum ScreenSide : uint8_t { SCREEN_RIGHT, SCREEN_LEFT };

/** Severity level for postAlert() and postActiveAlert().
 *  Controls the indicator symbol and flash behaviour. */
enum AlertLevel : uint8_t {
    ALERT_INFO,      // [i] — informational, steady
    ALERT_WARNING,   // [*] — warning, steady
    ALERT_CRITICAL   // [!] — critical, flashing
};

// ---- Field definition -------------------------------------------------------

/** Describes one data field on a submenu screen.
 *  Never fill this struct by hand — use the factory helpers instead:
 *    APALCDGUI::fieldInt(), fieldFloat(), fieldChoice(), fieldBool(),
 *    fieldAction(), fieldReadonly()
 *  They set all required members correctly and leave unused ones at safe defaults. */
struct FieldDef {
    FieldType                   type;
    const __FlashStringHelper*  label;    // field name shown in cols 1–12; max 12 chars (F("...") — stored in flash)
    const __FlashStringHelper*  unit;     // unit suffix in cols 18–19; max 2 chars, or nullptr
    void*                       valuePtr; // pointer to the variable being edited: int16_t* / float* / uint8_t* / bool*
    float                       minVal;   // lowest allowed value (FIELD_INT and FIELD_FLOAT only)
    float                       maxVal;   // highest allowed value (FIELD_INT and FIELD_FLOAT only)
    float                       step;     // change per encoder click (e.g. 1, 0.1, 10)
    uint8_t                     decimals; // digits after the decimal point for FIELD_FLOAT display
    const char**                choices;  // FIELD_CHOICE: null-terminated array of 4-char strings
    void                      (*action)(); // FIELD_ACTION: function to call on SAVE; nullptr shows "-no-"
    bool                        confirm;   // FIELD_ACTION: if true, show a confirmation prompt before firing
};

// ---- Main class -------------------------------------------------------------

class APALCDGUI {
public:

    // ---- Initialisation -----------------------------------------------------

    /** Construct with LCD pin assignments.
     *  All defaults match the APA Devices HMI board v1.0, so just write:
     *    APALCDGUI gui;
     *  For a custom wiring pass all six pins:
     *    APALCDGUI gui(rs, en, d4, d5, d6, d7);
     *  The LCD pins are stored here; the LCD is not started until begin(). */
    APALCDGUI(
        uint8_t rs=26, uint8_t en=27,
        uint8_t d4=22, uint8_t d5=23, uint8_t d6=24, uint8_t d7=25
    );

    /** Initialise the LCD, register encoder interrupts, load custom characters,
     *  and restore the saved brightness from EEPROM.
     *  All defaults match the APA Devices HMI board v1.0.
     *  Call once in setup() — must be called before addScreen() or update().
     *
     *  enc1 = right knob (knob1 / KB1) — used for screen navigation and brightness.
     *  enc2 = left  knob (knob2 / KB2) — used for cursor movement and value editing.
     *  encNDetents: encoder pulses per physical click — 4 is correct for PEC11R encoders. */
    void begin(
        uint8_t blPin = 4,
        uint8_t enc1Clk = 2,  uint8_t enc1Dt = 3,  uint8_t enc1Btn = 10,
        uint8_t enc2Clk = 19, uint8_t enc2Dt = 18, uint8_t enc2Btn = 11,
        uint8_t enc1Detents = 4,
        uint8_t enc2Detents = 4
    );

    /** Run all GUI logic: read encoders, process button presses, handle timeouts,
     *  and redraw the LCD when needed.
     *  Must be called on every loop() iteration — it never blocks and never calls delay(). */
    void update();

    // ---- Screen registration ------------------------------------------------

    /** Register a home screen page.
     *  Call once for a single home screen or multiple times for a scrollable dashboard —
     *  KB2 rotation cycles through the pages when more than one is registered.
     *  fn is called on every update() while that page is shown — keep it fast, no delay().
     *  The LiquidCrystal& parameter is the only callback that receives a parameter;
     *  all other callbacks (onSave, ackCallback, long-press) are plain void(void).
     *  Returns false if APA_LCD_MAX_HOME_SCREENS is already reached. */
    bool addHomeScreen(void (*fn)(LiquidCrystal& lcd));

    /** Alias for addHomeScreen() — single-screen sketches need no changes. */
    void setHomeCallback(void (*fn)(LiquidCrystal& lcd));

    /** Set a live-data callback drawn on row 2 of every 1- and 2-field submenu screen.
     *  Use it to show a sensor reading or status line that updates while the operator
     *  is in the menu — for example: "pH 7.24  ORP 682mV".
     *  Write to cols 0–16 only; cols 17–19 are reserved for the alert indicator.
     *  During the last 10 s before the menu timeout, the row shows a countdown instead.
     *  Not called on 3-field screens (row 2 is occupied by the third field). */
    void setMenuRow2Callback(void (*fn)());

    /** Register a 1-field submenu screen.
     *  title: optional heading text shown on row 2 (replaces setMenuRow2Callback output).
     *  Returns false if APA_LCD_MAX_SCREENS is already reached. */
    bool addScreen(ScreenSide side,
                   const FieldDef& field1,
                   void (*onSave)() = nullptr,
                   const __FlashStringHelper* title = nullptr);

    /** Register a 2-field submenu screen — the most common screen type.
     *  Each screen shows two editable fields on rows 0 and 1.
     *  SAVE: commits all edits, calls onSave(), and shows "Settings saved!" for 1 second.
     *  BACK: discards any in-progress edit and returns to navigation — onSave is NOT called.
     *  title: optional heading text shown on row 2.
     *  Returns false if APA_LCD_MAX_SCREENS is already reached. */
    bool addScreen(ScreenSide side,
                   const FieldDef& field1,
                   const FieldDef& field2,
                   void (*onSave)() = nullptr,
                   const __FlashStringHelper* title = nullptr);

    /** Register a 3-field submenu screen.
     *  The third field occupies row 2, so setMenuRow2Callback output and title are suppressed.
     *  The alert indicator (cols 17–19) is still shown on row 2.
     *  Returns false if APA_LCD_MAX_SCREENS is already reached. */
    bool addScreen(ScreenSide side,
                   const FieldDef& field1,
                   const FieldDef& field2,
                   const FieldDef& field3,
                   void (*onSave)() = nullptr,
                   const __FlashStringHelper* title = nullptr);

    // ---- RTC modal (optional — include DS3231.h before this header) ---------
#ifdef APA_LCD_USE_DS3231
    /** Wire both-buttons-pressed to the built-in time/date edit modal.
     *  Ignored if setBothPressedCallback() has been registered.
     *  DS3231.h must be included before APALCDGUI.h. */
    void setRTC(DS3231* rtc);
#endif

    // ---- Factory helpers — build a FieldDef without filling every member ----

    /** INT field: operator rotates knob2 to change an integer between minVal and maxVal.
     *  val:   pointer to the int16_t variable to edit (updated on SAVE, discarded on BACK).
     *  unit:  2-char suffix displayed after the value, e.g. F("mV"), F("h"), or nullptr.
     *  step:  amount to change per encoder click (default 1). */
    static FieldDef fieldInt(const __FlashStringHelper* label,
                             const __FlashStringHelper* unit,
                             int16_t* val,
                             int16_t minVal, int16_t maxVal,
                             int16_t step = 1);

    /** FLOAT field: same as INT but for a float variable, shown with a fixed number of
     *  decimal places. step and decimals are independent — e.g. step=0.01, decimals=2. */
    static FieldDef fieldFloat(const __FlashStringHelper* label,
                               const __FlashStringHelper* unit,
                               float* val,
                               float minVal, float maxVal,
                               float step, uint8_t decimals);

    /** CHOICE field: operator rotates to cycle through a list of named options.
     *  idx:     pointer to a uint8_t that stores the selected index (0, 1, 2, ...).
     *  choices: null-terminated array of option strings. Each string MUST be exactly
     *           4 characters — the value area on the LCD is 4 columns wide. Pad shorter
     *           strings with trailing spaces: {"AUTO", "MANU", "OFF ", nullptr}.
     *  The array must live in static or global memory for the lifetime of the screen. */
    static FieldDef fieldChoice(const __FlashStringHelper* label,
                                uint8_t* idx,
                                const char** choices);

    /** BOOL field: a simple on/off toggle stored in a bool variable.
     *  Displayed as " ON " (true) or "OFF " (false) in the 4-character value area.
     *  To edit: press knob2 to enter edit mode, rotate to preview the new state,
     *  then press knob2 again to commit. Pressing BACK restores the original value. */
    static FieldDef fieldBool(const __FlashStringHelper* label, bool* val);

    /** ACTION field: pressing SAVE on this field immediately calls action().
     *  The value area shows "STRT" when action is non-null, or "-no-" when nullptr.
     *  confirm=true: before calling action, the library shows a full-screen prompt:
     *    "Confirm action?" with KB1=NO and KB2=YES on row 3.
     *    Use confirm=true for irreversible operations such as factory reset or pump priming. */
    static FieldDef fieldAction(const __FlashStringHelper* label,
                                void (*action)() = nullptr,
                                bool confirm = false);

    /** READONLY field: displays a float value with fixed decimal places.
     *  The cursor skips this field automatically — knob2 moves straight past it
     *  to BACK or SAVE. No editing is possible. Useful for showing live sensor data
     *  (uptime, dose total, firmware version) alongside editable fields. */
    static FieldDef fieldReadonly(const __FlashStringHelper* label,
                                  const __FlashStringHelper* unit,
                                  float* val, uint8_t decimals);

    // ---- Gesture callbacks --------------------------------------------------

    /** Register a callback fired when an encoder button is held for 800 ms.
     *  encoder: 0 = right knob button (KB1 / knob1)
     *           1 = left  knob button (KB2 / knob2)
     *  Built-in KB2 behaviour (when no callback is registered for encoder 1):
     *    if a passive alert is active, the alert text is shown as a 3-second
     *    message overlay, then the alert is automatically cleared. */
    void setLongPressCallback(uint8_t encoder, void (*fn)());

    /** Register a callback fired when both buttons are pressed within 200 ms.
     *  Always takes priority over the RTC modal set by setRTC(). */
    void setBothPressedCallback(void (*fn)());

    // ---- Backlight ----------------------------------------------------------

    /** Force the backlight on or off immediately, overriding the inactivity timer. */
    void setBacklight(bool on);

    /** Set how many seconds of inactivity before the backlight turns off.
     *  The backlight dims to 20% at 40% of this time, then turns off at the full value.
     *  0 = always on (no timeout). Default: 300 s (5 minutes). */
    void setBacklightTimeout(uint16_t seconds);

    // ---- Menu timeout -------------------------------------------------------

    /** Set how many seconds of inactivity before the library returns to HOME automatically.
     *  During the last 10 s, row 2 of the menu shows a countdown warning.
     *  0 = timeout disabled. Default: 60 s. */
    void setMenuTimeout(uint16_t seconds);

    // ---- Message overlay ----------------------------------------------------

    /** Show a timed 2-line message that temporarily covers rows 1 and 2 of the screen.
     *  The previous screen content is restored automatically after ms milliseconds.
     *  line1: top message line, up to 20 characters.
     *  line2: bottom message line, or nullptr to leave it blank.
     *  ms:    how long to display the message (default 1500 ms). */
    void showMessage(const char* line1, const char* line2 = nullptr, uint16_t ms = 1500);
    void showMessage(const __FlashStringHelper* line1,
                     const __FlashStringHelper* line2 = nullptr, uint16_t ms = 1500);

    /** Dismiss a showMessage() overlay immediately before its timer expires. */
    void clearMessage();

    // ---- Display control ----------------------------------------------------

    /** Schedule a full LCD redraw on the next update().
     *  Call this when application data displayed on the current screen changes
     *  outside of a user edit — for example when a background task updates a
     *  sensor value shown by setMenuRow2Callback(). */
    void markDirty();

    // ---- Passive alerts (informational, non-blocking) -----------------------

    /** Show a non-blocking passive alert indicator in the top-right corner of every screen.
     *  The indicator appears at cols 17–19 row 2:
     *    [i] ALERT_INFO     — informational, shown steady
     *    [*] ALERT_WARNING  — warning, shown steady
     *    [!] ALERT_CRITICAL — critical, flashes on and off
     *  Navigation is not interrupted — the operator can still move between screens normally.
     *  line1 (up to 16 chars) and optional line2 are stored: when the operator long-presses KB2
     *  (and no custom long-press callback is registered), the text is shown as a full-screen
     *  message overlay for 3 seconds, then the indicator is cleared.
     *  Calling postAlert() again replaces any existing passive alert.
     *  Use F("...") for both lines to keep the strings in flash memory. */
    void postAlert(const char*                line1,
                   const char*                line2 = nullptr,
                   AlertLevel                 level = ALERT_INFO);
    void postAlert(const __FlashStringHelper*  line1,
                   const __FlashStringHelper*  line2 = nullptr,
                   AlertLevel                  level = ALERT_INFO);

    /** Clear the passive alert indicator and stored text. */
    void clearAlert();

    /** Returns true if a passive alert indicator is currently shown. */
    bool hasAlert() const;

    // ---- Active alerts (latching, require operator acknowledgment) ----------

    /** Show a full-screen alarm that blocks the home screen until acknowledged.
     *  While active, returning to HOME shows the alarm instead of the home callback:
     *    row 0: centred heading (line1, up to 16 chars)
     *    row 1: detail text    (line2, up to 16 chars)
     *    row 3: "KB2:ACK !" instruction
     *  The operator can still navigate to submenus; the alarm reappears when they
     *  return to HOME. KB2 press acknowledges: fires ackCallback, frees the slot,
     *  and advances to the next queued alert (if any).
     *  Queue holds APA_LCD_ACTIVE_ALERT_QUEUE slots; excess alerts are silently dropped.
     *  Use F("...") for both lines to keep the strings in flash memory. */
    void postActiveAlert(const char*                line1,
                         const char*                line2,
                         AlertLevel                 level,
                         void                     (*ackCallback)());
    void postActiveAlert(const __FlashStringHelper*  line1,
                         const __FlashStringHelper*  line2,
                         AlertLevel                  level,
                         void                      (*ackCallback)());

    /** Dismiss the current active alert without calling its ackCallback.
     *  Use this when the alarm condition has cleared automatically (not by operator action),
     *  for example when a sensor reading returns to a safe range. */
    void cancelActiveAlert();

    /** Returns true if at least one active alert is queued and waiting for acknowledgment. */
    bool hasActiveAlert() const;

    /** Returns the number of active alerts currently in the queue. */
    uint8_t activeAlertCount() const;

    // ---- State queries ------------------------------------------------------

    /** Returns the 0-based index of the home screen page currently shown.
     *  Use inside your home callback to draw a page indicator:
     *    if (gui.homePageCount() > 1)
     *        snprintf(buf, ..., "%d/%d", gui.currentHomePage()+1, gui.homePageCount()); */
    uint8_t currentHomePage() const;

    /** Returns the number of home screen pages registered with addHomeScreen(). */
    uint8_t homePageCount() const;

    /** Returns true when the operator is in any submenu screen (not at HOME). */
    bool isMenuActive() const;

    /** Returns true while a field value or RTC field is currently being edited. */
    bool isEditActive() const;

    /** Returns the current knob1 (screen navigation) position.
     *  0 = HOME, +1/+2/... = right screens in order, -1/-2/... = left screens. */
    int8_t currentScreen() const;

    /** Returns the current backlight brightness level (0–255).
     *  This value is saved to EEPROM and restored on power-on. */
    uint8_t getBrightness() const;

private:
    // ---- Private enums ------------------------------------------------------
    enum UIState : uint8_t {
        ST_HOME, ST_NAV, ST_EDIT,
        ST_FLASH_SAVE, ST_FLASH_BACK,
        ST_BRIGHTNESS,
        ST_CONFIRM,
        ST_RTC_NAV, ST_RTC_EDIT
    };
    enum BlStage : uint8_t { BL_ACTIVE, BL_DIM, BL_OFF };

    // ---- LCD dimensions (private — no global namespace pollution) -----------
    static constexpr uint8_t COLS = 20;
    static constexpr uint8_t ROWS = 4;

    // ---- Internal structs ---------------------------------------------------
    struct Enc {
        uint8_t          clk, dt, btn, detents;
        uint8_t          prevState;          // 2-bit previous quadrature state
        volatile int32_t pos;                // raw ISR position accumulator
        uint32_t         btnMs;              // timestamp of last button state change
        uint8_t          btnHeld;            // 1 = button currently pressed
        bool             longFired;          // long-press already fired this hold
        bool             pressed;            // ONE-SHOT: true on press edge
        bool             released;           // ONE-SHOT: true on release edge
    };

    struct Screen {
        FieldDef                     field[3];   // up to 3 fields; only fieldCount slots are used
        void                       (*onSave)();
        ScreenSide                   side;
        uint8_t                      fieldCount; // 1, 2, or 3
        const __FlashStringHelper*   title;      // shown on row 2 when no row2Cb; nullptr = off
    };

    struct AlertSlot {
        char        l1[17], l2[17];          // message lines (16 chars + NUL)
        AlertLevel  level;
        void      (*ackCb)();
        bool        used;
    };

    // ---- LCD object — constructed with pins passed to APALCDGUI() ----------
    LiquidCrystal _lcd;

    // ---- Hardware -----------------------------------------------------------
    bool     _ready;                         // true after begin()
    uint8_t  _blPin;
    Enc      _enc[2];                        // [0]=knob1 (right/KB1 navigation), [1]=knob2 (left/KB2 edit)

    // ---- Screen registry ----------------------------------------------------
    Screen   _screens[APA_LCD_MAX_SCREENS];
    uint8_t  _nScreens;
    void   (*_homeCbs[APA_LCD_MAX_HOME_SCREENS])(LiquidCrystal&);
    uint8_t  _nHomeCbs;   // number of registered home pages
    uint8_t  _homeIdx;    // currently displayed home page (0-based)
    void   (*_row2Cb)();

    // ---- UI state -----------------------------------------------------------
    UIState  _state;
    int8_t   _scrPos;   // 0=HOME, +n=right screen n, -n=left screen n
    uint8_t  _curPos;   // 0..fieldCount-1 = fields; fieldCount = BACK; fieldCount+1 = SAVE
    uint32_t _stateMs;  // millis() when current state was entered
    uint32_t _inputMs;  // millis() of last any-input (feeds both timeouts)
    uint16_t _menuSec;  // menu timeout in seconds (0 = disabled)
    uint8_t  _timeoutWarnSec; // last countdown value written (0xFF = not in warning window)
    bool     _dirty;    // full screen redraw requested

    // ---- Edit ---------------------------------------------------------------
    float    _editVal;  // working copy while editing — committed on SAVE, discarded on BACK
    uint8_t  _editIdx;  // which field is being edited: 0, 1, or 2

    // ---- Backlight ----------------------------------------------------------
    BlStage  _blStage;
    uint16_t _blOffSec; // off timeout in seconds (0 = always on)
    uint8_t  _bright;   // current target brightness (EEPROM-stored)

    // ---- Message overlay ----------------------------------------------------
    char     _msgL1[COLS + 1], _msgL2[COLS + 1];
    bool     _msgActive;
    uint32_t _msgUntilMs;

    // ---- Passive alert ------------------------------------------------------
    char       _passL1[17], _passL2[17];     // stored for built-in KB2 long-press detail view
    AlertLevel _passLevel;
    bool       _passActive;
    bool       _passFlash;   // current flash state for CRITICAL level
    uint32_t   _passFlashMs; // last flash toggle timestamp

    // ---- Active alert queue -------------------------------------------------
    AlertSlot _alertQ[APA_LCD_ACTIVE_ALERT_QUEUE];
    uint8_t   _alertHead;   // index of the alert currently displayed

    // ---- Gestures -----------------------------------------------------------
    void   (*_longCb[2])();  // long-press callbacks for enc[0] and enc[1]
    void   (*_bothCb)();     // both-pressed callback (overrides RTC)
    uint32_t  _bothMs;       // when both buttons were first detected pressed
    bool      _bothArmed;    // both are currently held
    bool      _bothFired;    // threshold already fired this press — no double-fire

    // ---- ACTION confirmation ------------------------------------------------
    void                       (*_pendingAction)();
    const __FlashStringHelper*   _pendingLabel;

    // ---- RTC (no-op if DS3231.h not included) -------------------------------
    void*    _rtcPtr;        // DS3231* stored as void* to avoid header dependency
    uint8_t  _rtcSub;        // 0 = TIME sub-screen, 1 = DATE sub-screen
    uint8_t  _rtcCur;        // cursor: 0-2 = field, 3 = BACK, 4 = SAVE
    int16_t  _rtcVal[2][3];  // editable copies: [TIME][H,M,S] / [DATE][D,M,Y]

    // ---- ISR singleton (one LCD + two encoders per board) -------------------
    static APALCDGUI* _inst;
    static void _isr0();
    static void _isr1();
    void _encISR(uint8_t i); // called from ISR — reads pins, updates pos

    // ---- Private methods ----------------------------------------------------
    void _pollBtns();                        // debounce + edge detection
    int32_t _encClicks(uint8_t i);           // atomic read + reset, returns delta clicks
    void    _setState(UIState s);            // transition + flush stale encoder counts
    void    _touchInput();                   // stamp _inputMs, wake backlight

    void _stateHome();
    void _stateNav();
    void _stateEdit();
    void _stateFlashSave();
    void _stateFlashBack();
    void _stateBrightness();
    void _stateConfirm();
    void _stateRTCNav();
    void _stateRTCEdit();

    void _checkBothPress();
    void _checkMenuTimeout();
    void _checkLongPress();
    void _blUpdate();

    void _renderMenu();
    void _renderField(uint8_t row, const FieldDef& f, bool cursor, bool editing);
    void _renderActionBar();
    void _renderAlertScreen();
    void _renderPassiveCorner();
    void _renderHome();

    void _rowWrite(uint8_t row, const char* text);           // write COLS chars, space-pad
    void _padWrite(uint8_t col, uint8_t row,                 // write width chars, space-pad
                   const char* text, uint8_t width);

    void _blSet(uint8_t val);
    void _loadEEPROM();
    void _saveEEPROM();

    uint8_t       _countSide(ScreenSide side) const;
    const Screen* _curScreen() const;                        // nullptr when _scrPos == 0
    uint8_t       _alertCount() const;
    uint8_t       _nextCurPos(uint8_t cur, int8_t dir) const; // wraps, skips READONLY

    static void _fstr(char* dst, const __FlashStringHelper* src, uint8_t maxLen);
    void _initCustomChars();
};
