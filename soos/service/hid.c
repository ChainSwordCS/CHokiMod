#include "hid.h"

const u32* HID_PAD_IO_Register = 0x1EC46000;

u32 kHeld = 0;
u32 kDown = 0;
u32 kUp = 0;

void hidScanInputDirectIO()
{
    u32 old_kHeld = kHeld;
    u32 new_kHeld = *HID_PAD_IO_Register;
    kHeld = ~new_kHeld & 0x0FFF;
    kDown = kHeld & ~old_kHeld;
    kUp = old_kHeld & ~kHeld;
}

u32 getkHeld(){ return kHeld; }
u32 getkDown(){ return kDown; }
u32 getkUp(){ return kUp; }
