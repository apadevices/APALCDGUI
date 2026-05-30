#include "APALCDGUI.h"
#include <string.h>
#include <stdio.h>

// ---- Custom character bitmaps (internal — slot IDs are public in the header) ----
static const uint8_t s_ccFilledRight[8] PROGMEM = {0x10,0x18,0x1C,0x1E,0x1C,0x18,0x10,0x00}; // slot 0 ►
static const uint8_t s_ccArrowRight[8]  PROGMEM = {0x00,0x04,0x06,0x1F,0x06,0x04,0x00,0x00}; // slot 1 →
static const uint8_t s_ccArrowLeft[8]   PROGMEM = {0x00,0x04,0x0C,0x1F,0x0C,0x04,0x00,0x00}; // slot 2 ←
static const uint8_t s_ccArrowBoth[8]   PROGMEM = {0x04,0x0E,0x1F,0x00,0x1F,0x0E,0x04,0x00}; // slot 3 ↕
static const uint8_t s_ccDegree[8]      PROGMEM = {0x06,0x09,0x09,0x06,0x00,0x00,0x00,0x00}; // slot 4 °
static const uint8_t s_ccUp[8]          PROGMEM = {0x04,0x0E,0x1F,0x04,0x04,0x04,0x04,0x00}; // slot 5 ↑
static const uint8_t s_ccDown[8]        PROGMEM = {0x04,0x04,0x04,0x04,0x1F,0x0E,0x04,0x00}; // slot 6 ↓

// ---- 4-bit quadrature decode table ------------------------------------------
// Index = (prevState << 2) | newState. Values: direction delta.
static const int8_t ENC_TABLE[16] PROGMEM = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

// ---- Singleton --------------------------------------------------------------
APALCDGUI* APALCDGUI::_inst = nullptr;

// ---- Constructor ------------------------------------------------------------
APALCDGUI::APALCDGUI(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
    : _lcd(rs, en, d4, d5, d6, d7)
{}

// ---- ISR stubs --------------------------------------------------------------
void APALCDGUI::_isr0() { _inst->_encISR(0); }
void APALCDGUI::_isr1() { _inst->_encISR(1); }

void APALCDGUI::_encISR(uint8_t i) {
    uint8_t clk      = digitalRead(_enc[i].clk);
    uint8_t dt       = digitalRead(_enc[i].dt);
    uint8_t newState = (clk << 1) | dt;
    uint8_t key      = (_enc[i].prevState << 2) | newState;
    _enc[i].prevState = newState;
    _enc[i].pos      += (int8_t)pgm_read_byte(&ENC_TABLE[key]);
}

// ---- Flash string copy helper -----------------------------------------------
void APALCDGUI::_fstr(char* dst, const __FlashStringHelper* src, uint8_t maxLen) {
    if (!src) { dst[0] = '\0'; return; }
#ifdef __AVR__
    strncpy_P(dst, (const char*)src, maxLen);
#else
    strncpy(dst,  (const char*)src, maxLen);
#endif
    dst[maxLen] = '\0';
}

// ---- Custom character init --------------------------------------------------
void APALCDGUI::_initCustomChars() {
    uint8_t buf[8];
    const uint8_t* tbls[7] = {
        s_ccFilledRight, s_ccArrowRight, s_ccArrowLeft, s_ccArrowBoth,
        s_ccDegree, s_ccUp, s_ccDown
    };
    for (uint8_t s = 0; s < 7; s++) {
        for (uint8_t r = 0; r < 8; r++) buf[r] = pgm_read_byte(&tbls[s][r]);
        _lcd.createChar(s, buf);
    }
}

// ---- EEPROM -----------------------------------------------------------------
void APALCDGUI::_loadEEPROM() {
#if defined(ESP32) || defined(ESP8266)
    EEPROM.begin(APA_LCD_EEPROM_ADDR + 2);
#endif
    if (EEPROM.read(APA_LCD_EEPROM_ADDR + 1) == APALCDGUI_EEPROM_MARKER)
        _bright = EEPROM.read(APA_LCD_EEPROM_ADDR);
    else
        _bright = APALCDGUI_BRIGHTNESS_DEFAULT;
}

void APALCDGUI::_saveEEPROM() {
#if defined(ESP32) || defined(ESP8266)
    EEPROM.begin(APA_LCD_EEPROM_ADDR + 2);
    EEPROM.write(APA_LCD_EEPROM_ADDR,     _bright);
    EEPROM.write(APA_LCD_EEPROM_ADDR + 1, APALCDGUI_EEPROM_MARKER);
    EEPROM.commit();
#else
    EEPROM.update(APA_LCD_EEPROM_ADDR,     _bright);   // update() skips write if value unchanged
    EEPROM.update(APA_LCD_EEPROM_ADDR + 1, APALCDGUI_EEPROM_MARKER);
#endif
}

// ---- Backlight helpers ------------------------------------------------------
void APALCDGUI::_blSet(uint8_t val) { analogWrite(_blPin, val); }

void APALCDGUI::_blUpdate() {
    if (_blOffSec == 0) return;
    uint32_t elapsed = millis() - _inputMs;
    uint32_t offMs   = (uint32_t)_blOffSec * 1000UL;
    uint32_t dimMs   = offMs * 2 / 5;
    if (_blStage == BL_ACTIVE && elapsed >= dimMs) {
        _blStage = BL_DIM;
        _blSet(APALCDGUI_BRIGHTNESS_DIM);
    } else if (_blStage == BL_DIM && !hasActiveAlert() && elapsed >= offMs) {
        _blStage = BL_OFF;
        _blSet(0);
    }
}

// ---- Internal helpers -------------------------------------------------------
void APALCDGUI::_touchInput() {
    _inputMs = millis();
    if (_blStage != BL_ACTIVE) {
        _blStage = BL_ACTIVE;
        _blSet(_bright);
    }
}

int32_t APALCDGUI::_encClicks(uint8_t i) {
    noInterrupts();
    int32_t raw = _enc[i].pos;
    int8_t  det = _enc[i].detents ? _enc[i].detents : 4;
    _enc[i].pos = raw % det;
    interrupts();
    return raw / det;
}

void APALCDGUI::_setState(UIState s) {
    // Flush stale encoder counts on state transition
    noInterrupts(); _enc[0].pos = 0; _enc[1].pos = 0; interrupts();
    _state   = s;
    _stateMs = millis();
    _dirty   = true;
}

uint8_t APALCDGUI::_countSide(ScreenSide side) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _nScreens; i++) if (_screens[i].side == side) n++;
    return n;
}

