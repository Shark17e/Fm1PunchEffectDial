#include <M5Dial.h>
#include <MIDIHandler.h>
#include <USBConnection.h>
#include <Preferences.h>

USBConnection usbHost;
static Preferences prefs;

static uint8_t cfgFxCh  = 16;
static uint8_t cfgRemCh = 5;
static uint8_t tgtCh[5] = {5, 1, 2, 3, 10};
static const uint8_t TGT_DEF[5] = {5, 1, 2, 3, 10};
static int8_t  cfgSel = 0;
static bool    cfgTouched = false;

static const uint8_t FX_COUNT   = 27;

static const uint8_t FX_NOTE[FX_COUNT] = {
    53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79,
    54, 56, 58, 61, 63, 66, 68, 70, 73, 75, 78
};

static const char* const FX_NAME[FX_COUNT] = {
    "LOOP 4", "LOOP 8", "LOOP 16", "LOOP 32", "STUTTER", "REVERSE", "STOP", "HALF",
    "LOW", "HIGH", "PHONE", "CRUSH", "ALIAS", "GATE", "ECHO", "WOBBLE",
    "KILL LOW", "KILL MID", "KILL HIGH", "REWIND", "OCT UP", "FLANGER", "AUTO-PAN",
    "FREEZE", "LOOP 8T", "MONO", "FADE OUT"
};

static const unsigned long CLICK_MAX = 250;
static const unsigned long GAP_MAX   = 300;

static const long   ENC_COUNTS_PER_DETENT = 1;
static const int8_t ENC_DIR               = +1;

static const float CX = 120.0f;
static const float CY = 120.0f;
static const float D2R = 0.0174532925f;

static const unsigned long PAD_GATE_MS = 20;

static const uint16_t C_RED      = 0xF800;
static const uint16_t C_RED_DIM  = 0x8800;
static const uint16_t C_RED_FILL = 0x4000;
static const uint16_t C_GRID     = 0x4208;

enum { PG_FX = 0, PG_TR, PG_1, PG_2, PG_3, PG_4, PG_5, PG_6, PG_CFG, PG_COUNT };
static uint8_t page = PG_FX;

struct ParamDef { uint8_t cc; const char* name; };
static const ParamDef PDEF[11] = {
    {74, "CUT"}, {71, "RES"}, {7, "LEVEL"}, {10, "PAN"}, {91, "REV"}, {93, "CHOR"},
    {94, "DLY"}, {73, "ATK"}, {75, "DEC"}, {72, "REL"}, {5, "GLIDE"}
};
static const bool    PMOM[11]      = {false, true, false, false, true, true, true, false, false, false, true};
static const uint8_t PDEF_INIT[11] = {64, 0, 127, 64, 0, 0, 0, 64, 64, 64, 0};
static const uint8_t     PAGE_X[6]    = {0, 4, 5, 2, 7, 9};
static uint8_t           pageY[6]     = {1, 6, 10, 3, 8, 10};
static const char* const PAGE_NAME[6] = {"FILTER", "SPACE", "MOTION", "MIX", "ENV", "RELEASE"};

static const char* const TGT_NAME[5]  = {"SEL", "T1", "T2", "T3", "DR"};
static uint8_t targetIdx = 0;
static uint8_t ccVal[5][11];

struct TileDef { const char* name; uint8_t cc1, cc2; };
static const TileDef TILES[6] = {
    {"PLAY",  112, 0},
    {"REC",   113, 0},
    {"OCT+",  115, 0},
    {"THR-E", 102, 115},
    {"THR-R", 102, 114},
    {"OCT-",  114, 0},
};
static const float    SEG_R      = 112.0f;
static const int16_t  SEG_Y0[3]  = {24, 88, 160};
static const int16_t  SEG_Y1[3]  = {80, 152, 216};
static const int16_t  LBL_X[6]   = {187, 198, 187, 54, 42, 54};
static const int16_t  LBL_Y[6]   = {52, 120, 188, 52, 120, 188};

static int8_t        curIdx       = 0;
static int8_t        latchIdx     = -1;
static int8_t        sentNote     = -1;
static unsigned long pressStart   = 0;
static unsigned long lastRelease  = 0;
static bool          lastWasShort = false;
static bool          pendDbl      = false;
static bool          pressNoop    = false;
static bool          touchActive  = false;
static int           sendErrors   = 0;
static bool          wasConnected = false;

