#include "pattern.h"
#include "../service/mcu.h"

#include <string.h>


RGBLedPattern pat; // Note to self. I should refactor this so it's not a global variable, etc.

// Applies the current color-pattern to the Notif LED
void PatApply()
{
    mcuWriteRegister(0x2D, &pat, sizeof(pat));
}

// Triggers the current color-pattern to the Notif LED
void PatTrigger()
{
    mcuWriteRegister(0x2D, &pat, 4);
}

// Take the current color-pattern, and make it stop pulsing. For Notif LED
void PatStay(u32 col)
{
    memset(&pat.r[0], (col >>  0) & 0xFF, 32);
    memset(&pat.g[0], (col >>  8) & 0xFF, 32);
    memset(&pat.b[0], (col >> 16) & 0xFF, 32);
    
    pat.ani = 0xFF0201;
    
    PatApply();
}

// Make the Notif LED pulse on and off repeatedly?
void PatPulse(u32 col)
{
    memset(&pat.r[ 0], 0xFF, 4);
    memset(&pat.g[ 0], 0xFF, 4);
    memset(&pat.b[ 0], 0xFF, 4);
    memset(&pat.r[ 4], 0, 4);
    memset(&pat.g[ 4], 0, 4);
    memset(&pat.b[ 4], 0, 4);
    memset(&pat.r[ 8], (col >>  0) & 0xFF, 8);
    memset(&pat.g[ 8], (col >>  8) & 0xFF, 8);
    memset(&pat.b[ 8], (col >> 16) & 0xFF, 8);
    memset(&pat.r[16], 0, 4);
    memset(&pat.g[16], 0, 4);
    memset(&pat.b[16], 0, 4);
    
    pat.ani = 0xFF0306;
    
    PatApply();
}

// Set current color-pattern for Notif LED to an array of colors.
void setrave()
{
    //marcus@Werkstaetiun:/media/marcus/WESTERNDIGI/dev_threedee/MCU_examples/RGB_rave$ lua assets/colorgen.lua
    pat.r[0] = 128;
    pat.r[1] = 103;
    pat.r[2] = 79;
    pat.r[3] = 57;
    pat.r[4] = 38;
    pat.r[5] = 22;
    pat.r[6] = 11;
    pat.r[7] = 3;
    pat.r[8] = 1;
    pat.r[9] = 3;
    pat.r[10] = 11;
    pat.r[11] = 22;
    pat.r[12] = 38;
    pat.r[13] = 57;
    pat.r[14] = 79;
    pat.r[15] = 103;
    pat.r[16] = 128;
    pat.r[17] = 153;
    pat.r[18] = 177;
    pat.r[19] = 199;
    pat.r[20] = 218;
    pat.r[21] = 234;
    pat.r[22] = 245;
    pat.r[23] = 253;
    pat.r[24] = 255;
    pat.r[25] = 253;
    pat.r[26] = 245;
    pat.r[27] = 234;
    pat.r[28] = 218;
    pat.r[29] = 199;
    pat.r[30] = 177;
    pat.r[31] = 153;
    pat.g[0] = 238;
    pat.g[1] = 248;
    pat.g[2] = 254;
    pat.g[3] = 255;
    pat.g[4] = 251;
    pat.g[5] = 242;
    pat.g[6] = 229;
    pat.g[7] = 212;
    pat.g[8] = 192;
    pat.g[9] = 169;
    pat.g[10] = 145;
    pat.g[11] = 120;
    pat.g[12] = 95;
    pat.g[13] = 72;
    pat.g[14] = 51;
    pat.g[15] = 33;
    pat.g[16] = 18;
    pat.g[17] = 8;
    pat.g[18] = 2;
    pat.g[19] = 1;
    pat.g[20] = 5;
    pat.g[21] = 14;
    pat.g[22] = 27;
    pat.g[23] = 44;
    pat.g[24] = 65;
    pat.g[25] = 87;
    pat.g[26] = 111;
    pat.g[27] = 136;
    pat.g[28] = 161;
    pat.g[29] = 184;
    pat.g[30] = 205;
    pat.g[31] = 223;
    pat.b[0] = 18;
    pat.b[1] = 33;
    pat.b[2] = 51;
    pat.b[3] = 72;
    pat.b[4] = 95;
    pat.b[5] = 120;
    pat.b[6] = 145;
    pat.b[7] = 169;
    pat.b[8] = 192;
    pat.b[9] = 212;
    pat.b[10] = 229;
    pat.b[11] = 242;
    pat.b[12] = 251;
    pat.b[13] = 255;
    pat.b[14] = 254;
    pat.b[15] = 248;
    pat.b[16] = 238;
    pat.b[17] = 223;
    pat.b[18] = 205;
    pat.b[19] = 184;
    pat.b[20] = 161;
    pat.b[21] = 136;
    pat.b[22] = 111;
    pat.b[23] = 87;
    pat.b[24] = 64;
    pat.b[25] = 44;
    pat.b[26] = 27;
    pat.b[27] = 14;
    pat.b[28] = 5;
    pat.b[29] = 1;
    pat.b[30] = 2;
    pat.b[31] = 8;
}

// Set the current pattern to Rave(tm), and apply it to the Notif LED
void makerave()
{
    setrave();
    PatApply();
}