const APALCDGUI::Screen* APALCDGUI::_curScreen() const {
    if (_scrPos == 0) return nullptr;
    bool right = (_scrPos > 0);
    ScreenSide want = right ? SCREEN_RIGHT : SCREEN_LEFT;
    int8_t idx = right ? _scrPos : -_scrPos;
    uint8_t count = 0;
    for (uint8_t i = 0; i < _nScreens; i++) {
        if (_screens[i].side == want) {
            count++;
            if (count == (uint8_t)idx) return &_screens[i];
        }
    }
    return nullptr;
}

uint8_t APALCDGUI::_nextCurPos(uint8_t cur, int8_t dir) const {
    const Screen* s = _curScreen();
    if (!s) return 0;
    uint8_t total = s->fieldCount + 2; // fields + BACK + SAVE
    for (uint8_t attempt = 0; attempt < total; attempt++) {
        if (dir > 0) cur = (cur + 1) % total;
        else         cur = (cur == 0) ? (total - 1) : (cur - 1);
        // skip READONLY fields (positions 0..fieldCount-1)
        if (cur < s->fieldCount && s->field[cur].type == FIELD_READONLY) continue;
        return cur;
    }
    return cur;
}

uint8_t APALCDGUI::_alertCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < APA_LCD_ACTIVE_ALERT_QUEUE; i++) if (_alertQ[i].used) n++;
    return n;
}

// ---- LCD write helpers ------------------------------------------------------
void APALCDGUI::_rowWrite(uint8_t row, const char* text) {
    _lcd.setCursor(0, row);
    uint8_t i = 0;
    while (text[i] && i < COLS) { _lcd.write((uint8_t)text[i]); i++; }
    while (i < COLS)            { _lcd.write(' '); i++; }
}

void APALCDGUI::_padWrite(uint8_t col, uint8_t row, const char* text, uint8_t width) {
    _lcd.setCursor(col, row);
    uint8_t i = 0;
    while (text[i] && i < width) { _lcd.write((uint8_t)text[i]); i++; }
    while (i < width)            { _lcd.write(' '); i++; }
}

// ---- begin() ----------------------------------------------------------------
void APALCDGUI::begin(
    uint8_t blPin,
    uint8_t enc1Clk, uint8_t enc1Dt, uint8_t enc1Btn,
    uint8_t enc2Clk, uint8_t enc2Dt, uint8_t enc2Btn,
    uint8_t enc1Detents, uint8_t enc2Detents)
{
    _inst = this;
    _lcd.begin(COLS, ROWS);

    _blPin = blPin;
    pinMode(_blPin, OUTPUT);
    _loadEEPROM();
    _blSet(_bright);
    _blStage  = BL_ACTIVE;
    _blOffSec = APALCDGUI_BL_OFF_MS / 1000;

    // Encoder 0
    _enc[0].clk = enc1Clk; _enc[0].dt = enc1Dt; _enc[0].btn = enc1Btn;
    _enc[0].detents = enc1Detents; _enc[0].pos = 0; _enc[0].btnHeld = 0;
    _enc[0].longFired = false; _enc[0].pressed = false; _enc[0].released = false;
    pinMode(enc1Clk, INPUT_PULLUP); pinMode(enc1Dt, INPUT_PULLUP); pinMode(enc1Btn, INPUT_PULLUP);
    _enc[0].prevState = (digitalRead(enc1Clk) << 1) | digitalRead(enc1Dt);
    attachInterrupt(digitalPinToInterrupt(enc1Clk), _isr0, CHANGE);
    attachInterrupt(digitalPinToInterrupt(enc1Dt),  _isr0, CHANGE);

    // Encoder 1
    _enc[1].clk = enc2Clk; _enc[1].dt = enc2Dt; _enc[1].btn = enc2Btn;
    _enc[1].detents = enc2Detents; _enc[1].pos = 0; _enc[1].btnHeld = 0;
    _enc[1].longFired = false; _enc[1].pressed = false; _enc[1].released = false;
    pinMode(enc2Clk, INPUT_PULLUP); pinMode(enc2Dt, INPUT_PULLUP); pinMode(enc2Btn, INPUT_PULLUP);
    _enc[1].prevState = (digitalRead(enc2Clk) << 1) | digitalRead(enc2Dt);
    attachInterrupt(digitalPinToInterrupt(enc2Clk), _isr1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(enc2Dt),  _isr1, CHANGE);

    _initCustomChars();
    _lcd.clear();

    _state = ST_HOME; _scrPos = 0; _curPos = 0;
    _stateMs = millis(); _inputMs = millis();
    _menuSec = APALCDGUI_MENU_TIMEOUT_MS / 1000; _dirty = true;
    _editVal = 0.0f; _editIdx = 0;
    _nScreens = 0; _homeCb = nullptr; _row2Cb = nullptr;
    _timeoutWarnSec = 0xFF;
    _longCb[0] = nullptr; _longCb[1] = nullptr;
    _bothCb = nullptr; _bothArmed = false; _bothMs = 0;
    _msgActive = false; _msgUntilMs = 0;
    _passActive = false; _passFlash = false; _passFlashMs = 0;
    _passL1[0] = '\0'; _passL2[0] = '\0';
    _alertHead = 0;
    _pendingAction = nullptr; _pendingLabel = nullptr;
    _rtcPtr = nullptr; _rtcSub = 0; _rtcCur = 0;
    for (uint8_t i = 0; i < APA_LCD_ACTIVE_ALERT_QUEUE; i++) _alertQ[i].used = false;
    _ready = true;
}

