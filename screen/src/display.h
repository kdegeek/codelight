#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#define MAX_DISPLAY_PROVIDERS 4

enum ClaudeStatus {
    STATUS_INACTIVE = 0,
    STATUS_WORKING = 1,
    STATUS_WAITING = 2,
    STATUS_OFFLINE = 3,
    STATUS_AUTH_FAILED = 4
};

struct DisplayProviderRow {
    String provider;
    String mark;
    uint16_t accent;
    uint16_t background;
    float pressurePct;   // 0.0-1.0
    bool animated;
    bool rainbow;
};

struct DisplayAttention {
    bool active;
    String provider;
    String reason;
    String action;
    String mark;
    uint16_t accent;
    uint16_t background;
    bool animated;
    bool rainbow;
};

struct DisplayData {
    DisplayProviderRow providers[MAX_DISPLAY_PROVIDERS];
    uint8_t providerCount;
    DisplayAttention attention;
    ClaudeStatus status;
    bool connected;
    bool authFailed;
    unsigned long animationTick;
};

extern TFT_eSPI tft;
extern DisplayData displayData;

void displayInit();
void displayUpdate();          // full redraw
void displayUpdateClock();     // clock/animation partial update
String generateScreenSvg();    // SVG representation of current display state
