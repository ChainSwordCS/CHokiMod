/*
    setdmacfg.c - Shortcut functions to adjust DMA Config block.

    Part of ChirunoMod - A utility background process for the Nintendo 3DS,
    purpose-built for screen-streaming over WiFi.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "setdmacfg.h"
#include <3ds.h>
#include <string.h>
#include <stdio.h>

inline void initCustomDmaCfg(u8* dmacfgblk)
{
    memset(dmacfgblk, 0, 0x18);

    dmacfgblk[CFG_OFFS_CHANNEL_SEL] = -1;
    dmacfgblk[CFG_OFFS_FLAGS] = 0b11000100;

    dmacfgblk[CFG_OFFS_DST_PERIPHERAL_ID] = 0xFF;
    dmacfgblk[CFG_OFFS_DST_ALLOWED_BURST_SIZES] = 8|4|2|1;

    dmacfgblk[CFG_OFFS_SRC_PERIPHERAL_ID] = 0xFF;
    dmacfgblk[CFG_OFFS_SRC_ALLOWED_BURST_SIZES] = 8|4|2|1;

    return;
}

void updateDmaCfgBpp(u8* dmacfgblk, u8 source_bpp, u8 interlaced, u32 rowstride)
{
    u8 destination_bpp;

    // For 32-bit (RGBA8) this skips every fourth byte (the AFAIK unused Alpha channel)
    // Saves RAM but might cause issues with the image in rare edge-cases.
    if(source_bpp == 32){
        destination_bpp = 24;
    }else{
        destination_bpp = source_bpp;
    }

    const u8 source_bytesperpixel = source_bpp / 8;
    const u8 destination_bytesperpixel = destination_bpp / 8;

    const u8 source_skipbytes = interlaced ? source_bytesperpixel : 0;

    const u8 width = 240;
    //const u8 width = interlaced ? 120 : 240;

    // Shortcut: These are all 16-bit integers, but touching the higher byte may be a waste of time.

    // TODO: Potentially rethink how this works and what is most optimal.

    //printf("\n");
    //printf("updateDmaCfgBpp\n");
    //printf("source: %i bits (%i bytes)\n", source_bpp, source_bytesperpixel);
    //printf("destination: %i bits (%i bytes)\n", destination_bpp, destination_bytesperpixel);
    //printf("interlaced=%i ; skip %i bytes every pixel\n", interlaced, source_skipbytes);
    //printf("width=%i\n", width);

    if(!interlaced)
    {
        //printf("rowstride=%i\n", rowstride);
    }
    else
    {
        //rowstride = rowstride/2;
        //printf("rowstride/2=%i\n", rowstride);
    }

    dmacfgblk[1+CFG_OFFS_DST_S16_GATHER_GRANULE_SIZE] = destination_bytesperpixel;
    dmacfgblk[1+CFG_OFFS_DST_S16_GATHER_STRIDE] = destination_bytesperpixel;

    ((u16*)dmacfgblk)[CFG_OFFS_DST_S16_SCATTER_GRANULE_SIZE/2] = destination_bytesperpixel * width;
    ((u16*)dmacfgblk)[CFG_OFFS_DST_S16_SCATTER_STRIDE/2] = destination_bytesperpixel * width;

    dmacfgblk[1+CFG_OFFS_SRC_S16_GATHER_GRANULE_SIZE] = destination_bytesperpixel;

    if(interlaced)
        dmacfgblk[1+CFG_OFFS_SRC_S16_GATHER_STRIDE] = source_bytesperpixel * 2;
    else
        dmacfgblk[1+CFG_OFFS_SRC_S16_GATHER_STRIDE] = source_bytesperpixel;

    ((u16*)dmacfgblk)[CFG_OFFS_SRC_S16_SCATTER_GRANULE_SIZE/2] = destination_bytesperpixel * width;
    ((u16*)dmacfgblk)[CFG_OFFS_SRC_S16_SCATTER_STRIDE/2] = rowstride;
    //((u16*)dmacfgblk)[CFG_OFFS_SRC_S16_SCATTER_STRIDE/2] = source_bytesperpixel * width;
}