// ---- Screen registration ---------------------------------------------------
void APALCDGUI::setHomeCallback(void (*fn)(LiquidCrystal& lcd)) { _homeCb = fn; }
void APALCDGUI::setMenuRow2Callback(void (*fn)()) { _row2Cb = fn; }

bool APALCDGUI::addScreen(ScreenSide side, const FieldDef& f1, void (*onSave)(),
                           const __FlashStringHelper* title) {
    if (_nScreens >= APA_LCD_MAX_SCREENS) return false;
    _screens[_nScreens].field[0]   = f1;
    _screens[_nScreens].onSave     = onSave;
    _screens[_nScreens].side       = side;
    _screens[_nScreens].fieldCount = 1;
    _screens[_nScreens].title      = title;
    _nScreens++;
    return true;
}

bool APALCDGUI::addScreen(ScreenSide side, const FieldDef& f1, const FieldDef& f2,
                           void (*onSave)(), const __FlashStringHelper* title) {
    if (_nScreens >= APA_LCD_MAX_SCREENS) return false;
    _screens[_nScreens].field[0]   = f1;
    _screens[_nScreens].field[1]   = f2;
    _screens[_nScreens].onSave     = onSave;
    _screens[_nScreens].side       = side;
    _screens[_nScreens].fieldCount = 2;
    _screens[_nScreens].title      = title;
    _nScreens++;
    return true;
}

bool APALCDGUI::addScreen(ScreenSide side, const FieldDef& f1, const FieldDef& f2,
                           const FieldDef& f3, void (*onSave)(),
                           const __FlashStringHelper* title) {
    if (_nScreens >= APA_LCD_MAX_SCREENS) return false;
    _screens[_nScreens].field[0]   = f1;
    _screens[_nScreens].field[1]   = f2;
    _screens[_nScreens].field[2]   = f3;
    _screens[_nScreens].onSave     = onSave;
    _screens[_nScreens].side       = side;
    _screens[_nScreens].fieldCount = 3;
    _screens[_nScreens].title      = title;
    _nScreens++;
    return true;
}

#ifdef __DS3231_h
void APALCDGUI::setRTC(DS3231* rtc) { _rtcPtr = (void*)rtc; }
#endif

// ---- Gesture callbacks -----------------------------------------------------
void APALCDGUI::setLongPressCallback(uint8_t enc, void (*fn)()) { if (enc < 2) _longCb[enc] = fn; }
void APALCDGUI::setBothPressedCallback(void (*fn)())            { _bothCb = fn; }

// ---- Backlight API ---------------------------------------------------------
void APALCDGUI::setBacklight(bool on) {
    _blStage = on ? BL_ACTIVE : BL_OFF;
    _blSet(on ? _bright : 0);
}
void APALCDGUI::setBacklightTimeout(uint16_t seconds) { _blOffSec = seconds; }

// ---- Menu timeout ----------------------------------------------------------
void APALCDGUI::setMenuTimeout(uint16_t seconds) { _menuSec = seconds; }

// ---- Message overlay -------------------------------------------------------
void APALCDGUI::showMessage(const char* l1, const char* l2, uint16_t ms) {
    strncpy(_msgL1, l1 ? l1 : "", COLS); _msgL1[COLS] = '\0';
    strncpy(_msgL2, l2 ? l2 : "", COLS); _msgL2[COLS] = '\0';
    _msgActive = true; _msgUntilMs = millis() + ms; _dirty = true;
}
void APALCDGUI::clearMessage() { _msgActive = false; _dirty = true; }

// ---- Display control -------------------------------------------------------
void APALCDGUI::markDirty() { _dirty = true; }

// ---- State queries ---------------------------------------------------------
bool    APALCDGUI::isMenuActive()  const { return _state != ST_HOME; }
bool    APALCDGUI::isEditActive()  const { return _state == ST_EDIT || _state == ST_RTC_EDIT; }
int8_t  APALCDGUI::currentScreen() const { return _scrPos; }
uint8_t APALCDGUI::getBrightness() const { return _bright; }

// ---- Passive alerts --------------------------------------------------------
void APALCDGUI::postAlert(const char* l1, const char* l2, AlertLevel level) {
    strncpy(_passL1, l1 ? l1 : "", 16); _passL1[16] = '\0';
    strncpy(_passL2, l2 ? l2 : "", 16); _passL2[16] = '\0';
    _passLevel = level; _passActive = true; _dirty = true;
}
void APALCDGUI::postAlert(const __FlashStringHelper* l1, const __FlashStringHelper* l2, AlertLevel level) {
    _fstr(_passL1, l1, 16); _fstr(_passL2, l2, 16);
    _passLevel = level; _passActive = true; _dirty = true;
}
void APALCDGUI::clearAlert()     { _passActive = false; _dirty = true; }
bool APALCDGUI::hasAlert() const  { return _passActive; }

// ---- Active alerts ---------------------------------------------------------
void APALCDGUI::postActiveAlert(const char* l1, const char* l2, AlertLevel level, void (*cb)()) {
    if (_alertCount() >= APA_LCD_ACTIVE_ALERT_QUEUE) return;
    for (uint8_t i = 0; i < APA_LCD_ACTIVE_ALERT_QUEUE; i++) {
        if (!_alertQ[i].used) {
            strncpy(_alertQ[i].l1, l1 ? l1 : "", 16); _alertQ[i].l1[16] = '\0';
            strncpy(_alertQ[i].l2, l2 ? l2 : "", 16); _alertQ[i].l2[16] = '\0';
            _alertQ[i].level = level; _alertQ[i].ackCb = cb; _alertQ[i].used = true;
            _dirty = true; return;
        }
    }
}
void APALCDGUI::postActiveAlert(const __FlashStringHelper* l1, const __FlashStringHelper* l2, AlertLevel level, void (*cb)()) {
    if (_alertCount() >= APA_LCD_ACTIVE_ALERT_QUEUE) return;
    for (uint8_t i = 0; i < APA_LCD_ACTIVE_ALERT_QUEUE; i++) {
        if (!_alertQ[i].used) {
            _fstr(_alertQ[i].l1, l1, 16); _fstr(_alertQ[i].l2, l2, 16);
            _alertQ[i].level = level; _alertQ[i].ackCb = cb; _alertQ[i].used = true;
            _dirty = true; return;
        }
    }
}
void APALCDGUI::cancelActiveAlert() {
    if (!_alertQ[_alertHead].used) return;
    _alertQ[_alertHead].used = false;
    for (uint8_t i = 0; i < APA_LCD_ACTIVE_ALERT_QUEUE; i++) {
        _alertHead = (_alertHead + 1) % APA_LCD_ACTIVE_ALERT_QUEUE;
        if (_alertQ[_alertHead].used) break;
    }
    _dirty = true;
}
bool    APALCDGUI::hasActiveAlert()   const { return _alertCount() > 0; }
uint8_t APALCDGUI::activeAlertCount() const { return _alertCount(); }

