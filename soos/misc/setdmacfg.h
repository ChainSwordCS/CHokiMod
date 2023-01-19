/*
	setdmacfg.h - Shortcut functions to adjust DMA Config block.

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

#include <3ds.h>

// Note: in modern libctru, DmaConfig is its own object type.
// https://www.3dbrew.org/wiki/Corelink_DMA_Engines
// https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/svc.h



// DMA Config Block offset defines

// -1 = Auto-assign to a free channel (Arm11: 3-7, Arm9:0-1)
#define CFG_OFFS_CHANNEL_SEL 0
// Endian swap size. 0 = None, 2 = 16-bit, 4 = 32-bit, 8 = 64-bit
#define CFG_OFFS_ENDIAN_SWAP_SIZE 1
// Flags
#define CFG_OFFS_FLAGS 2

// Destination Config block

// peripheral ID. FF for ram (it's forced to FF anyway)
#define CFG_OFFS_DST_PERIPHERAL_ID 4
// Allowed Alignments. Defaults to "1|2|4|8" (15). Also acceptable = 4, 8, "4|8" (12)
#define CFG_OFFS_DST_ALLOWED_BURST_SIZES 5
 // burstSize? (Number of bytes transferred in a burst loop. Can be 0, in which case the max allowed alignment is used as a unit.)
#define CFG_OFFS_DST_S16_GATHER_GRANULE_SIZE 6
// burstStride? (Burst loop stride, can be <= 0.)
#define CFG_OFFS_DST_S16_GATHER_STRIDE 8
// transferSize? (Number of bytes transferred in a "transfer" loop, which is made of burst loops.)
#define CFG_OFFS_DST_S16_SCATTER_GRANULE_SIZE 10
// transferStride? ("Transfer" loop stride, can be <= 0.)
#define CFG_OFFS_DST_S16_SCATTER_STRIDE 12

// Source Config block
#define CFG_OFFS_SRC_PERIPHERAL_ID 14
#define CFG_OFFS_SRC_ALLOWED_BURST_SIZES 15
#define CFG_OFFS_SRC_S16_GATHER_GRANULE_SIZE 16
#define CFG_OFFS_SRC_S16_GATHER_STRIDE 18
#define CFG_OFFS_SRC_S16_SCATTER_GRANULE_SIZE 20
#define CFG_OFFS_SRC_S16_SCATTER_STRIDE 22



// Initialize DMA Config Block.
// No need to memset before calling.
// But afterwards, please call updateDmaCfg() to finish setting the variables.
void initCustomDmaCfg(u8*);

// Pass variables:
// void* dmacfgblk = Pointer to DMA Config Block
// u8 source_bpp = Bits Per Pixel of the framebuffer we want to read from.
// u8 interlaced = Do we want to output interlaced video? (1 = yes, 0 = no)
void updateDmaCfg(void*,u8,u8);