static long encPos   = 0;
static long encAccum = 0;

static bool           padActive   = false;
static bool           chipArmed   = false;
static unsigned long  lastPadSend = 0;
static float          padX        = 120.0f;
static float          padY        = 120.0f;
static uint8_t        padSnapX    = 0;
static uint8_t        padSnapY    = 0;
static int8_t         activeTile  = -1;
static bool           panicHeld   = false;
static bool           menuTurn    = false;
static bool           padHold     = false;
static unsigned long  padPressMs  = 0;
static unsigned long  padLastRel  = 0;
static bool           padLastShort = false;
static bool           padPendDbl  = false;

static const int OUTBOX_MAX = 32;
struct OutMsg { uint8_t note; bool on; };
static OutMsg outbox[OUTBOX_MAX];
static int    outHead  = 0;
static int    outCount = 0;

static const int CCOUT_MAX = 64;
struct CCMsg { uint8_t ch, cc, val; };
static CCMsg ccout[CCOUT_MAX];
static int   ccHead  = 0;
static int   ccCount = 0;

static void outPush(uint8_t note, bool on) {
    if (outCount >= OUTBOX_MAX) { sendErrors++; return; }
    OutMsg& m = outbox[(outHead + outCount) % OUTBOX_MAX];
    m.note = note;
    m.on = on;
    outCount++;
}

static void ccPush(uint8_t ch, uint8_t cc, uint8_t val) {
    if (ccCount >= CCOUT_MAX) { sendErrors++; return; }
    CCMsg& m = ccout[(ccHead + ccCount) % CCOUT_MAX];
    m.ch = ch;
    m.cc = cc;
    m.val = val;
    ccCount++;
}

static void outClear() {
    outHead = 0;
    outCount = 0;
}

static void ccClear() {
    ccHead = 0;
    ccCount = 0;
}

static uint8_t* cfgVal(int i) {
    switch (i) {
    case 0:  return &cfgFxCh;
    case 1:  return &cfgRemCh;
    case 2:  return &tgtCh[0];
    case 3:  return &tgtCh[1];
    case 4:  return &tgtCh[2];
    case 5:  return &tgtCh[3];
    default: return &tgtCh[4];
    }
}

static void cfgSave() {
    prefs.begin("fm1dial", false);
    prefs.putUChar("fxch", cfgFxCh);
    prefs.putUChar("remch", cfgRemCh);
    prefs.putBytes("tgtch", tgtCh, 5);
    prefs.end();
}

static void cfgLoad() {
    prefs.begin("fm1dial", true);
    uint8_t v;
    v = prefs.getUChar("fxch", 16);
    if (v >= 1 && v <= 16) cfgFxCh = v;
    v = prefs.getUChar("remch", 5);
    if (v >= 1 && v <= 16) cfgRemCh = v;
    if (prefs.getBytes("tgtch", tgtCh, 5) == 5)
        for (int i = 0; i < 5; i++)
            if (tgtCh[i] < 1 || tgtCh[i] > 16) tgtCh[i] = TGT_DEF[i];
    prefs.end();
}

static void outFlush() {
    while (outCount > 0) {
        OutMsg& m = outbox[outHead];
        bool ok = m.on ? midiHandler.sendNoteOn(cfgFxCh, m.note, 100, &usbHost)
                       : midiHandler.sendNoteOff(cfgFxCh, m.note, 0, &usbHost);
        if (!ok) break;
        outHead = (outHead + 1) % OUTBOX_MAX;
        outCount--;
    }
    while (ccCount > 0) {
        CCMsg& m = ccout[ccHead];
        bool ok = midiHandler.sendControlChange(m.ch, m.cc, m.val, &usbHost);
        if (!ok) break;
        ccHead = (ccHead + 1) % CCOUT_MAX;
        ccCount--;
    }
}

static void noteOn(uint8_t idx) {
    outPush(FX_NOTE[idx], true);
    sentNote = (int8_t)idx;
}

static void noteOff(int8_t idx) {
    outPush(FX_NOTE[idx], false);
    sentNote = -1;
}

static void panicAll() {
    for (uint8_t i = 0; i < FX_COUNT; i++)
        outPush(FX_NOTE[i], false);
    sentNote = -1;
}

static void tick()        { M5Dial.Speaker.tone(6000, 8); }
static void beepLatch()   { M5Dial.Speaker.tone(1500, 60); }
static void beepUnlatch() { M5Dial.Speaker.tone(700, 60); }

