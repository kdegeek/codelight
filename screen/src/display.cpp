#include "display.h"

TFT_eSPI tft = TFT_eSPI();

static DisplayProviderRow emptyRow() {
    DisplayProviderRow row;
    row.provider = "";
    row.mark = "";
    row.accent = TFT_WHITE;
    row.background = TFT_BLACK;
    row.pressurePct = 0.0f;
    row.animated = false;
    row.rainbow = false;
    return row;
}

DisplayData displayData = {
    {emptyRow(), emptyRow(), emptyRow(), emptyRow()},
    0,
    {false, "", "", "", "", TFT_WHITE, TFT_BLACK, false, false},
    STATUS_OFFLINE,
    false,
    false,
    0
};

#define COL_BG       TFT_BLACK
#define COL_TEXT     0xFFFF
#define COL_MUTED    0x8410
#define COL_PANEL    0x1082
#define COL_OFFLINE  0x4208
#define ROW_TOP      16
#define ROW_H        54
#define X_MARGIN     8

static uint16_t dimColor(uint16_t color) {
    uint8_t r = ((color >> 11) & 0x1F) >> 1;
    uint8_t g = ((color >> 5) & 0x3F) >> 1;
    uint8_t b = (color & 0x1F) >> 1;
    return (r << 11) | (g << 5) | b;
}

static uint16_t pressureColor(float pct, uint16_t accent) {
    if (pct >= 0.90f) return TFT_RED;
    if (pct >= 0.70f) return 0xFD20;
    return accent;
}

static void drawRainbowMark(int x, int y, int size) {
    static const uint16_t colors[] = {TFT_RED, 0xFD20, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_BLUE, 0xA81F};
    int stripe = max(1, size / 7);
    for (int i = 0; i < 7; i++) {
        tft.fillRect(x, y + i * stripe, size, stripe + 1, colors[i]);
    }
    tft.drawRect(x, y, size, size, TFT_WHITE);
}

static void drawCodexMark(int x, int y, int size, uint16_t accent, bool animated) {
    int bob = animated ? (displayData.animationTick % 8 < 4 ? 1 : -1) : 0;
    tft.fillRoundRect(x, y + bob, size, size, size / 5, dimColor(accent));
    tft.drawRoundRect(x, y + bob, size, size, size / 5, accent);
    tft.fillCircle(x + size / 3, y + bob + size / 3, max(1, size / 12), TFT_WHITE);
    tft.fillCircle(x + (size * 2) / 3, y + bob + size / 3, max(1, size / 12), TFT_WHITE);
    tft.drawFastHLine(x + size / 3, y + bob + (size * 2) / 3, size / 3, TFT_WHITE);
}

static void drawClaudeMark(int x, int y, int size, uint16_t accent, bool animated) {
    int pulse = animated ? (displayData.animationTick % 10 < 5 ? 1 : 0) : 0;
    int cx = x + size / 2;
    int cy = y + size / 2;
    tft.fillCircle(cx, cy, size / 3 + pulse, accent);
    tft.fillCircle(cx - size / 5, cy, size / 6, dimColor(accent));
    tft.fillCircle(cx + size / 5, cy, size / 6, dimColor(accent));
    tft.fillCircle(cx, cy - size / 5, size / 6, dimColor(accent));
    tft.fillCircle(cx, cy + size / 5, size / 6, dimColor(accent));
}

static void drawOllamaMark(int x, int y, int size, uint16_t accent) {
    tft.fillCircle(x + size / 2, y + size / 2, size / 2, TFT_BLACK);
    tft.drawCircle(x + size / 2, y + size / 2, size / 2 - 1, accent);
    tft.fillTriangle(
        x + size / 2, y + size / 5,
        x + size / 4, y + (size * 3) / 4,
        x + (size * 3) / 4, y + (size * 3) / 4,
        accent);
}

static void drawProviderMark(const DisplayProviderRow& row, int x, int y, int size) {
    if (row.rainbow || row.provider == "antigravity") {
        drawRainbowMark(x, y, size);
    } else if (row.provider == "codex") {
        drawCodexMark(x, y, size, row.accent, row.animated);
    } else if (row.provider == "claude") {
        drawClaudeMark(x, y, size, row.accent, row.animated);
    } else if (row.provider == "ollama") {
        drawOllamaMark(x, y, size, row.accent);
    } else {
        tft.fillCircle(x + size / 2, y + size / 2, size / 2, row.accent);
    }
}

static void drawProviderMark(const DisplayAttention& attention, int x, int y, int size) {
    DisplayProviderRow row;
    row.provider = attention.provider;
    row.mark = attention.mark;
    row.accent = attention.accent;
    row.background = attention.background;
    row.pressurePct = 1.0f;
    row.animated = attention.animated;
    row.rainbow = attention.rainbow;
    drawProviderMark(row, x, y, size);
}

static void drawOffline() {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, 240, 240, COL_OFFLINE);
    const char* label = displayData.authFailed ? "AUTH" : "OFF";
    tft.setTextFont(4);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, COL_OFFLINE);
    int tw = tft.textWidth(label);
    tft.setCursor((240 - tw) / 2, 106);
    tft.print(label);
}

