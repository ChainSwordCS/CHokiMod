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

    DmaConfig_ *dmacfg = (DmaConfig_*) &dmacfgblk;

    dmacfg->channelId = -1;
    dmacfg->flags = 0b11000100;

    dmacfg->src_deviceId = 0xFF;
    dmacfg->src_allowedAlignments = 8|4|2|1;

    dmacfg->dst_deviceId = 0xFF;
    dmacfg->dst_allowedAlignments = 8|4|2|1;

    return;
}

void updateDmaCfgBpp(u8* dmacfgblk, u8 source_bpp, u8 interlaced, u32 rowstride)
{
    DmaConfig_ *dmacfg = (DmaConfig_*) &dmacfgblk;

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

    dmacfg->dst_burstSize = destination_bytesperpixel;
    dmacfg->dst_burstStride = destination_bytesperpixel;

    dmacfg->dst_transferSize = destination_bytesperpixel * width;
    dmacfg->dst_transferStride = destination_bytesperpixel * width;

    dmacfg->src_burstSize = destination_bytesperpixel;

    if(interlaced)
        dmacfg->src_burstStride = source_bytesperpixel * 2;
    else
        dmacfg->src_burstStride = source_bytesperpixel;

    dmacfg->src_transferSize = destination_bytesperpixel * width;
    dmacfg->src_transferStride = rowstride;
    //((u16*)dmacfgblk)[CFG_OFFS_SRC_S16_SCATTER_STRIDE/2] = source_bytesperpixel * width;
}

void updateDmaCfg_GBVC(u8* dmacfgblk, u8 source_bpp, u8 interlaced, u32 rowstride)
{
    if(source_bpp != 16)
    {
        updateDmaCfgBpp(dmacfgblk, source_bpp, interlaced, rowstride);
        return;
    }
    else
    {
        DmaConfig_ *dmacfg = (DmaConfig_*) &dmacfgblk;

        const u8 width = 160; // vertical resolution of GB screen

        dmacfg->dst_burstSize = 2;
        dmacfg->dst_burstStride = 2;

        dmacfg->dst_transferSize = 2 * width;
        dmacfg->dst_transferStride = 2 * width;

        dmacfg->src_burstSize = 2;

        if(interlaced)
            dmacfg->src_burstStride = 2 * 2;
        else
            dmacfg->src_burstStride = 2;

        dmacfg->src_transferSize = 2 * width;
        dmacfg->src_transferStride = rowstride + 2*80; // 240 - 160 = 80 pixels to skip
    }
}
