/*
	imanip.c - Image Manipulation code

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

#include "imanip.h"
#include <3ds.h>


// TODO: I should calculate this elsewhere, preferably so it's not being recalc'd every frame.
inline void cvt1624_help1(u32 mywidth, u8* myscreenbuf, u8** endof24bimg, u8** endof16bimg)
{
	*endof24bimg = (u8*)myscreenbuf + (240*mywidth*3) - 3;
	*endof16bimg = (u8*)myscreenbuf + (240*mywidth*2) - 2;
	//*sparebuffersiz = (mywidth*240*4) - (mywidth*240*3);
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


// works on o3DS
void convert16to24_rgb5a1(u32 scrbfwidth, u8* scrbf)
{
	u8* buf24; // Copy TO here, starting at the end of the 24bpp buffer
	u8* buf16; // Copy FROM here, starting at the end of the 16bpp buffer
	cvt1624_help1(scrbfwidth, scrbf, &buf24, &buf16); // calc variables

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgb5a1(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}


// works on o3DS
void convert16to24_rgb565(u32 scrbfwidth, u8* scrbf)
{
	u8* buf16;
	u8* buf24;
	cvt1624_help1(scrbfwidth, scrbf, &buf24, &buf16);

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgb565(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}


// works on o3DS
void convert16to24_rgba4(u32 scrbfwidth, u8* scrbf)
{
	u8* buf16;
	u8* buf24;
	cvt1624_help1(scrbfwidth, scrbf, &buf24, &buf16);

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgba4(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}


// soon to be obsolete
void lazyConvert16to32andInterlace(u32 flag, u32 passedsiz, u8* scrbuf, int* interlace_px_offset)
{
	// offs is used to track our progress through the loop.
	u32 offs = 0;

	//u16* u16scrbuf = (void*)scrbuf;

	if(flag == 4) // RGBA4 -> RGBA8
	{
		while((offs + 3) < passedsiz) // This conditional should be good enough to catch errors...
		{
			u8 b = ( scrbuf[offs + *interlace_px_offset] & 0b11110000);
			u8 g = ( scrbuf[offs+1+*interlace_px_offset] & 0b00001111) << 4;
			u8 r = ( scrbuf[offs+1+*interlace_px_offset] & 0b11110000);
			scrbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

			// At compile-time, hopefully this will just be one register(?)
			// i.e. this is hard to read, but I think working with u32 objects instead of u8s will save us CPU time(...?)

			// derive red pixel
			//u32 rgba8pix = (u32)(u8scrbuf[(offs+interlace_px_offset)] & 0b11110000) << 24;
			// derive green pixel
			//rgba8pix = rgba8pix + ( (u32)(u8scrbuf[(offs+interlace_px_offset)] & 0b00001111) << 20 );
			// derive blue pixel
			//rgba8pix = rgba8pix + ( (u32)(u8scrbuf[(offs+1+interlace_px_offset)] & 0b11110000) << 8 );
			//screenbuf[(offs/4)] = rgba8pix;

			offs = offs + 4;
		}
	}
	else if(flag == 3) // RGB5A1 -> RGBA8
	{
		while((offs + 3) < passedsiz)
		{
			u8 b = ( scrbuf[offs + *interlace_px_offset] & 0b00111110) << 2;
			u8 g = ( scrbuf[offs + *interlace_px_offset] & 0b11000000) >> 3;
			g = g +((scrbuf[offs+1+*interlace_px_offset] & 0b00000111) << 5);
			u8 r = ( scrbuf[offs+1+*interlace_px_offset] & 0b11111000);
			scrbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

			offs = offs + 4;
		}
	}
	else if(flag == 2) // RGB565 -> RGBA8
	{
		while((offs + 3) < passedsiz)
		{
			u8 b = ( scrbuf[offs + *interlace_px_offset] & 0b00011111) << 3;
			u8 g = ( scrbuf[offs + *interlace_px_offset] & 0b11100000) >> 3;
			g = g +((scrbuf[offs+1+*interlace_px_offset] & 0b00000111) << 5);
			u8 r = ( scrbuf[offs+1+*interlace_px_offset] & 0b11111000);
			scrbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

			offs = offs + 4;
		}
	}
	else
	{
		// Do nothing; we expect to receive a valid flag.
	}

	// Next frame, do the other set of rows instead.
	if(*interlace_px_offset == 0)
		*interlace_px_offset = 2;
	else
		*interlace_px_offset = 0;

	return;
}


// soon to be obsolete
void fastConvert16to32andInterlace2_rgb565(u32 stride, void* scrbuf, int* interlace_px_offset)
{
	u32 i = 0;
	u32 max = 120*stride;

	if(*interlace_px_offset == 0)
	{
		while(i < max)
		{
			cvt1632i_row1_rgb565(i,(u32*)scrbuf);
			i++;
		}
		*interlace_px_offset = 2;
	}
	else
	{
		while(i < max)
		{
			cvt1632i_row2_rgb565(i,(u32*)scrbuf);
			i++;
		}
		*interlace_px_offset = 0;
	}
}
// Helper functions
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


// obsolete, unused, slightly non-functional
void convert16to24andInterlace(u32 flag, u32 passedsiz)
{
	// Placeholder variables!
	// Defined here so this code will compile.
	// This function is obsolete!
	u32 screenbuf = 0;
	int interlace_px_offset = 0;
	u8* pxarraytwo;
	//


	u32 offs_univ = 0;
	const u32 ofumax = 120*400;

	//const u32 buf2siz = 2*120*400;

	u8* u8scrbuf = (u8*)screenbuf;

	if(interlace_px_offset == 0)
	{
		if(flag == 4) // RGBA4 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs1 = offs_univ*4;

				u8 r = ( u8scrbuf[deriveoffs1+0] & 0b11110000);
				u8 g = ( u8scrbuf[deriveoffs1+1] & 0b00001111) << 4;
				u8 b = ( u8scrbuf[deriveoffs1+1] & 0b11110000);

				u32 deriveoffs2 = offs_univ*2;

				pxarraytwo[deriveoffs2+0] = u8scrbuf[deriveoffs1+2];
				pxarraytwo[deriveoffs2+1] = u8scrbuf[deriveoffs1+3];

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else if(flag == 3) // RGB5A1 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs1 = offs_univ*4;

				u8 r = ( u8scrbuf[deriveoffs1+0] & 0b00111110) << 2;
				u8 g = ( u8scrbuf[deriveoffs1+0] & 0b11000000) >> 3;
				g = g +((u8scrbuf[deriveoffs1+1] & 0b00000111) << 5);
				u8 b = ( u8scrbuf[deriveoffs1+1] & 0b11111000);

				u32 deriveoffs2 = offs_univ*2;

				pxarraytwo[deriveoffs2+0] = u8scrbuf[deriveoffs1+2];
				pxarraytwo[deriveoffs2+1] = u8scrbuf[deriveoffs1+3];

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else if(flag == 2) // RGB565 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs1 = offs_univ*4;

				u8 r = ( u8scrbuf[deriveoffs1+0] & 0b00011111) << 3;
				u8 g = ( u8scrbuf[deriveoffs1+0] & 0b11100000) >> 3;
				g = g +((u8scrbuf[deriveoffs1+1] & 0b00000111) << 5);
				u8 b = ( u8scrbuf[deriveoffs1+1] & 0b11111000);

				u32 deriveoffs2 = offs_univ*2;

				pxarraytwo[deriveoffs2+0] = u8scrbuf[deriveoffs1+2];
				pxarraytwo[deriveoffs2+1] = u8scrbuf[deriveoffs1+3];

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else
		{
			// Do nothing; we expect to receive a valid flag.
		}
		interlace_px_offset = 2;
	}
	else
	{
		// Alternate rows. Complex style...

		if(flag == 4) // RGBA4 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs2 = offs_univ*2;

				u8 r = ( pxarraytwo[deriveoffs2+0] & 0b11110000);
				u8 g = ( pxarraytwo[deriveoffs2+0] & 0b00001111) << 4;
				u8 b = ( pxarraytwo[deriveoffs2+1] & 0b11110000);

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else if(flag == 3) // RGB5A1 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs2 = offs_univ*2;

				u8 r = ( pxarraytwo[deriveoffs2+0] & 0b00111110) << 2;
				u8 g = ( pxarraytwo[deriveoffs2+0] & 0b11000000) >> 3;
				g = g +((pxarraytwo[deriveoffs2+1] & 0b00000111) << 5);
				u8 b = ( pxarraytwo[deriveoffs2+1] & 0b11111000);

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else if(flag == 2) // RGB565 -> RGB8
		{
			while(offs_univ < ofumax) // (offs + 2) < passedsiz && (offstwo+1) < buf2siz )
			{
				u32 deriveoffs2 = offs_univ*2;

				u8 r = ( pxarraytwo[deriveoffs2+0] & 0b00011111) << 3;
				u8 g = ( pxarraytwo[deriveoffs2+0] & 0b11100000) >> 3;
				g = g +((pxarraytwo[deriveoffs2+1] & 0b00000111) << 5);
				u8 b = ( pxarraytwo[deriveoffs2+1] & 0b11111000);

				u32 deriveoffs3 = offs_univ*3;

				u8scrbuf[deriveoffs3] = r;
				u8scrbuf[deriveoffs3+1] = g;
				u8scrbuf[deriveoffs3+2] = b;

				offs_univ++;
			}
		}
		else
		{
			// Do nothing; we expect to receive a valid flag.
		}
		interlace_px_offset = 0;
	}

	return;
}


// placeholder, completely non-functional
void dummyinterlace24(u32 passedsiz, u32 scrbfwidth)
{
	// PLACEHOLDER! This is not intended to function!
	// This is just so the code compiles.
	u32 screenbuf;
	//


	u32 offs2 = 0;

	// Used as a spare buffer.
	// This is critical on Old-3DS,
	// (This is only used when we have a 32bpp image,
	// so it's free memory otherwise! :)
	//
	// Buffer starts at refaddr_endof24bimg + 1
	u32 sparebuffersiz;

	u8* refaddr_endof24bimg;
	u8* refaddr_endof16bimg;
	//cvt1624_help1(scrbfwidth, &refaddr_endof24bimg, &refaddr_endof16bimg, &sparebuffersiz);

	// Address to read from, the first of two bytes of
	// the very last pixel of the 16bpp image.
	u8* addr1 = refaddr_endof16bimg - 1;
	// Address to write to, the first of three bytes of
	// the very last pixel of the 24bpp image.
	u8* addr2 = refaddr_endof24bimg - 2;

	//u32* addr3 = 0;

	u8* addr4 = 0;

	u32 pixelsdrawn = 0;
	u32 maxpix = scrbfwidth * 240;

	// this While-loop is only Part 1.
	// When these two addresses are too close together,
	// we move on to Part 2.
	while(addr1 + 1 < addr2 && addr1 >= (u8*)screenbuf)
	{
		cvt1624_help2_forrgb5a1(addr1,addr2);

		// Increment and decrement
		pixelsdrawn++;
		addr1 -= 2;
		addr2 -= 3;
	}

	// Big Part 2
	while(0)//(pixelsdrawn <= maxpix)
	{
		offs2 = 0;
		addr4 = refaddr_endof24bimg + 1;

		// Copy from 16bpp framebuffer to spare buffer
		while(addr1 >= (u8*)screenbuf && pixelsdrawn < maxpix && addr4 <= (refaddr_endof24bimg+sparebuffersiz))
		{
			//cvt1624_help3(addr4,addr1);

			addr1 -= 2;
			addr4 += 2;
		}

		if(addr1 < (u8*)screenbuf)
			addr1 = (u8*)screenbuf;

		offs2 = 0;
		addr4 = refaddr_endof24bimg + 1;

		// Copy from spare buffer to 24bpp framebuffer
		while(addr1 <= addr2 && offs2 + 1 < sparebuffersiz && pixelsdrawn < maxpix)
		{
			cvt1624_help2_forrgb5a1((u8*)addr4,(u8*)addr2);
			// Increment
			addr2 -= 3;
			offs2 += 2;
			addr4 += 2;
			pixelsdrawn++;
		}

		// Loop back on Part 2 (:
	}

	// return;
}
