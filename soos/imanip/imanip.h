/*
	imanip.h - Image Manipulation code

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
#include "imhelper.h"




// In the given framebuffer, converts a 16bpp image to a 24bpp image.
// The three different 16bpp formats each have their own corresponding
// version of this function (for runtime optimization)
//
// Pass variables:
// u32 scrbfwidth - Screen buffer width, in pixels (where "height" is usually 240,
//                  and "width" is always either 320 or 400 on n3DS)
// u8* scrbf - Pointer to screen buffer
//
// Decently-written
// Designed for compatibility, to work on old-3DS.
//
// Writes to the same buffer it reads from, but fills the buffer backwards.
// Basically. And it doesn't interfere with itself, or corrupt any data.
//
// Pixels are treated as Progressive.
// Thus, interlacing is done at time of DMA if at all.
// But these functions don't know or care about that.
//
// Errata:
// With a 16bpp input in RGBA4 format, the Alpha channel nibble is effectively ignored.
// As far as I know the 3DS doesn't really use the Alpha channel of each pixel in the framebuffer.
// In any case that it does, unexpected issues may arise.
//
void convert16to24_rgb5a1(u32,u8*);
void convert16to24_rgb565(u32,u8*);
void convert16to24_rgba4(u32,u8*);




// Converts 16bpp to 32bpp (RGBA8 / constant 'TJPF_RGBX')
//
// Pass variables:
// u32 flag      - option flag (4 = RGBA4, 3 = RGB5A1, 2 = RGB565)
// u32 passedsiz - size of current 16bpp frame (in bytes)
// u8* scrbuf    - pointer to framebuffer
// int* interlace_px_offset - &interlace_px_offset
//
// Proof-of-concept.
// Implementation is hacky, destructive, and not optimal.
// This will be obsolete when the DMA code is finished being refactored
// and optimized for interlaced functionality (both o3DS and n3DS)
//
// Errata:
// This function makes the Alpha byte always zero.
// The Alpha byte is usually ignored anyway, but if not
// then unexpected issues may arise.
//
void lazyConvert16to32andInterlace(u32,u32,u8*,int*);


// Rewrite of 16->32 bpp function (only finished for RGB565 input)
//
// Pass variables:
// u32 stride   - ?
// void* scrbuf - pointer to framebuffer
// int* interlace_px_offset - &interlace_px_offset
//
// Implementation is still destructive.
// Will be fully obsolete when DMA code is fully refactored.
//
// Should work without issue IIRC, or maybe colors are still broken.
//
void fastConvert16to32andInterlace2_rgb565(u32,void*,int*);



// Placeholder; unused and non-functional.
// These will not function as intended if they are used!
// For reference only!
//
void convert16to24andInterlace(u32,u32); // Soon to be fully obsolete. Last meaningfully modified 2022-09-01
void dummyinterlace24(u32,u32); // Last meaningfully modified 2022-09-03

