#include "yuvconvert.h"

void yuvConvertFromRGB565(u32* screenbuf, u32 width, int porch, u32 height)
{
    /**
     * It seems like, when each 4 bytes of data is treated as a uint32,
     * the order is as follows (ordered from most to least significant bits)
     *
     * 1. V / Cr (Red Chroma)
     * 2. U / Cb (Blue Chroma)
     * 3. Y / Luma
     * 4. V (2)
     * 5. U (2)
     * 6. Y (2)
     */

    u32 uVar7 = 0;
    u32 offs = 0;

    for(u32 row = 0; row < height; row++)
    {
        for(u32 i = 0; i < width/2; i++)
        {
            /**
             * NOTE: The framebuffer is actually BGR, not RGB.
             * Switch "red" and "blu" to fix the colors :P
             */
            u32 red = (screenbuf[offs+i] & 0xF800F800) >> 8;
            u32 grn = (screenbuf[offs+i] & 0x07C007C0) >> 3;
            u32 blu = (screenbuf[offs+i] & 0x001F001F) << 3;

            u32 a = (grn>>16) * 29;
            u32 b = (blu>>16) * 149;
            u32 c = (red>>16) * 76;
            u32 abc = ((a+b+c) >> 8) << 24;

            u32 d = grn * 29;
            u32 e = blu * 149;
            u32 f = red * 76;
            u32 def = ((d+e+f) >> 8) << 16;

            uVar7 = abc | def | (uVar7 >> 16);

            if(i & 1) // for every other uint32
            {
                screenbuf[offs+(i/2)] = uVar7;
            }
        }
        offs += width/2 + porch/2;
    }
    return;
}

void yuvConvertFromRGBA4(u32* screenbuf, u32 width, int porch, u32 height)
{
    // Actually, prettifying this code is a waste of time.
    u32 uVar4 = 0;
    u32 *puVar2 = screenbuf + 2;
    u32 uVar1 = width >> 1;
    int local_38 = height;
    u32 *puVar3 = puVar2;
    u32 *param_1 = screenbuf;

    do
    {
        do
        {
            // NOTE: The framebuffer is actually BGR, not RGB.

            u32 uVar6 = puVar2[-2];
            u32 uVar5 = puVar2[-1];

            u32 a = (short)((ushort)(uVar5 >> 16) & 0xff) * 29;
            u32 b = (short)((ushort)(uVar5 >> 8) & 0xff) * 149;
            u32 c = (short)(ushort)(u8)(uVar5 >> 24) * 76;
            u32 abc = ((uint)(a + b + c) >> 8) << 24;

            u32 d = (short)(ushort)(u8)(uVar6 >> 16) * 29;
            u32 e = (short)(ushort)(u8)(uVar6 >> 8) * 149;
            u32 f = (short)(ushort)(u8)(uVar6 >> 24) * 76;
            u32 def = ((uint)(d + e + f) >> 8) << 16;

            uVar4 = abc | def | (uVar4 >> 16);

            if(uVar1 & 1)
            {
                *param_1 = uVar4;
                param_1++;
            }
            uVar1--;
            puVar2 += 2;
        } while(uVar1 != 0);
        puVar2 = puVar3 + (width & 0xfffffffe) + (porch >> 1);
        local_38--;
        uVar1 = width >> 1;
        puVar3 = puVar2;
    } while(local_38 != 0);
    return;
}