static uint8_t valOf(uint8_t pi) { return ccVal[targetIdx][pi]; }

static void setVal(uint8_t pi, uint8_t v) {
    ccVal[targetIdx][pi] = v;
    ccPush(tgtCh[targetIdx], PDEF[pi].cc, v);
}

static uint8_t xToVal(float x) {
    int v = (int)((x - 32.0f) * 127.0f / 176.0f + 0.5f);
    return (uint8_t)constrain(v, 0, 127);
}

static uint8_t yToVal(float y) {
    int v = (int)((208.0f - y) * 127.0f / 176.0f + 0.5f);
    return (uint8_t)constrain(v, 0, 127);
}

static void drawFxPage() {
    auto& d = M5Dial.Display;
    d.fillScreen(TFT_BLACK);
    d.drawCircle(120, 120, 118, C_RED_DIM);
    d.drawCircle(120, 120, 97, C_GRID);

    for (int i = 0; i < FX_COUNT; i++) {
        float c = -90.0f + i * (360.0f / FX_COUNT);
        float h = (360.0f / FX_COUNT) / 2.0f;
        bool sel = (i == curIdx);
        d.fillArc(120, 120, sel ? 117 : 116, sel ? 101 : 104, c - h + 1.6f, c + h - 1.6f,
                  sel ? C_RED : C_RED_FILL);
    }

    d.setTextDatum(middle_center);

    d.setFont(&fonts::Font2);
    d.setTextColor(usbHost.isConnected() ? TFT_WHITE : C_RED, TFT_BLACK);
    d.drawString(usbHost.isConnected() ? "USB OK" : "USB --", CX, 30);

    d.setFont(&fonts::Orbitron_Light_32);
    d.setTextColor(C_RED, TFT_BLACK);
    d.drawString(String(curIdx + 1) + "/" + String(FX_COUNT), CX, 68);

    d.setFont(&fonts::Orbitron_Light_24);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(FX_NAME[curIdx], CX, 100);

    d.drawLine(72, 118, 168, 118, C_GRID);

    d.setFont(&fonts::Font2);
    d.setTextColor(latchIdx >= 0 ? C_RED : C_GRID, TFT_BLACK);
    d.drawString(latchIdx >= 0 ? String("HOLD ") + FX_NAME[latchIdx] : String("HOLD -"), CX, 136);
    d.setTextColor(sentNote >= 0 ? C_RED : C_GRID, TFT_BLACK);
    d.drawString(sentNote >= 0 ? "FX ON" : "FX off", CX, 158);

    if (sendErrors > 0) {
        d.setFont(&fonts::Font0);
        d.setTextColor(C_RED, TFT_BLACK);
        d.drawString("err " + String(sendErrors), CX, 182);
    }
}

static void drawParamPage() {
    auto& d = M5Dial.Display;
    d.fillScreen(TFT_BLACK);

    d.drawCircle(120, 120, 118, C_RED_DIM);
    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f * D2R;
        float ca = cosf(a), sa = sinf(a);
        d.drawLine((int)lroundf(120.0f + 111.0f * ca), (int)lroundf(120.0f + 111.0f * sa),
                   (int)lroundf(120.0f + 117.0f * ca), (int)lroundf(120.0f + 117.0f * sa), C_GRID);
    }
    d.drawCircle(120, 120, 95, C_GRID);

    uint8_t p  = page - PG_1;
    uint8_t xi = PAGE_X[p];
    uint8_t yi = pageY[p];
    uint8_t vx = ccVal[targetIdx][xi];
    uint8_t vy = ccVal[targetIdx][yi];
    float px = 32.0f + vx * 176.0f / 127.0f;
    float py = 208.0f - vy * 176.0f / 127.0f;

    d.drawLine(12, (int)py, 228, (int)py, C_RED_DIM);
    d.drawLine((int)px, 12, (int)px, 228, C_RED_DIM);

    d.setTextDatum(middle_center);
    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.drawString(PDEF[xi].name, 56, 96);
    d.drawString(PDEF[yi].name, 184, 96);

    d.setFont(&fonts::Orbitron_Light_24);
    d.setTextColor(C_RED, TFT_BLACK);
    d.drawString(String(vx), 56, 120);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(String(vy), 184, 120);

    d.setFont(&fonts::Font2);
    d.setTextColor(C_RED_DIM, TFT_BLACK);
    d.drawString(PAGE_NAME[p], 120, 208);
    if (padHold) {
        d.setTextColor(C_RED, TFT_BLACK);
        d.drawString("HOLD", 120, 228);
    }

    if (chipArmed) d.fillRoundRect(86, 26, 68, 22, 8, C_RED);
    d.drawRoundRect(86, 26, 68, 22, 8, chipArmed ? C_RED : C_RED_DIM);
    d.setTextColor(chipArmed ? TFT_BLACK : C_RED, TFT_BLACK);
    d.drawString(TGT_NAME[targetIdx], 120, 37);

    d.setFont(&fonts::Font0);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    char cb[10];
    snprintf(cb, sizeof cb, "CH %u", tgtCh[targetIdx]);
    d.drawString(cb, 120, 58);

    d.fillCircle((int)px, (int)py, 5, C_RED);
    d.drawCircle((int)px, (int)py, 9, C_RED_DIM);
}

