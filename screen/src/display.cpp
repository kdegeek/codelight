#include "display.h"
#include "logo_assets.h"

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
#define COL_PANEL    0x0841
#define COL_OFFLINE  0x4208
#define ROW_TOP      16
#define ROW_H        54
#define X_MARGIN     8

enum RenderMode {
    RENDER_UNKNOWN,
    RENDER_OFFLINE,
    RENDER_ATTENTION,
    RENDER_ROWS
};

static RenderMode renderedMode = RENDER_UNKNOWN;
static uint8_t renderedRows = 0;

static uint16_t dimColor(uint16_t color) {
    uint8_t r = ((color >> 11) & 0x1F) >> 1;
    uint8_t g = ((color >> 5) & 0x3F) >> 1;
    uint8_t b = (color & 0x1F) >> 1;
    return (r << 11) | (g << 5) | b;
}

static uint16_t pressureColor(float pct, uint16_t accent) {
    if (pct >= 0.90f) return 0xF9A6;
    if (pct >= 0.70f) return 0xFDE0;
    return accent;
}

static const LogoAsset* logoForProvider(const String& provider) {
    for (uint8_t i = 0; i < sizeof(LOGO_ASSETS) / sizeof(LOGO_ASSETS[0]); i++) {
        if (provider == LOGO_ASSETS[i].provider) return &LOGO_ASSETS[i];
    }
    return nullptr;
}

static bool logoPixel(const uint8_t* data, int x, int y) {
    int index = y * (LOGO_BITMAP_SIZE / 8) + (x / 8);
    uint8_t byte = pgm_read_byte(data + index);
    return (byte & (1 << (x % 8))) != 0;
}

static void drawLogoBitmap(const LogoAsset* logo, int x, int y, int size, uint16_t color) {
    if (!logo) return;
    int scale = max(1, size / LOGO_BITMAP_SIZE);
    int drawn = LOGO_BITMAP_SIZE * scale;
    int ox = x + (size - drawn) / 2;
    int oy = y + (size - drawn) / 2;
    for (int yy = 0; yy < LOGO_BITMAP_SIZE; yy++) {
        for (int xx = 0; xx < LOGO_BITMAP_SIZE; xx++) {
            if (logoPixel(logo->data, xx, yy)) {
                if (scale == 1) tft.drawPixel(ox + xx, oy + yy, color);
                else tft.fillRect(ox + xx * scale, oy + yy * scale, scale, scale, color);
            }
        }
    }
}

