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

inline void initCustomDmaCfg(u8* dmacfgblk)
{
	memset(dmacfgblk, 0, sizeof(dmacfgblk));

	dmacfgblk[CFG_OFFS_CHANNEL_SEL] = -1;
	dmacfgblk[CFG_OFFS_FLAGS] = 0b11000000;

	dmacfgblk[CFG_OFFS_DST_PERIPHERAL_ID] = 0xFF;
	dmacfgblk[CFG_OFFS_DST_ALLOWED_BURST_SIZES] = 8|4|2|1;

	dmacfgblk[CFG_OFFS_SRC_PERIPHERAL_ID] = 0xFF;
	dmacfgblk[CFG_OFFS_SRC_ALLOWED_BURST_SIZES] = 8|4|2|1;

	return;
}

void updateDmaCfgBpp(u8* dmacfgblk, u8 source_bpp, u8 interlaced)
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

	// Shortcut: These are all 16-bit integers, but touching the higher byte may be a waste of time.

	// TODO: Potentially rethink how this works and what is most optimal.

	dmacfgblk[1+CFG_OFFS_DST_S16_GATHER_GRANULE_SIZE] = destination_bytesperpixel;
	dmacfgblk[1+CFG_OFFS_DST_S16_GATHER_STRIDE] = destination_bytesperpixel;
	dmacfgblk[1+CFG_OFFS_DST_S16_SCATTER_GRANULE_SIZE] = destination_bytesperpixel;
	dmacfgblk[1+CFG_OFFS_DST_S16_SCATTER_STRIDE] = destination_bytesperpixel;

	dmacfgblk[1+CFG_OFFS_SRC_S16_GATHER_GRANULE_SIZE] = destination_bytesperpixel;
	dmacfgblk[1+CFG_OFFS_SRC_S16_GATHER_STRIDE] = destination_bytesperpixel;

	dmacfgblk[1+CFG_OFFS_SRC_S16_SCATTER_GRANULE_SIZE] = source_bytesperpixel + source_skipbytes;
	dmacfgblk[1+CFG_OFFS_SRC_S16_SCATTER_STRIDE] = source_bytesperpixel + source_skipbytes;
}