static void segFill(bool right, int band, uint16_t color) {
    auto& d = M5Dial.Display;
    int xin = right ? 164 : 76;
    for (int y = SEG_Y0[band]; y <= SEG_Y1[band]; y++) {
        float dy = (float)y - 120.0f;
        float w = sqrtf(SEG_R * SEG_R - dy * dy);
        if (right) {
            int xo = (int)(120.0f + w);
            d.drawFastHLine(xin, y, xo - xin + 1, color);
        } else {
            int xo = (int)(120.0f - w);
            d.drawFastHLine(xo, y, xin - xo + 1, color);
        }
    }
}

static void drawTransport() {
    auto& d = M5Dial.Display;
    d.fillScreen(TFT_BLACK);
    d.drawCircle(120, 120, 118, C_RED_DIM);
    d.drawCircle(120, 120, 95, C_GRID);
    d.setTextDatum(middle_center);
    d.setFont(&fonts::Font2);
    for (int i = 0; i < 6; i++) {
        bool act = (i == activeTile);
        bool right = (i < 3);
        int band = right ? i : i - 3;
        uint16_t fill = act ? C_RED : C_RED_FILL;
        segFill(right, band, fill);
        d.setTextColor(act ? TFT_BLACK : C_RED, fill);
        d.drawString(TILES[i].name, LBL_X[i], LBL_Y[i]);
    }
    d.setTextColor(C_RED_DIM, TFT_BLACK);
    d.drawString("TRANSPORT", 120, 208);
}

static const char* const CFG_NAME[7] = {"FX", "REMOTE", "SEL TRACK", "TRACK 1", "TRACK 2", "TRACK 3", "DRUMS"};

static void drawCfgPage() {
    auto& d = M5Dial.Display;
    d.fillScreen(TFT_BLACK);
    d.drawCircle(120, 120, 118, C_RED_DIM);
    d.setTextDatum(middle_center);
    for (int i = 0; i < 7; i++) {
        int yc = 58 + i * 20;
        bool sel = (i == cfgSel);
        if (sel) {
            d.fillRoundRect(30, yc - 10, 180, 20, 8, C_RED_FILL);
            d.fillCircle(40, yc, 3, C_RED);
        }
        d.setFont(&fonts::Font2);
        d.setTextColor(sel ? TFT_WHITE : TFT_DARKGREY, sel ? C_RED_FILL : TFT_BLACK);
        d.drawString(CFG_NAME[i], 92, yc);
        char vb[8];
        snprintf(vb, sizeof vb, "CH %u", *cfgVal(i));
        d.setTextColor(sel ? C_RED : TFT_WHITE, sel ? C_RED_FILL : TFT_BLACK);
        d.drawString(vb, 184, yc);
    }
    d.setTextColor(C_RED_DIM, TFT_BLACK);
    d.drawString("CONFIG", 120, 208);
}

static void redraw() {
    if (page == PG_FX)
        drawFxPage();
    else if (page == PG_TR)
        drawTransport();
    else if (page == PG_CFG)
        drawCfgPage();
    else
        drawParamPage();
}

