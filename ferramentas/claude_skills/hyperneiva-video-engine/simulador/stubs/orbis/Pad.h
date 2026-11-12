#pragma once
#include <stdint.h>
enum {
    ORBIS_PAD_BUTTON_UP = 0x0010, ORBIS_PAD_BUTTON_RIGHT = 0x0020, ORBIS_PAD_BUTTON_DOWN = 0x0040, ORBIS_PAD_BUTTON_LEFT = 0x0080,
    ORBIS_PAD_BUTTON_L2 = 0x0100, ORBIS_PAD_BUTTON_R2 = 0x0200, ORBIS_PAD_BUTTON_L1 = 0x0400, ORBIS_PAD_BUTTON_R1 = 0x0800,
    ORBIS_PAD_BUTTON_TRIANGLE = 0x1000, ORBIS_PAD_BUTTON_CIRCLE = 0x2000, ORBIS_PAD_BUTTON_CROSS = 0x4000, ORBIS_PAD_BUTTON_SQUARE = 0x8000
};
typedef struct stick { uint8_t x, y; } stick;
typedef struct OrbisPadData { unsigned int buttons; stick leftStick; stick rightStick; } OrbisPadData;