// ---- Factory helpers --------------------------------------------------------
FieldDef APALCDGUI::fieldInt(const __FlashStringHelper* label, const __FlashStringHelper* unit,
                              int16_t* val, int16_t minVal, int16_t maxVal, int16_t step) {
    FieldDef f{};
    f.type = FIELD_INT; f.label = label; f.unit = unit; f.valuePtr = val;
    f.minVal = (float)minVal; f.maxVal = (float)maxVal; f.step = (float)step;
    return f;
}
FieldDef APALCDGUI::fieldFloat(const __FlashStringHelper* label, const __FlashStringHelper* unit,
                                float* val, float minVal, float maxVal, float step, uint8_t decimals) {
    FieldDef f{};
    f.type = FIELD_FLOAT; f.label = label; f.unit = unit; f.valuePtr = val;
    f.minVal = minVal; f.maxVal = maxVal; f.step = step; f.decimals = decimals;
    return f;
}
FieldDef APALCDGUI::fieldChoice(const __FlashStringHelper* label, uint8_t* idx, const char** choices) {
    FieldDef f{};
    f.type = FIELD_CHOICE; f.label = label; f.valuePtr = idx; f.choices = choices;
    return f;
}
FieldDef APALCDGUI::fieldBool(const __FlashStringHelper* label, bool* val) {
    FieldDef f{};
    f.type = FIELD_BOOL; f.label = label; f.valuePtr = val;
    return f;
}
FieldDef APALCDGUI::fieldAction(const __FlashStringHelper* label, void (*action)(), bool confirm) {
    FieldDef f{};
    f.type = FIELD_ACTION; f.label = label; f.action = action; f.confirm = confirm;
    return f;
}
FieldDef APALCDGUI::fieldReadonly(const __FlashStringHelper* label, const __FlashStringHelper* unit,
                                   float* val, uint8_t decimals) {
    FieldDef f{};
    f.type = FIELD_READONLY; f.label = label; f.unit = unit; f.valuePtr = val; f.decimals = decimals;
    return f;
}

// ---- Button polling ---------------------------------------------------------
void APALCDGUI::_pollBtns() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 2; i++) {
        _enc[i].pressed  = false;
        _enc[i].released = false;
        bool low = (digitalRead(_enc[i].btn) == LOW);
        if (low && !_enc[i].btnHeld) {
            if (now - _enc[i].btnMs >= APALCDGUI_BTN_DEBOUNCE_MS) {
                _enc[i].btnHeld  = 1;
                _enc[i].btnMs    = now;
                _enc[i].longFired = false;
                _enc[i].pressed  = true;
                _touchInput();
            }
        } else if (!low && _enc[i].btnHeld) {
            _enc[i].btnHeld  = 0;
            _enc[i].btnMs    = now;
            _enc[i].released = true;
        }
    }
}

// ---- Both-press detection --------------------------------------------------
void APALCDGUI::_checkBothPress() {
    bool b0 = (_enc[0].btnHeld != 0);
    bool b1 = (_enc[1].btnHeld != 0);
    uint32_t now = millis();
    if (b0 && b1) {
        if (!_bothArmed) {
            _bothArmed = true;
            _bothMs    = now;
        } else if (now - _bothMs >= APALCDGUI_BOTH_PRESS_MS) {
            _bothArmed = false;
            _touchInput();
            if (_bothCb) {
                _bothCb();
            }
#ifdef __DS3231_h
            else if (_rtcPtr && _state != ST_RTC_NAV && _state != ST_RTC_EDIT) {
                DS3231* rtc = (DS3231*)_rtcPtr;
                _rtcVal[0][0] = rtc->getHour(false, false);
                _rtcVal[0][1] = rtc->getMinute();
                _rtcVal[0][2] = rtc->getSecond();
                _rtcVal[1][0] = rtc->getDate();
                _rtcVal[1][1] = rtc->getMonth(false);
                _rtcVal[1][2] = rtc->getYear() + 2000;
                _rtcSub = 0; _rtcCur = 0;
                _setState(ST_RTC_NAV);
            }
#endif
            else if (_state == ST_RTC_NAV || _state == ST_RTC_EDIT) {
                _setState(ST_HOME);
            }
        }
    } else {
        _bothArmed = false;
    }
}

// ---- Long-press detection --------------------------------------------------
void APALCDGUI::_checkLongPress() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 2; i++) {
        if (_enc[i].btnHeld && !_enc[i].longFired) {
            if (now - _enc[i].btnMs >= APALCDGUI_LONG_PRESS_MS) {
                _enc[i].longFired = true;
                _touchInput();
                if (i == 0 && (_state == ST_RTC_NAV || _state == ST_RTC_EDIT)) {
                    _setState(ST_HOME);
                    return;
                }
                if (_longCb[i]) {
                    _longCb[i]();
                } else if (i == 1 && _passActive) {
                    // built-in: show passive alert text as message overlay, then clear
                    showMessage(_passL1, _passL2[0] ? _passL2 : nullptr, 3000);
                    clearAlert();
                }
            }
        }
    }
}