static void drawProviderMark(const DisplayProviderRow& row, int x, int y, int size) {
    const LogoAsset* logo = logoForProvider(row.provider);
    if (logo) {
        drawLogoBitmap(logo, x, y, size, row.accent);
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

static void drawOffline(bool modeChanged) {
    if (!modeChanged) return;
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

static void drawAttention(bool modeChanged) {
    uint16_t bg = displayData.attention.background == TFT_BLACK
        ? dimColor(displayData.attention.accent)
        : displayData.attention.background;
    if (modeChanged) tft.fillScreen(bg);
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
    for (uint8_t i = 0; i < displayData.providerCount && i < MAX_DISPLAY_PROVIDERS; i++) {
        const DisplayProviderRow& row = displayData.providers[i];
        int y = ROW_TOP + i * ROW_H;
        uint16_t panel = row.background == TFT_BLACK ? COL_PANEL : dimColor(row.background);
        tft.fillRoundRect(X_MARGIN, y, 224, ROW_H - 6, 5, panel);
        tft.fillRoundRect(X_MARGIN, y, 6, ROW_H - 6, 3, row.accent);
        drawProviderMark(row, X_MARGIN + 12, y + 6, 36);

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

    if (renderedRows > displayData.providerCount) {
        int clearY = ROW_TOP + displayData.providerCount * ROW_H;
        tft.fillRect(0, clearY, 240, 240 - clearY, COL_BG);
    }
    renderedRows = displayData.providerCount;

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
    RenderMode nextMode;
    if (displayData.authFailed || !displayData.connected) {
        nextMode = RENDER_OFFLINE;
    } else if (displayData.attention.active) {
        nextMode = RENDER_ATTENTION;
    } else {
        nextMode = RENDER_ROWS;
    }

    bool modeChanged = nextMode != renderedMode;
    if (modeChanged) {
        tft.startWrite();
        tft.fillScreen(COL_BG);
        tft.endWrite();
        renderedRows = 0;
    }

    tft.startWrite();
    if (nextMode == RENDER_OFFLINE) {
        drawOffline(modeChanged);
    } else if (nextMode == RENDER_ATTENTION) {
        drawAttention(modeChanged);
    } else {
        drawRows();
    }
    tft.endWrite();
    renderedMode = nextMode;
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

static void svgCodexMark(String& s, int cx, int cy, const String& fill) {
    s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='17' fill='#02060c' stroke='"; s += fill; s += "' stroke-width='2'/>";
    const int pts[][2] = {{0,-10},{9,-5},{9,5},{0,10},{-9,5},{-9,-5}};
    for (auto& pt : pts) {
        s += "<circle cx='"; s += (cx + pt[0]); s += "' cy='"; s += (cy + pt[1]); s += "' r='4' fill='"; s += fill; s += "'/>";
    }
    s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='3' fill='#fff'/>";
}

static void svgClaudeMark(String& s, int cx, int cy, const String& fill) {
    s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='18' fill='#050200'/>";
    s += "<path d='M"; s += cx; s += " "; s += (cy - 18); s += " L"; s += (cx + 5); s += " "; s += (cy - 5);
    s += " L"; s += (cx + 18); s += " "; s += cy; s += " L"; s += (cx + 5); s += " "; s += (cy + 5);
    s += " L"; s += cx; s += " "; s += (cy + 18); s += " L"; s += (cx - 5); s += " "; s += (cy + 5);
    s += " L"; s += (cx - 18); s += " "; s += cy; s += " L"; s += (cx - 5); s += " "; s += (cy - 5);
    s += " Z' fill='"; s += fill; s += "'/>";
    s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='4' fill='#050200'/>";
}

static void svgOllamaMark(String& s, int cx, int cy, const String& fill) {
    s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='18' fill='#050505'/>";
    s += "<path d='M"; s += (cx - 11); s += " "; s += (cy - 6); s += " L"; s += (cx - 8); s += " "; s += (cy - 18);
    s += " L"; s += (cx - 2); s += " "; s += (cy - 6); s += " L"; s += (cx + 2); s += " "; s += (cy - 6);
    s += " L"; s += (cx + 8); s += " "; s += (cy - 18); s += " L"; s += (cx + 11); s += " "; s += (cy - 6);
    s += " Q"; s += (cx + 12); s += " "; s += (cy + 13); s += " "; s += cx; s += " "; s += (cy + 14);
    s += " Q"; s += (cx - 12); s += " "; s += (cy + 13); s += " "; s += (cx - 11); s += " "; s += (cy - 6);
    s += " Z' fill='"; s += fill; s += "'/>";
    s += "<circle cx='"; s += (cx - 5); s += "' cy='"; s += cy; s += "' r='2' fill='#050505'/>";
    s += "<circle cx='"; s += (cx + 5); s += "' cy='"; s += cy; s += "' r='2' fill='#050505'/>";
}

static void svgRainbowMark(String& s, int cx, int cy) {
    static const char* colors[] = {"#ff4fd8", "#ff9f1c", "#ffe66d", "#4cd964", "#00c2ff", "#7b61ff"};
    for (int i = 0; i < 6; i++) {
        s += "<circle cx='"; s += cx; s += "' cy='"; s += cy; s += "' r='"; s += (18 - i * 2);
        s += "' fill='none' stroke='"; s += colors[i]; s += "' stroke-width='2'/>";
    }
    s += "<path d='M"; s += (cx - 13); s += " "; s += cy; s += " H"; s += (cx + 13);
    s += " M"; s += cx; s += " "; s += (cy - 13); s += " V"; s += (cy + 13);
    s += "' stroke='#fff' stroke-width='2'/>";
}

static void svgProviderMark(String& s, const String& provider, int cx, int cy, const String& fill) {
    if (provider == "codex") svgCodexMark(s, cx, cy, fill);
    else if (provider == "claude") svgClaudeMark(s, cx, cy, fill);
    else if (provider == "ollama") svgOllamaMark(s, cx, cy, fill);
    else if (provider == "antigravity") svgRainbowMark(s, cx, cy);
    else svgText(s, cx, cy + 8, fill.c_str(), 20, "middle", provider.substring(0, 1));
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
        svgProviderMark(s, displayData.attention.provider, 120, 110, colorHex(displayData.attention.accent));
        svgText(s, 120, 210, "#fff", 22, "middle", displayData.attention.action);
    } else {
        for (uint8_t i = 0; i < displayData.providerCount && i < MAX_DISPLAY_PROVIDERS; i++) {
            const DisplayProviderRow& row = displayData.providers[i];
            int y = ROW_TOP + i * ROW_H;
            int filled = (int)(126 * constrain(row.pressurePct, 0.0f, 1.0f));
            s += "<rect x='8' y='"; s += y; s += "' width='224' height='48' rx='5' fill='"; s += colorHex(dimColor(row.background)); s += "'/>";
            s += "<rect x='8' y='"; s += y; s += "' width='6' height='48' fill='"; s += colorHex(row.accent); s += "'/>";
            svgProviderMark(s, row.provider, 38, y + 24, colorHex(row.accent));
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