static void applyPad(bool force) {
    uint8_t p  = page - PG_1;
    uint8_t xi = PAGE_X[p];
    uint8_t yi = pageY[p];
    uint8_t vx = xToVal(padX);
    uint8_t vy = yToVal(padY);
    unsigned long now = millis();
    if (!force && now - lastPadSend < PAD_GATE_MS) return;
    lastPadSend = now;
    bool chg = false;
    if (force || vx != ccVal[targetIdx][xi]) { setVal(xi, vx); chg = true; }
    if (force || vy != ccVal[targetIdx][yi]) { setVal(yi, vy); chg = true; }
    if (chg) redraw();
}

static void padRestore() {
    uint8_t p = (uint8_t)(page - PG_1);
    uint8_t xi = PAGE_X[p];
    uint8_t yi = pageY[p];
    if (PMOM[xi]) setVal(xi, padSnapX);
    if (PMOM[yi]) setVal(yi, padSnapY);
    redraw();
}

static void setCurIdxByIdx(int8_t idx, bool beep) {
    if (idx == curIdx) return;
    curIdx = idx;
    if (touchActive) {
        if (sentNote >= 0 && sentNote != curIdx) {
            noteOff(sentNote);
            noteOn(curIdx);
            pressNoop = false;
        }
    } else if (latchIdx >= 0 && sentNote == latchIdx) {
        noteOff(latchIdx);
        noteOn(curIdx);
        latchIdx = curIdx;
    }
    if (beep) tick();
    redraw();
}

static void stepFX(int8_t dir) {
    int8_t n = curIdx + dir;
    if (n < 0) n = FX_COUNT - 1;
    if (n >= FX_COUNT) n = 0;
    setCurIdxByIdx(n, true);
}

static void stepY(int8_t dir) {
    uint8_t p  = page - PG_1;
    uint8_t xi = PAGE_X[p];
    uint8_t n  = pageY[p];
    for (int k = 0; k < 11; k++) {
        n = (uint8_t)((n + dir + 11) % 11);
        if (n != xi) break;
    }
    pageY[p] = n;
    tick();
    redraw();
}

static void stepPage(int8_t dir) {
    page = (uint8_t)(((int)page + dir + PG_COUNT) % PG_COUNT);
    M5Dial.Touch.setFlickThresh(page == PG_FX ? 25 : 6);
    tick();
    redraw();
}

static void cfgSelect(int i) {
    if (i == cfgSel) return;
    cfgSel = (int8_t)i;
    tick();
    redraw();
}

static void cfgEdit(int8_t dir) {
    uint8_t* v = cfgVal(cfgSel);
    int n = (int)*v + dir;
    if (n < 1) n = 16;
    if (n > 16) n = 1;
    *v = (uint8_t)n;
    cfgSave();
    tick();
    redraw();
}

static void cycleTarget() {
    targetIdx = (uint8_t)((targetIdx + 1) % 5);
    tick();
    redraw();
}

static void pressAction() {
    unsigned long now = millis();
    pressStart = now;
    bool hadHold = (latchIdx >= 0);
    pendDbl = !hadHold && lastWasShort && lastRelease != 0 && (now - lastRelease) <= GAP_MAX;
    pressNoop = (hadHold && curIdx == latchIdx);
    if (!pressNoop) {
        if (sentNote >= 0) noteOff(sentNote);
        noteOn(curIdx);
    }
    redraw();
}

static void releaseAction() {
    unsigned long now = millis();
    bool wasShort = (now - pressStart) <= CLICK_MAX;
    lastRelease = now;
    lastWasShort = wasShort;
    if (pendDbl && wasShort) {
        lastWasShort = false;
        if (latchIdx == curIdx) {
            if (sentNote >= 0) noteOff(sentNote);
            latchIdx = -1;
            beepUnlatch();
        } else {
            latchIdx = curIdx;
            beepLatch();
        }
    } else if (latchIdx >= 0 && wasShort) {
        if (sentNote >= 0) noteOff(sentNote);
        latchIdx = -1;
        lastWasShort = false;
        beepUnlatch();
    } else if (!pressNoop) {
        if (sentNote >= 0) noteOff(sentNote);
        if (latchIdx >= 0) noteOn(latchIdx);
    }
    pendDbl = false;
    pressNoop = false;
    touchActive = false;
    redraw();
}

static int tileAt(float x, float y) {
    float dx = x - 120.0f, dy = y - 120.0f;
    if (dx * dx + dy * dy > (SEG_R + 8.0f) * (SEG_R + 8.0f)) return -1;
    bool right = (x >= 158.0f);
    if (!right && x > 82.0f) return -1;
    for (int b = 0; b < 3; b++)
        if (y >= (float)(SEG_Y0[b] - 8) && y <= (float)(SEG_Y1[b] + 8))
            return right ? b : 3 + b;
    return -1;
}