// ---- Menu timeout ----------------------------------------------------------
void APALCDGUI::_checkMenuTimeout() {
    if (_menuSec == 0) return;
    if (_state == ST_HOME || _state == ST_RTC_NAV || _state == ST_RTC_EDIT) return;
    uint32_t totalMs = (uint32_t)_menuSec * 1000UL;
    uint32_t elapsed = millis() - _inputMs;
    if (elapsed >= totalMs) {
        _timeoutWarnSec = 0xFF;
        _scrPos = 0;
        _setState(ST_HOME);
        return;
    }
    // Last 10 s: force a redraw each second so the countdown on row 2 updates
    uint32_t remaining = totalMs - elapsed;
    if (remaining <= 10000UL) {
        uint8_t secsLeft = (uint8_t)((remaining + 999UL) / 1000UL);
        if (secsLeft != _timeoutWarnSec) {
            _timeoutWarnSec = secsLeft;
            _dirty = true;
        }
    } else if (_timeoutWarnSec != 0xFF) {
        _timeoutWarnSec = 0xFF;
        _dirty = true;
    }
}

// ---- Render: passive alert indicator — cols 17-19, row 2 ----
void APALCDGUI::_renderPassiveCorner() {
    _lcd.setCursor(17, 2);
    if (!_passActive) { _lcd.print(F("   ")); return; }
    uint32_t now = millis();
    if (_passLevel == ALERT_CRITICAL) {
        if (now - _passFlashMs >= APALCDGUI_ALERT_FLASH_MS) {
            _passFlash   = !_passFlash;
            _passFlashMs = now;
        }
        _lcd.print(_passFlash ? F("[!]") : F("   "));
    } else {
        _lcd.print(_passLevel == ALERT_WARNING ? F("[*]") : F("[i]"));
    }
}

// ---- Render: field row (row 0, 1, or 2) ------------------------------------
void APALCDGUI::_renderField(uint8_t row, const FieldDef& f, bool cursor, bool editing) {
    // Col 0: cursor char
    _lcd.setCursor(0, row);
    if (cursor) _lcd.write(editing ? CC_CURSOR_EDIT : '>');
    else        _lcd.write(' ');

    // Col 1–12: label (12 chars), col 13: separator space
    char lbuf[13]; _fstr(lbuf, f.label, 12);
    _padWrite(1, row, lbuf, 12);
    _lcd.setCursor(13, row); _lcd.print(' ');

    // Col 14–17: value (4 chars, right-aligned)
    char vbuf[8] = "    ";  // 8 bytes: dtostrf/snprintf headroom for any int16_t or float string
    switch (f.type) {
        case FIELD_INT: {
            int16_t v = editing ? (int16_t)_editVal : *(int16_t*)f.valuePtr;
            snprintf(vbuf, sizeof(vbuf), "%4d", v);
            break;
        }
        case FIELD_FLOAT: {
            float v = editing ? _editVal : *(float*)f.valuePtr;
            dtostrf(v, 4, f.decimals, vbuf);
            break;
        }
        case FIELD_CHOICE: {
            uint8_t idx = editing ? (uint8_t)_editVal : *(uint8_t*)f.valuePtr;
            if (f.choices && f.choices[idx]) {
                strncpy(vbuf, f.choices[idx], 4); vbuf[4] = '\0';
            }
            break;
        }
        case FIELD_BOOL: {
            bool v = editing ? (_editVal != 0.0f) : *(bool*)f.valuePtr;
            strncpy(vbuf, v ? " ON " : "OFF ", 4); vbuf[4] = '\0';
            break;
        }
        case FIELD_ACTION:
            strncpy(vbuf, f.action ? "STRT" : "-no-", 4); vbuf[4] = '\0';
            break;
        case FIELD_READONLY: {
            float v = *(float*)f.valuePtr;
            dtostrf(v, 4, f.decimals, vbuf);
            break;
        }
    }
    _padWrite(14, row, vbuf, 4);

    // Col 18–19: unit (2 chars) — omitted on 3-field screens (alert indicator owns cols 17-19)
    char ubuf[3] = "  ";
    if (f.unit) { _fstr(ubuf, f.unit, 2); }
    _padWrite(18, row, ubuf, 2);
}

// ---- Render: BACK/SAVE action bar (row 3) with page indicator ---------------
void APALCDGUI::_renderActionBar() {
    const Screen* s = _curScreen();
    if (!s) return;
    uint8_t backPos = s->fieldCount;
    uint8_t savePos = s->fieldCount + 1;

    _lcd.setCursor(0, 3);
    _lcd.write(_curPos == backPos ? '>' : ' ');
    _lcd.print(F("*BACK  "));
    if (_scrPos != 0) {
        char    side   = (_scrPos > 0) ? 'R' : 'L';
        uint8_t absPos = (_scrPos > 0) ? (uint8_t)_scrPos : (uint8_t)(-_scrPos);
        uint8_t total  = _countSide(_scrPos > 0 ? SCREEN_RIGHT : SCREEN_LEFT);
        bool first = (absPos == 1);
        bool last  = (absPos == total);
        _lcd.print(side);
        _lcd.print(absPos <= 9 ? (char)('0' + absPos) : '+');
        if      (first && !last) _lcd.write((uint8_t)6); // ↓
        else if (last)           _lcd.write((uint8_t)5); // ↑
        else                     _lcd.write((uint8_t)3); // ↕
    } else {
        _lcd.print(F("   "));
    }
    _lcd.print(F("   "));
    _lcd.write(_curPos == savePos ? '>' : ' ');
    _lcd.print(F("*SAVE"));
}

