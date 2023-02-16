/*
	imhelper.c - Image Manipulation helper functions

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

#include "imhelper.h"
#include <3ds.h>


// TODO: I should calculate this elsewhere, preferably so it's not being recalc'd every frame.
inline void cvt1624_help1(u32 mywidth, u32 myheight, u8* myscreenbuf, u8** endof24bimg, u8** endof16bimg)
{
	*endof24bimg = (u8*)myscreenbuf + (myheight*mywidth*3) - 3;
	*endof16bimg = (u8*)myscreenbuf + (myheight*mywidth*2) - 2;
	// *sparebuffersiz = (mywidth*myheight*4) - (mywidth*myheight*3);
}
inline void cvt1624_help2_forrgba4(u8* myaddr1, u8* myaddr2)
{
	u8 r = myaddr1[0] & 0b11110000;
	u8 g =(myaddr1[1] & 0b00001111) << 4;
	u8 b = myaddr1[1] & 0b11110000;

	myaddr2[0] = r;
	myaddr2[1] = g;
	myaddr2[2] = b;
}
inline void cvt1624_help2_forrgb5a1(u8* myaddr1, u8* myaddr2)
{
	u8 r =(myaddr1[0] & 0b00111100) << 2;
	u8 g =(myaddr1[0] & 0b11000000) >> 3;
	   g+=(myaddr1[1] & 0b00000111) << 5;
	u8 b = myaddr1[1] & 0b11111000;

	myaddr2[0] = r;
	myaddr2[1] = g;
	myaddr2[2] = b;
}
inline void cvt1624_help2_forrgb565(u8* myaddr1, u8* myaddr2)
{
	u8 r =(myaddr1[0] & 0b00011111) << 3;
	u8 g =(myaddr1[0] & 0b11100000) >> 3;
	   g+=(myaddr1[1] & 0b00000111) << 5;
	u8 b = myaddr1[1] & 0b11111000;

	myaddr2[0] = r;
	myaddr2[1] = g;
	myaddr2[2] = b;
}


inline void cvt1632i_row1_rgb565(u32 pxnum, u32* fbuf)
{
	u32 temppx = fbuf[pxnum];
	// Blue
	fbuf[pxnum] =(temppx & 0x0000F800) << 8;
	// Green
	fbuf[pxnum]+=(temppx & 0x000007E0) << 5;
	// Red
	fbuf[pxnum]+=(temppx & 0x0000001F) << 3;
}
inline void cvt1632i_row2_rgb565(u32 pxnum, u32* fbuf)
{
	u32 temppx = fbuf[pxnum];
	// Blue
	fbuf[pxnum] =(temppx & 0xF8000000) >> 8;
	// Green
	fbuf[pxnum]+=(temppx & 0x07E00000) >> 11; // 8 + 3
	// Red
	fbuf[pxnum]+=(temppx & 0x001F0000) >> 13; // 8 + 5
}
