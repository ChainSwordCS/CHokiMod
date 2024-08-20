#pragma once

#include <3ds.h>

/**
 * This is actually a reimplementation of hidScanInput,
 * designed to access the HID_PAD IO Registers directly,
 * and bypass the hid service.
 */
void hidScanInputDirectIO();

u32 getkHeld();
u32 getkDown();
u32 getkUp();