// ---- Render: full menu screen ----------------------------------------------
void APALCDGUI::_renderMenu() {
    const Screen* s = _curScreen();
    if (!s) return;

    // Field rows 0..fieldCount-1
    for (uint8_t i = 0; i < s->fieldCount; i++) {
        _renderField(i, s->field[i], _curPos == i, _state == ST_EDIT && _editIdx == i);
    }

    // On 1-field screens row 1 needs clearing
    if (s->fieldCount == 1) {
        char blank[COLS + 1]; memset(blank, ' ', COLS); blank[COLS] = '\0';
        _rowWrite(1, blank);
    }

    // Row 2: only available when fieldCount < 3 (otherwise the third field owns it)
    if (s->fieldCount < 3) {
        char blank[COLS + 1]; memset(blank, ' ', COLS); blank[COLS] = '\0';
        _rowWrite(2, blank);
        if (_timeoutWarnSec != 0xFF) {
            // Countdown overrides everything else on row 2
            char buf[COLS + 1];
            snprintf(buf, sizeof(buf), "  HOME in %us...  ", _timeoutWarnSec);
            _rowWrite(2, buf);
        } else if (_row2Cb) {
            _row2Cb();
        } else if (s->title) {
            char tbuf[14]; _fstr(tbuf, s->title, 13);
            _padWrite(0, 2, tbuf, 17); // cols 0-16; alert indicator owns 17-19
        }
    }

    // Row 3: BACK / page indicator / SAVE
    _renderActionBar();
    // Alert indicator — written last so it always wins over any row 2 content
    _renderPassiveCorner();
}

// ---- Render: home screen ---------------------------------------------------
void APALCDGUI::_renderHome() {
    if (hasActiveAlert()) {
        _renderAlertScreen();
        return;
    }
    if (_homeCb) {
        _homeCb(_lcd);
    } else {
        for (uint8_t r = 0; r < ROWS; r++) {
            char blank[COLS + 1]; memset(blank, ' ', COLS); blank[COLS] = '\0';
            _rowWrite(r, blank);
        }
    }
    _renderPassiveCorner();
}

// ---- Render: active alert screen -------------------------------------------
void APALCDGUI::_renderAlertScreen() {
    uint8_t count = _alertCount();
    const AlertSlot& a = _alertQ[_alertHead];

    // Row 0: header
    if (count > 1) {
        char hdr[COLS + 4];   // +4: "!! ALARM  [1 of N] !" is 21 chars + NUL; +4 gives headroom
        snprintf(hdr, sizeof(hdr), "!! ALARM  [1 of %d] !", count);
        _rowWrite(0, hdr);
    } else {
        _rowWrite(0, "!!!!!  ALARM  !!!!!");
    }

    // Row 1 & 2: message lines
    char r1[COLS + 1], r2[COLS + 1];
    strncpy(r1, a.l1, COLS); r1[COLS] = '\0';
    strncpy(r2, a.l2, COLS); r2[COLS] = '\0';
    _rowWrite(1, r1); _rowWrite(2, r2);

    // Row 3: ACK instruction — consistent regardless of queue depth
    _rowWrite(3, "         KB2:ACK !");
}

// ---- State: HOME -----------------------------------------------------------
void APALCDGUI::_stateHome() {
    if (_dirty) { _renderHome(); _dirty = false; }

    int32_t k1 = _encClicks(0);
    if (k1 != 0) {
        _touchInput();
        int8_t newPos = _scrPos + ((k1 > 0) ? 1 : -1);
        if (newPos > 0 && (uint8_t)newPos > _countSide(SCREEN_RIGHT)) newPos = _scrPos;
        if (newPos < 0 && (uint8_t)(-newPos) > _countSide(SCREEN_LEFT))  newPos = _scrPos;
        if (newPos != 0 && newPos != _scrPos) {
            _scrPos = newPos;
            _curPos = 0;
            _setState(ST_NAV);
            return;
        }
        _dirty = true;
    }

    // KB2 press on active alert: ACK current slot, fire callback, advance to next
    if (hasActiveAlert() && _enc[1].pressed) {
        _touchInput();
        AlertSlot& cur = _alertQ[_alertHead];
        void (*cb)() = cur.ackCb;
        cur.used = false;
        // Advance head to next used slot (if any)
        for (uint8_t i = 1; i <= APA_LCD_ACTIVE_ALERT_QUEUE; i++) {
            uint8_t next = (_alertHead + i) % APA_LCD_ACTIVE_ALERT_QUEUE;
            if (_alertQ[next].used) { _alertHead = next; break; }
        }
        _dirty = true;
        if (cb) cb(); // fire after state update so callback can call showMessage
    }

    if (_passActive) _renderPassiveCorner();
}

// ---- State: NAV ------------------------------------------------------------
void APALCDGUI::_stateNav() {
    if (_dirty) { _renderMenu(); _dirty = false; }

    int32_t k1 = _encClicks(0);
    if (k1 != 0) {
        _touchInput();
        int8_t step   = (k1 > 0) ? 1 : -1;
        int8_t newPos = _scrPos + step;
        if (newPos == 0) { _setState(ST_HOME); _scrPos = 0; return; }
        bool ok = (newPos > 0) ? ((uint8_t)newPos  <= _countSide(SCREEN_RIGHT))
                               : ((uint8_t)(-newPos) <= _countSide(SCREEN_LEFT));
        if (ok) { _scrPos = newPos; _curPos = 0; }
        _dirty = true;
        return;
    }

    int32_t k2 = _encClicks(1);
    if (k2 != 0) {
        _touchInput();
        _curPos = _nextCurPos(_curPos, k2 > 0 ? 1 : -1);
        _dirty = true;
    }

    if (_enc[1].pressed) {
        _touchInput();
        const Screen* s = _curScreen();
        if (!s) return;
        if (_curPos == s->fieldCount) { // BACK
            _setState(ST_FLASH_BACK);
        } else if (_curPos == s->fieldCount + 1) { // SAVE
            _setState(ST_FLASH_SAVE);
        } else { // field
            const FieldDef& f = s->field[_curPos];
            if (f.type == FIELD_READONLY) return;
            if (f.type == FIELD_ACTION) {
                if (f.action) {
                    if (f.confirm) {
                        _pendingAction = f.action;
                        _pendingLabel  = f.label;
                        _setState(ST_CONFIRM);
                    } else {
                        f.action();
                    }
                }
                return;
            }
            // Enter EDIT — load current value into working copy
            _editIdx = _curPos;
            switch (f.type) {
                case FIELD_INT:    _editVal = (float)*(int16_t*)f.valuePtr; break;
                case FIELD_FLOAT:  _editVal = *(float*)f.valuePtr;          break;
                case FIELD_CHOICE: _editVal = (float)*(uint8_t*)f.valuePtr; break;
                case FIELD_BOOL:   _editVal = *(bool*)f.valuePtr ? 1.0f : 0.0f; break;
                default: break;
            }
            _setState(ST_EDIT);
        }
    }

    if (_passActive) _renderPassiveCorner();
}