static void drawAttention() {
    uint16_t bg = displayData.attention.background == TFT_BLACK
        ? dimColor(displayData.attention.accent)
        : displayData.attention.background;
    tft.fillScreen(bg);
    drawProviderMark(displayData.attention, 50, 30, 140);

    String action = displayData.attention.action.length() > 0 ? displayData.attention.action : "OPEN";
    action.toUpperCase();
    tft.setTextFont(4);
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT, bg);
    int tw = tft.textWidth(action);
    tft.setCursor(max(0, (240 - tw) / 2), 188);
    tft.print(action);
}

static void drawRows() {
    tft.fillScreen(COL_BG);
    for (uint8_t i = 0; i < displayData.providerCount && i < MAX_DISPLAY_PROVIDERS; i++) {
        const DisplayProviderRow& row = displayData.providers[i];
        int y = ROW_TOP + i * ROW_H;
        uint16_t panel = row.background == TFT_BLACK ? COL_PANEL : dimColor(row.background);
        tft.fillRoundRect(X_MARGIN, y, 224, ROW_H - 6, 5, panel);
        tft.fillRoundRect(X_MARGIN, y, 6, ROW_H - 6, 3, row.accent);
        drawProviderMark(row, X_MARGIN + 14, y + 8, 32);

        int barX = 58;
        int barY = y + 20;
        int barW = 126;
        int barH = 10;
        float pct = constrain(row.pressurePct, 0.0f, 1.0f);
        uint16_t fill = pressureColor(pct, row.accent);
        tft.fillRect(barX, barY, barW, barH, COL_BG);
        tft.fillRect(barX, barY, (int)(barW * pct), barH, fill);

        char buf[6];
        snprintf(buf, sizeof(buf), "%3d%%", (int)(pct * 100.0f + 0.5f));
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(COL_TEXT, panel);
        tft.setCursor(190, y + 16);
        tft.print(buf);
    }

    if (displayData.providerCount == 0) {
        tft.setTextFont(4);
        tft.setTextSize(1);
        tft.setTextColor(COL_MUTED, COL_BG);
        tft.setCursor(55, 106);
        tft.print("NO DATA");
    }
}

void displayInit() {
    tft.init();
    tft.setRotation(0);
    tft.setSwapBytes(true);
    tft.fillScreen(COL_BG);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);   // active LOW = backlight on
}

void displayUpdate() {
    displayData.animationTick++;
    if (displayData.authFailed || !displayData.connected) {
        drawOffline();
    } else if (displayData.attention.active) {
        drawAttention();
    } else {
        drawRows();
    }
    displayUpdateClock();
}

static String colorHex(uint16_t color) {
    uint8_t r = ((color >> 11) & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x3F) << 2;
    uint8_t b = (color & 0x1F) << 3;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return String(buf);
}

static void svgText(String& s, int x, int y, const char* fill, int sz,
                    const char* anchor, const String& text) {
    s += "<text x='"; s += x; s += "' y='"; s += y;
    s += "' fill='"; s += fill;
    s += "' font-family='monospace' font-size='"; s += sz; s += "'";
    if (anchor) { s += " text-anchor='"; s += anchor; s += "'"; }
    s += ">"; s += text; s += "</text>";
}

String generateScreenSvg() {
    String s;
    s.reserve(1800);
    s = "<svg xmlns='http://www.w3.org/2000/svg' width='240' height='240'>";
    s += "<rect width='240' height='240' fill='#000'/>";

    if (displayData.authFailed || !displayData.connected) {
        s += "<rect width='240' height='240' fill='#404040'/>";
        svgText(s, 120, 128, "#000", 28, "middle", displayData.authFailed ? "AUTH" : "OFF");
    } else if (displayData.attention.active) {
        s += "<rect width='240' height='240' fill='"; s += colorHex(displayData.attention.background); s += "'/>";
        svgText(s, 120, 120, colorHex(displayData.attention.accent).c_str(), 72, "middle", displayData.attention.mark);
        svgText(s, 120, 210, "#fff", 22, "middle", displayData.attention.action);
    } else {
        for (uint8_t i = 0; i < displayData.providerCount && i < MAX_DISPLAY_PROVIDERS; i++) {
            const DisplayProviderRow& row = displayData.providers[i];
            int y = ROW_TOP + i * ROW_H;
            int filled = (int)(126 * constrain(row.pressurePct, 0.0f, 1.0f));
            s += "<rect x='8' y='"; s += y; s += "' width='224' height='48' rx='5' fill='"; s += colorHex(dimColor(row.background)); s += "'/>";
            s += "<rect x='8' y='"; s += y; s += "' width='6' height='48' fill='"; s += colorHex(row.accent); s += "'/>";
            svgText(s, 38, y + 33, colorHex(row.accent).c_str(), 24, "middle", row.mark);
            s += "<rect x='58' y='"; s += (y + 20); s += "' width='126' height='10' fill='#000'/>";
            s += "<rect x='58' y='"; s += (y + 20); s += "' width='"; s += filled; s += "' height='10' fill='"; s += colorHex(pressureColor(row.pressurePct, row.accent)); s += "'/>";
            char buf[6];
            snprintf(buf, sizeof(buf), "%d%%", (int)(row.pressurePct * 100.0f + 0.5f));
            svgText(s, 218, y + 32, "#fff", 13, "end", buf);
        }
    }
    s += "</svg>";
    return s;
}

void displayUpdateClock() {
    if (displayData.attention.active) {
        displayData.animationTick++;
    }
}