static void tileSend(int n, uint8_t val) {
    ccPush(cfgRemCh, TILES[n].cc1, val);
    if (TILES[n].cc2) ccPush(cfgRemCh, TILES[n].cc2, val);
}

static void handleFxTouch() {
    auto t = M5Dial.Touch.getDetail();
    float dx = t.x - CX, dy = t.y - CY;
    float dist = sqrtf(dx * dx + dy * dy);

    if (t.wasPressed() && dist < 75.0f) {
        touchActive = true;
        pressAction();
    }

    if (t.wasFlicked()) {
        float bx = t.base_x - CX, by = t.base_y - CY;
        float bdist = sqrtf(bx * bx + by * by);
        float mdx = t.distanceX(), mdy = t.distanceY();
        bool vert = fabsf(mdy) > fabsf(mdx);
        if (!vert && !touchActive && bdist >= 75.0f) {
            stepFX(mdx > 0 ? +1 : -1);
        }
        if (touchActive) releaseAction();
    } else if (t.wasReleased() && touchActive) {
        releaseAction();
    } else if (t.wasClicked() && !touchActive && dist >= 75.0f) {
        float ang = atan2f(dy, dx) / D2R;
        int i = lroundf((ang + 90.0f) / (360.0f / FX_COUNT));
        i %= FX_COUNT;
        if (i < 0) i += FX_COUNT;
        setCurIdxByIdx((int8_t)i, true);
    }

    if (touchActive && t.state == m5::touch_state_t::none) releaseAction();
}

static void handleParamPage() {
    auto t = M5Dial.Touch.getDetail();

    if (!padActive && !chipArmed && t.isPressed()) {
        auto r = M5Dial.Touch.getTouchPointRaw();
        float rx = (float)r.x, ry = (float)r.y;
        float pdx = rx - CX, pdy = ry - CY;
        float pdist = sqrtf(pdx * pdx + pdy * pdy);
        if (rx >= 80.0f && rx <= 160.0f && ry >= 20.0f && ry <= 54.0f && pdist < 95.0f) {
            chipArmed = true;
            redraw();
        } else if (pdist < 112.0f) {
            uint8_t p = (uint8_t)(page - PG_1);
            padActive = true;
            padPressMs = millis();
            padPendDbl = padLastShort && padLastRel != 0 && (padPressMs - padLastRel) <= GAP_MAX;
            padSnapX = ccVal[targetIdx][PAGE_X[p]];
            padSnapY = ccVal[targetIdx][pageY[p]];
            padX = constrain(rx, 32.0f, 208.0f);
            padY = constrain(ry, 32.0f, 208.0f);
            applyPad(true);
        }
    }

    if (padActive && t.isPressed()) {
        auto r = M5Dial.Touch.getTouchPointRaw();
        padX = constrain((float)r.x, 32.0f, 208.0f);
        padY = constrain((float)r.y, 32.0f, 208.0f);
        applyPad(false);
    }

    if (chipArmed) {
        auto r = M5Dial.Touch.getTouchPointRaw();
        float cdx = (float)r.x - CX, cdy = (float)r.y - CY;
        if (r.x < 74 || r.x > 166 || r.y < 14 || r.y > 60 || sqrtf(cdx * cdx + cdy * cdy) >= 95.0f) {
            chipArmed = false;
            redraw();
        }
    }

    if (!t.isPressed()) {
        if (chipArmed) cycleTarget();
        chipArmed = false;
        if (padActive) {
            padActive = false;
            unsigned long now = millis();
            bool wasShort = (now - padPressMs) <= CLICK_MAX;
            padLastRel = now;
            padLastShort = wasShort;
            if (padPendDbl && wasShort) {
                padHold = !padHold;
                if (padHold) beepLatch(); else beepUnlatch();
                redraw();
            } else if (!padHold) {
                padRestore();
            }
            padPendDbl = false;
        }
    }
}

static void handleTransportPage() {
    auto t = M5Dial.Touch.getDetail();

    if (activeTile < 0 && t.isPressed()) {
        auto r = M5Dial.Touch.getTouchPointRaw();
        int n = tileAt((float)r.x, (float)r.y);
        if (n >= 0) {
            activeTile = (int8_t)n;
            tileSend(n, 127);
            tick();
            redraw();
        }
    }

    if (activeTile >= 0 && !t.isPressed()) {
        tileSend(activeTile, 0);
        activeTile = -1;
        redraw();
    }
}