// ---- State: EDIT -----------------------------------------------------------
void APALCDGUI::_stateEdit() {
    if (_dirty) { _renderMenu(); _dirty = false; }

    const Screen* s = _curScreen();
    if (!s) { _setState(ST_NAV); return; }
    const FieldDef& f = s->field[_editIdx];

    int32_t k2 = _encClicks(1);
    if (k2 != 0) {
        _touchInput();
        if (f.type == FIELD_BOOL) {
            _editVal = (_editVal == 0.0f) ? 1.0f : 0.0f; // any rotation toggles
        } else if (f.type == FIELD_CHOICE) {
            uint8_t nChoices = 0;
            while (f.choices && f.choices[nChoices]) nChoices++;
            if (nChoices) {
                int32_t idx = (int32_t)_editVal + k2;
                if (idx < 0) idx = nChoices - 1;
                if ((uint8_t)idx >= nChoices) idx = 0;
                _editVal = (float)idx;
            }
        } else {
            _editVal += (float)k2 * f.step;
            if (_editVal < f.minVal) _editVal = f.minVal;
            if (_editVal > f.maxVal) _editVal = f.maxVal;
        }
        _dirty = true;
    }

    if (_enc[1].pressed) {
        _touchInput();
        // Commit value
        switch (f.type) {
            case FIELD_INT:    *(int16_t*)f.valuePtr = (int16_t)_editVal; break;
            case FIELD_FLOAT:  *(float*)f.valuePtr   = _editVal;          break;
            case FIELD_CHOICE: *(uint8_t*)f.valuePtr = (uint8_t)_editVal; break;
            case FIELD_BOOL:   *(bool*)f.valuePtr    = (_editVal != 0.0f); break;
            default: break;
        }
        // Teleport cursor to SAVE — _setState flushes enc pos, so set _curPos directly
        _curPos = s->fieldCount + 1;
        _setState(ST_NAV);
    }

    if (_passActive) _renderPassiveCorner();
}

// ---- State: FLASH_SAVE -----------------------------------------------------
void APALCDGUI::_stateFlashSave() {
    if (_dirty) {
        _lcd.setCursor(14, 3);
        _lcd.write(CC_CURSOR_EDIT); // ►
        _lcd.print(F("*SAVE"));
        _dirty = false;
    }
    if (millis() - _stateMs >= APALCDGUI_FLASH_SAVE_MS) {
        const Screen* s = _curScreen();
        if (s && s->onSave) s->onSave();
        _curPos = s ? s->fieldCount : 2; // land cursor on BACK after save
        _setState(ST_NAV);
        showMessage("Settings saved!", nullptr, 1000);
    }
}

// ---- State: FLASH_BACK -----------------------------------------------------
void APALCDGUI::_stateFlashBack() {
    if (_dirty) {
        _lcd.setCursor(0, 3);
        _lcd.write(CC_CURSOR_EDIT); // ► — same confirmation symbol as field EDIT
        _lcd.print(F("*BACK"));
        _dirty = false;
    }
    if (millis() - _stateMs >= APALCDGUI_FLASH_BACK_MS) {
        _scrPos = 0;
        _setState(ST_HOME);
    }
}

// ---- State: BRIGHTNESS_ADJUST ----------------------------------------------
void APALCDGUI::_stateBrightness() {
    char buf[COLS + 1];
    if (_dirty) {
        char blank[COLS + 1]; memset(blank, ' ', COLS); blank[COLS] = '\0';
        _rowWrite(0, blank);
        _rowWrite(1, "  Brightness adj    ");
        snprintf(buf, sizeof(buf), "  Brightness: %3d  ", _bright);
        _rowWrite(2, buf);
        _rowWrite(3, "KB1+K1:brightness   ");
        _dirty = false;
    }
    int32_t k1 = _encClicks(0);
    if (k1 != 0) {
        int16_t b = (int16_t)_bright + (int16_t)k1 * APALCDGUI_BRIGHTNESS_STEP;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        _bright = (uint8_t)b;
        _blSet(_bright);
        _saveEEPROM();
        snprintf(buf, sizeof(buf), "  Brightness: %3d  ", _bright);
        _rowWrite(2, buf);
    }
    if (_enc[0].released) {
        _setState(_scrPos == 0 ? ST_HOME : ST_NAV);
    }
}

// ---- State: CONFIRM --------------------------------------------------------
void APALCDGUI::_stateConfirm() {
    if (_dirty) {
        char lbuf[14]; _fstr(lbuf, _pendingLabel, 13);
        _rowWrite(0, "  Confirm action?   ");
        _rowWrite(1, lbuf);
        _rowWrite(2, "");
        _rowWrite(3, ">*NO           *YES ");
        _dirty = false;
    }
    if (_enc[1].pressed) { // KB2 = YES — execute action
        _touchInput();
        void (*fn)() = _pendingAction;
        _setState(ST_NAV);
        if (fn) fn();
    }
    if (_enc[0].pressed) { // KB1 = NO — cancel
        _touchInput();
        _setState(ST_NAV);
    }
}

// ---- State: RTC_NAV --------------------------------------------------------
void APALCDGUI::_stateRTCNav() {
#ifndef __DS3231_h
    _setState(ST_HOME); return;
#else
    if (_dirty) {
        const char* labels[2][3] = {{"  HH","  MM","  SS"},{"  DD","  MM","YYYY"}};
        char r0[COLS+1]; memset(r0,' ',COLS); r0[COLS]='\0';
        for(uint8_t f=0;f<3;f++) { uint8_t c=f*6+2; for(uint8_t k=0;labels[_rtcSub][f][k];k++) r0[c+k]=labels[_rtcSub][f][k]; }
        _rowWrite(0, r0);
        char r1[COLS+1]; memset(r1,' ',COLS); r1[COLS]='\0';
        char tmp[5];
        for(uint8_t f=0;f<3;f++){
            uint8_t c=f*6+2;
            if(_rtcSub==1&&f==2) snprintf(tmp,5,"%04d",_rtcVal[_rtcSub][f]);
            else snprintf(tmp,5,"%2d",_rtcVal[_rtcSub][f]);
            for(uint8_t k=0;tmp[k]&&c+k<COLS;k++) r1[c+k]=tmp[k];
        }
        _rowWrite(1, r1);
        char r2[COLS+1]; memset(r2,' ',COLS); r2[COLS]='\0';
        for(uint8_t f=0;f<3;f++){
            uint8_t c=f*6+2;
            r2[c+1]=(uint8_t)(_rtcCur==f ? 6 : 5);
        }
        _rowWrite(2, r2);
        _lcd.setCursor(0,3);
        _lcd.write(_rtcCur==3 ? '>' : ' ');
        _lcd.print(F("*BACK        "));
        _lcd.write(_rtcCur==4 ? '>' : ' ');
        _lcd.print(F("*SAVE"));
        _dirty = false;
    }

    int32_t k1 = _encClicks(0);
    if (k1 != 0) { _rtcSub = (_rtcSub + 1) & 1; _dirty = true; }

    int32_t k2 = _encClicks(1);
    if (k2 != 0) {
        int16_t c = (int16_t)_rtcCur + (int16_t)k2;
        if (c < 0) c = 4; if (c > 4) c = 0;
        _rtcCur = (uint8_t)c;
        _dirty = true;
    }

    if (_enc[1].pressed) {
        if (_rtcCur < 3) {
            _setState(ST_RTC_EDIT);
        } else if (_rtcCur == 3) {
            _setState(ST_HOME);
        } else {
            DS3231* rtc = (DS3231*)_rtcPtr;
            rtc->setSecond(_rtcVal[0][2]);
            rtc->setMinute(_rtcVal[0][1]);
            rtc->setHour(_rtcVal[0][0]);
            rtc->setDate(_rtcVal[1][0]);
            rtc->setMonth(_rtcVal[1][1]);
            rtc->setYear(_rtcVal[1][2] - 2000);
            _setState(ST_HOME);
        }
    }
#endif
}

// ---- State: RTC_EDIT -------------------------------------------------------
void APALCDGUI::_stateRTCEdit() {
#ifndef __DS3231_h
    _setState(ST_HOME); return;
#else
    int32_t k2 = _encClicks(1);
    if (k2 != 0) {
        int16_t& v = _rtcVal[_rtcSub][_rtcCur];
        v += (int16_t)k2;
        if (_rtcSub == 0) {
            if (_rtcCur == 0) { if(v<0)v=23; if(v>23)v=0; }
            if (_rtcCur == 1) { if(v<0)v=59; if(v>59)v=0; }
            if (_rtcCur == 2) { if(v<0)v=59; if(v>59)v=0; }
        } else {
            if (_rtcCur == 0) { if(v<1)v=31; if(v>31)v=1; }
            if (_rtcCur == 1) { if(v<1)v=12; if(v>12)v=1; }
            if (_rtcCur == 2) { if(v<2000)v=2099; if(v>2099)v=2000; }
        }
        _dirty = true;
        char tmp[5];
        uint8_t col = _rtcCur * 6 + 2;
        if (_rtcSub==1 && _rtcCur==2) snprintf(tmp,5,"%04d",v);
        else snprintf(tmp,5,"%2d",v);
        _padWrite(col, 1, tmp, (_rtcSub==1&&_rtcCur==2) ? 4 : 2);
    }

    if (_enc[1].pressed) {
        _setState(ST_RTC_NAV);
    }
#endif
}

// ---- update() ---------------------------------------------------------------
void APALCDGUI::update() {
    if (!_ready) return;

    _pollBtns();
    _checkBothPress();
    _checkLongPress();
    _blUpdate();
    _checkMenuTimeout();

    // Message overlay takes priority
    if (_msgActive) {
        if (millis() >= _msgUntilMs) {
            _msgActive = false;
            _dirty = true;
        } else {
            if (_dirty) {
                _rowWrite(0, _msgL1); _rowWrite(1, _msgL2);
                char blank[COLS+1]; memset(blank,' ',COLS); blank[COLS]='\0';
                _rowWrite(2, blank); _rowWrite(3, blank);
                _dirty = false;
            }
            return;
        }
    }

    // Knob1 rotation while KB1 held → brightness adjust.
    // Do NOT use _setState() here — it would flush enc[0].pos and lose the click.
    {
        int32_t k1raw; noInterrupts(); k1raw = _enc[0].pos; interrupts();
        if (k1raw != 0 && _enc[0].btnHeld) {
            if (_state != ST_BRIGHTNESS) {
                _state   = ST_BRIGHTNESS;
                _stateMs = millis();
                _dirty   = true;
            }
        }
    }

    switch (_state) {
        case ST_HOME:        _stateHome();        break;
        case ST_NAV:         _stateNav();         break;
        case ST_EDIT:        _stateEdit();        break;
        case ST_FLASH_SAVE:  _stateFlashSave();   break;
        case ST_FLASH_BACK:  _stateFlashBack();   break;
        case ST_BRIGHTNESS:  _stateBrightness();  break;
        case ST_CONFIRM:     _stateConfirm();     break;
        case ST_RTC_NAV:     _stateRTCNav();      break;
        case ST_RTC_EDIT:    _stateRTCEdit();     break;
    }
}