static void handleCfgPage() {
    auto t = M5Dial.Touch.getDetail();
    if (!t.isPressed()) {
        cfgTouched = false;
        return;
    }
    if (cfgTouched) return;
    cfgTouched = true;
    auto r = M5Dial.Touch.getTouchPointRaw();
    if (r.y < 48 || r.y > 188) return;
    int i = ((int)r.y - 48) / 20;
    if (i > 6) i = 6;
    cfgSelect(i);
}

static void handleTouch() {
    switch (page) {
    case PG_FX:
        handleFxTouch();
        break;
    case PG_TR:
        handleTransportPage();
        break;
    case PG_CFG:
        handleCfgPage();
        break;
    default:
        handleParamPage();
        break;
    }
}

static void pollEncoder() {
    long p = M5Dial.Encoder.read();
    long delta = (p - encPos) * ENC_DIR;
    if (delta == 0) return;
    encPos = p;
    encAccum += delta;
    while (encAccum >= ENC_COUNTS_PER_DETENT) {
        encAccum -= ENC_COUNTS_PER_DETENT;
        if (M5Dial.BtnA.isPressed()) { menuTurn = true; stepPage(+1); }
        else if (page == PG_FX) stepFX(+1);
        else if (page >= PG_1 && page <= PG_6) stepY(+1);
        else if (page == PG_CFG) cfgEdit(+1);
    }
    while (encAccum <= -ENC_COUNTS_PER_DETENT) {
        encAccum += ENC_COUNTS_PER_DETENT;
        if (M5Dial.BtnA.isPressed()) { menuTurn = true; stepPage(-1); }
        else if (page == PG_FX) stepFX(-1);
        else if (page >= PG_1 && page <= PG_6) stepY(-1);
        else if (page == PG_CFG) cfgEdit(-1);
    }
}

void setup() {
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    M5Dial.Display.setBrightness(128);
    M5Dial.Display.setTextWrap(false);
    M5Dial.Touch.setHoldThresh(800);
    M5Dial.Touch.setFlickThresh(25);

    midiHandler.begin();
    midiHandler.addTransport(&usbHost);

    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.drawCircle(120, 120, 118, C_RED_DIM);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
    M5Dial.Display.setTextColor(C_RED, TFT_BLACK);
    M5Dial.Display.drawString("TAP TO START", CX, 100);
    M5Dial.Display.setFont(&fonts::Font2);
    M5Dial.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5Dial.Display.drawString("FM-1 on USB-C", CX, 132);
    M5Dial.Display.setTextColor(C_RED_DIM, TFT_BLACK);
    M5Dial.Display.drawString("by Shark17e", CX, 156);

    while (true) {
        M5Dial.update();
        if (M5Dial.Touch.getDetail().wasClicked() || M5Dial.BtnA.wasPressed()) break;
        delay(10);
    }
    while (M5Dial.BtnA.isPressed()) {
        M5Dial.update();
        delay(10);
    }

    encPos = M5Dial.Encoder.read();
    encAccum = 0;

    cfgLoad();

    for (int t = 0; t < 5; t++)
        for (int i = 0; i < 11; i++)
            ccVal[t][i] = PDEF_INIT[i];

    usbHost.begin();

    panicAll();
    redraw();
    M5Dial.Speaker.tone(2000, 100);
}

void loop() {
    M5Dial.update();
    midiHandler.task();
    outFlush();

    bool connected = usbHost.isConnected();
    if (connected != wasConnected) {
        wasConnected = connected;
        if (connected) {
            outClear();
            ccClear();
            panicAll();
            if (latchIdx >= 0) noteOn(latchIdx);
        } else {
            outClear();
            ccClear();
        }
        redraw();
    }

    if (M5Dial.BtnA.pressedFor(1000) && !panicHeld && !menuTurn) {
        panicHeld = true;
        panicAll();
        latchIdx = -1;
        M5Dial.Speaker.tone(300, 150);
        redraw();
    }
    if (M5Dial.BtnA.wasReleased()) {
        panicHeld = false;
        menuTurn = false;
    }

    handleTouch();
    pollEncoder();
}
