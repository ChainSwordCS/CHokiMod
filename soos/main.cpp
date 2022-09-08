#include <3ds.h>

/*
    ChirunoMod - A utility background process for the Nintendo 3DS,
    purpose-built for screen-streaming over WiFi.

    Original HorizonM (HzMod) code is Copyright (C) 2017 Sono

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

extern "C"
{
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/iosupport.h>

#include <poll.h>
#include <arpa/inet.h>

#include "miscdef.h"
//#include "service/screen.h"
#include "service/mcu.h"
#include "misc/pattern.h"

#include "tga/targa.h"
#include <turbojpeg.h>
}

#include <exception>

#include "utils.hpp"



#define yield() svcSleepThread(1e8)

#define hangmacro()\
{\
    memset(&pat.r[0], 0x7F, 16);\
    memset(&pat.g[0], 0x7F, 16);\
    memset(&pat.b[0], 0x00, 16);\
    memset(&pat.r[16], 0, 16);\
    memset(&pat.g[16], 0, 16);\
    memset(&pat.b[16], 0, 16);\
    pat.ani = 0x1006;\
    PatApply();\
    while(1)\
    {\
        hidScanInput();\
        if(hidKeysHeld() == (KEY_SELECT | KEY_START))\
        {\
            goto killswitch;\
        }\
        yield();\
    }\
}

// Functions from original codebase.
int checkwifi();
int pollsock(int,int,int);
void CPPCrashHandler();


// Image Processing Functions added by me
void lazyConvert16to32andInterlace(u32,u32); // Finished, works. Destructive implementation
void convert16to24andInterlace(u32,u32); // Finished, works. Clean implementation (but needs refactor for o3DS)
void fastConvert16to32andInterlace2_rgb565(u32); // Unfinished, broken colors. (Destructive implementation)
void convert16to24_rgb5a1(u32); // Finished, works.
void convert16to24_rgb565(u32); // Finished, works.
void convert16to24_rgba4(u32); // Finished, works.
void dummyinterlace24(u32,u32); // Very unfinished, broken, do not use.

// Other big functions added by me
inline int setCpuResourceLimit(u32); // Unfinished, doesn't work IIRC.
void waitforDMAtoFinish(void*); // Don't use this. This is only used by a separate thread, started from within netfunc.
void sendDebugFrametimeStats(double,double,double*,double); // It works, and I'm still adding to it.

// Helper functions, added by me.
inline void cvt1632i_row1_rgb565(u32); // Unfinished
inline void cvt1632i_row2_rgb565(u32); // Unfinished
inline void cvt1624_help1(u32,u8**,u8**);
inline void cvt1624_help2_forrgba4(u8*,u8*);
inline void cvt1624_help2_forrgb5a1(u8*,u8*);
inline void cvt1624_help2_forrgb565(u8*,u8*);


// More functions from original codebase.
void netfunc(void*);
int main(); // So you can call main from main (:

// Debug flag for testing; use experimental UDP instead of TCP.
// Defined at compile-time, for now.
const bool debug_useUDP = true;

static int haznet = 0;
int checkwifi()
{
    haznet = 0;
    u32 wifi = 0;
    hidScanInput();
    if(hidKeysHeld() == (KEY_SELECT | KEY_START)) return 0;
    if(ACU_GetWifiStatus(&wifi) >= 0 && wifi) haznet = 1;
    return haznet;
}


int pollsock(int sock, int wat, int timeout = 0)
{
    struct pollfd pd;
    pd.fd = sock;
    pd.events = wat;
    
    if(poll(&pd, 1, timeout) == 1)
        return pd.revents & wat;
    return 0;
}

const int bufsoc_pak_data_offset = 8; // After 8 bytes, the real data begins.

// Socket Buffer class
class bufsoc
{
public:
    
    typedef struct
    {
        //u32 packettype : 8;
        //u32 size : 24;
        u8 data[0];
    } packet;
    
    int socketid;
    u8* bufferptr;
    int bufsize;
    // recvsize is useless; is never read from.
    int recvsize;
    
    bufsoc(int passed_sock, int passed_bufsize)
    {
        bufsize = passed_bufsize;
        bufferptr = new u8[passed_bufsize];
        
        recvsize = 0;
        socketid = passed_sock;
    }
    
    // Destructor
    ~bufsoc()
    {
    	// If this socket buffer is already null,
    	// don't attempt to delete it again.
        if(!this) return;
        close(socketid);
        delete[] bufferptr;
    }
    
    // TODO: consider making these functions 'inline'

    u8 getPakType()
    {
    	return bufferptr[2];
    }

    u8 getPakSubtype()
    {
    	return bufferptr[3];
    }

    // Retrieve the packet size, derived from the byte array
    u32 getPakSize()
    {
		return *( (u32*)(bufferptr+4) );
    }

    // Write to packet size, in the byte array
    void setPakSize(u32 input)
    {
    	*( (u32*)(bufferptr+4) ) = input;
    	return;
    }

    void setPakType(u8 input)
    {
    	bufferptr[2] = input;
    	return;
    }

    void setPakSubtype(u8 input)
    {
    	bufferptr[3] = input;
    	return;
    }

    int avail()
    {
        return pollsock(socketid, POLLIN) == POLLIN;
    }
    
    int readbuf(int flags = 0)
    {
    	puts("attempting recv function call...");
    	yield();

    	//packet* p = pack();

        int ret = recv(socketid, bufferptr+2, 2, flags);

        printf("incoming packet type = %i\npacket subtype = %i\nrecv function return value = %i\n",bufferptr[2],bufferptr[3],ret);

        if(ret < 0) return -errno;
        if(ret < 2) return -1; // if it returned 0, we will now error out of this function

        //Get the reported size from the packet data

        ret = recv(socketid, bufferptr+4, 4, flags);

        u32 reads_remaining = getPakSize(); // "this->" ? maybe?
        printf("incoming packet size = %i\nrecv return value = %i\n",reads_remaining,ret);
        
        if(ret < 0) return -errno;
        if(ret < 4) return -1;

        // Copy data to the buffer

        u32 offs = bufsoc_pak_data_offset; // Starting offset
        while(reads_remaining)
        {
            ret = recv(socketid, &(bufferptr[offs]), reads_remaining, flags);
            if(ret <= 0) return -errno;
            reads_remaining -= ret;
            offs += ret;
        }
        
        // We don't need to do this
        //p->size = size_lol;
        //recvsize = offs;

        return offs;
    }
    
    int wribuf(int flags = 0)
    {
    	//u32 size = getPakSize();
        int mustwri = getPakSize() + 6; // +4?
        int offs = 2; // Start at 2, because we have to send the header.
        int ret = 0;
        
        while(mustwri)
        {
            if(mustwri >> 12)
                ret = send(socketid, &(bufferptr[offs]), 0x1000, flags);
            else
                ret = send(socketid, &(bufferptr[offs]), mustwri, flags);
            if(ret < 0) return -errno;
            mustwri -= ret;
            offs += ret;
        }
        
        return offs;
    }
    
    packet* pack()
    {
        return (packet*)bufferptr;
    }
    
    int errformat(char* c, ...)
    {
        //packet* p = pack();
        
        int len = 0;
        
        va_list args;
        va_start(args, c);

        // Is this line of code broken? I give up
        //len = vsprintf((char*)p->data + 1, c, args);
        len = vsprintf((char*)(bufferptr+8), c, args);

        va_end(args);
        
        if(len < 0)
        {
            puts("out of memory"); //???
            return -1;
        }
        
        //printf("Packet error %i: %s\n", p->packettype, p->data + 1);
        setPakType(0xFF);
        setPakSubtype(0x00);
        setPakSize(len + 2); // Is the +2 necessary?
        
        return wribuf();
    }
};

static jmp_buf __exc;
static int  __excno;

void CPPCrashHandler()
{
    puts("\e[0m\n\n- The application has crashed\n\n");
    
    try
    {
        throw;
    }
    catch(std::exception &e)
    {
        printf("std::exception: %s\n", e.what());
    }
    catch(Result res)
    {
        printf("Result: %08X\n", res);
        //NNERR(res);
    }
    catch(int e)
    {
        printf("(int) %i\n", e);
    }
    catch(...)
    {
        puts("<unknown exception>");
    }
    
    puts("\n");
    
    PatStay(0xFFFFFF);
    PatPulse(0xFF);
    
    svcSleepThread(1e9);
    
    hangmacro();
    
    killswitch:
    longjmp(__exc, 1);
}


extern "C" u32 __get_bytes_per_pixel(GSPGPU_FramebufferFormats format);

const int port = 6464;

static u32 kDown = 0;
static u32 kHeld = 0;
static u32 kUp = 0;

// GPU 'Capture Info' object.
// Data about framebuffers from the GSP.
static GSPGPU_CaptureInfo capin;

// Whether or not this is running on an 'Old' 3DS?
static int isold = 1;

static Result ret = 0;
static int cx = 0;
static int cy = 0;

// Related to screen capture. Dimensions and color format.
static u32 offs[2] = {0, 0};
static u32 limit[2] = {1, 1};
static u32 stride[2] = {80, 80};
static u32 format[2] = {0xF00FCACE, 0xF00FCACE};

// Config Block
static u8 cfgblk[0x100];

// Set to 1 to force Interlaced
// Set to -1 to force Non-Interlaced
// Set to 0 to not force anything
//
// This is only listened to by a few select bits of code
// Intended for when either Interlaced or Progressive mode
// is not yet implemented for a given color format.
static int forceInterlaced = 0;

static int sock = 0;

static struct sockaddr_in sai;
static socklen_t sizeof_sai = sizeof(sai);

static bufsoc* soc = nullptr;

static bufsoc::packet* k = nullptr;

static Thread netthread = 0;
static vu32 threadrunning = 0;

static u32* screenbuf = nullptr;

//static u32* scrbuf_two = nullptr;
static u8 pxarraytwo[2*120*400];

static tga_image img;
static tjhandle jencode = nullptr;

static TickCounter tick_ctr_1;
static TickCounter tick_ctr_2_dma;

// If this is 0, we are converting odd-numbered rows.
// If this is 2, we are converting even-numbered rows.
//
// This variable is separate from the row index numbers.
// Rows of pixels will generally be indexed starting at 1 and ending at 240 (or 120)
static int interlace_px_offset = 0;

// Super lazy, proof-of-concept function for converting 16-bit color images to 24-bit.
// (Note: This function focuses on speed and not having to reallocate anything.)
// Converts to RGBA8 (hopefully) (TJPF_RGBX)
//
// As it is now, Alpha is not read and will always be totally nullified after this function.
// Interpreting the resulting data as RGBA instead of RGBX could cause the
// Alpha channel to be interpreted as 0.
//
// P.S. Sorry my code is dumb and bad and hard to read lol
//
// Note to self... What I actually kinda want to do is to modify the libturbojpeg code
// to accept 16bpp input to the compressor, but that'd be a whole other challenge...
//
void lazyConvert16to32andInterlace(u32 flag, u32 passedsiz)
{
	// offs is used to track our progress through the loop.
	u32 offs = 0;

	// One-time reinterpret as an array of u8 objects
	// u8scrbuf points to the exact same data (if my logic is sound)
	u8* u8scrbuf = (u8*)screenbuf;
	//u16* u16scrbuf = (u16*)screenbuf;

	if(flag == 4) // RGBA4 -> RGBA8
	{
		while((offs + 3) < passedsiz) // This conditional should be good enough to catch errors...
		{
			u8 b = ( u8scrbuf[offs + interlace_px_offset] & 0b11110000);
			u8 g = ( u8scrbuf[offs+1+interlace_px_offset] & 0b00001111) << 4;
			u8 r = ( u8scrbuf[offs+1+interlace_px_offset] & 0b11110000);
			screenbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

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
			u8 b = ( u8scrbuf[offs + interlace_px_offset] & 0b00111110) << 2;
			u8 g = ( u8scrbuf[offs + interlace_px_offset] & 0b11000000) >> 3;
			g = g +((u8scrbuf[offs+1+interlace_px_offset] & 0b00000111) << 5);
			u8 r = ( u8scrbuf[offs+1+interlace_px_offset] & 0b11111000);
			screenbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

			offs = offs + 4;
		}
	}
	else if(flag == 2) // RGB565 -> RGBA8
	{
		while((offs + 3) < passedsiz)
		{
			u8 b = ( u8scrbuf[offs + interlace_px_offset] & 0b00011111) << 3;
			u8 g = ( u8scrbuf[offs + interlace_px_offset] & 0b11100000) >> 3;
			g = g +((u8scrbuf[offs+1+interlace_px_offset] & 0b00000111) << 5);
			u8 r = ( u8scrbuf[offs+1+interlace_px_offset] & 0b11111000);
			screenbuf[(offs/4)] = ((u32)r << 16) + ((u32)g << 8) + ((u32)b << 0);

			offs = offs + 4;
		}
	}
	else
	{
		// Do nothing; we expect to receive a valid flag.
	}

	// Next frame, do the other set of rows instead.
	if(interlace_px_offset == 0)
		interlace_px_offset = 2;
	else
		interlace_px_offset = 0;

	return;
}

void convert16to24andInterlace(u32 flag, u32 passedsiz)
{
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

// Unreadable code alert
inline void cvt1632i_row1_rgb565(u32 offs)
{
	u32* u32ptr = screenbuf + offs;
	u32 temppx = u32ptr[offs];
	// Blue
	u32ptr[offs] = temppx & 0xF8000000;
	// Green
	u32ptr[offs]+=(temppx & 0x07E00000 >> 3);
	// Red
	u32ptr[offs]+=(temppx & 0x001F0000 >> 5);
}

inline void cvt1632i_row2_rgb565(u32 offs)
{
	u32* u32ptr = screenbuf + offs;
	u32 temppx = u32ptr[offs];
	// Blue
	u32ptr[offs] = temppx & 0x0000F800 << 16;
	// Green
	u32ptr[offs]+=(temppx & 0x000007E0 << 13);
	// Red
	u32ptr[offs]+=(temppx & 0x0000001F << 11);
}

// Unfinished, colors are broken. Currently not faster.
// (The bottleneck may be elsewhere right now. -C 2022-09-03)
void fastConvert16to32andInterlace2_rgb565(u32 scrbufwidth)
{
	u32 offs;
	u32 offsmax = 120*scrbufwidth;

	if(interlace_px_offset == 0)
	{
		while(offs < offsmax)
		{
			cvt1632i_row1_rgb565(offs);
			offs++;
		}
	}
	else
	{
		while(offs < offsmax)
		{
			cvt1632i_row2_rgb565(offs);
			offs++;
		}
	}
}

// Helper function 1
inline void cvt1624_help1(u32 mywidth, u8** endof24bimg, u8** endof16bimg)
{
	*endof24bimg = (u8*)screenbuf + (240*mywidth*3) - 3; // -1
	*endof16bimg = (u8*)screenbuf + (240*mywidth*2) - 2; // -1
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

// second argument "scr_width" should be 320 or 400,
// depending on which screen we have a capture of!
//
// Actually, on Old-3DS, this is required to be like the Stride of the screen... so like 50.
void convert16to24_rgb5a1(u32 scrbfwidth)
{
	u8* buf16; // Copy FROM here, starting at the end of the 16bpp buffer
	u8* buf24; // Copy TO here, starting at the end of the 24bpp buffer
	cvt1624_help1(scrbfwidth, &buf24, &buf16); // calc variables

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgb5a1(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}

void convert16to24_rgb565(u32 scrbfwidth)
{
	u8* buf16;
	u8* buf24;
	cvt1624_help1(scrbfwidth, &buf24, &buf16);

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgb565(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}

void convert16to24_rgba4(u32 scrbfwidth)
{
	u8* buf16;
	u8* buf24;
	cvt1624_help1(scrbfwidth, &buf24, &buf16);

	while(buf16 + 1 < buf24)
	{
		cvt1624_help2_forrgba4(buf16,buf24);
		buf16 -= 2;
		buf24 -= 3;
	}
}

void dummyinterlace24(u32 passedsiz, u32 scrbfwidth) // UNFINISHED
{
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
	while(false)//(pixelsdrawn <= maxpix)
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

static u32 pmAppHandle;

// Based on (and slightly modified from) devkitpro/libctru source
//
// svcSetResourceLimitValues(Handle res_limit, LimitableResource* resource_type_list, s64* resource_list, u32 count)
//
inline int setCpuResourceLimit(u32 passed_cpu_time_limit)
{
	srvGetServiceHandle(&pmAppHandle, "pm:app"); // Does this work?

	int ret = 0;
	u32* cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0xA,5,0); // 0x000A0140
	cmdbuf[1] = 0;
	cmdbuf[2] = 9; // RESLIMIT_CPUTIME (iirc)
	cmdbuf[3] = passed_cpu_time_limit;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;

	ret = svcSendSyncRequest(pmAppHandle);

	if(ret < 0)
		return ret;
	else
		return cmdbuf[1];
}

double timems_dmaasync = 0;
u32 dmastatusthreadrunning = 0;
u32 dmafallbehind = 0;
Handle dmahand = 0;

void waitforDMAtoFinish(void* __dummy_arg__)
{
	// Don't have more than one thread running this
	// function at a time. Don't want to accidentally
	// overload and slow the 3DS.
	dmastatusthreadrunning = 1;

	int r1 = 0;

	//while(r1 != 4)// DMASTATE_DONE)
	//{
		//svcSleepThread(1e7); // 10 ms
		//r2 = svcGetDmaState(&r1,dmahand);
	//}


	r1 = svcWaitSynchronization(dmahand,500000); // keep trying and waiting for half a second

	if(r1 >= 0)
	{
		osTickCounterUpdate(&tick_ctr_2_dma);
		timems_dmaasync = osTickCounterRead(&tick_ctr_2_dma);
	}

	dmastatusthreadrunning = 0;
	return;
}

void sendDebugFrametimeStats(double ms_compress, double ms_writesocbuf, double* ms_dma, double ms_convert)
{
	const u32 charbuflimit = 100 + sizeof(char);
	char str1[charbuflimit];
	char str2[charbuflimit];
	char str3[charbuflimit];
	char str4[charbuflimit];

	sprintf(str4,"Image format conversion / interlacing took %g ms\n",ms_convert);
	sprintf(str1,"Image compression took %g ms\n",ms_compress);
	sprintf(str2,"Copying to Socket Buffer (in WRAM) took %g ms\n",ms_writesocbuf);

	if(*ms_dma == 0)
	{
		sprintf(str3,"DMA not yet finished\n");
		dmafallbehind++;
	}
	else
	{
		double ms_dma_localtemp = *ms_dma;
		*ms_dma = 0;
		sprintf(str3,"DMA copy from framebuffer to ChirunoMod WRAM took %g ms (measurement is %i frames behind)\n",ms_dma_localtemp,dmafallbehind);
		dmafallbehind = 0;
	}

	soc->setPakType(0xFF);
	soc->setPakSubtype(03);

	char finalstr[500+sizeof(char)];

	u32 strsiz = sprintf(finalstr,"%s%s%s%s",str4,str1,str2,str3);

	strsiz--;

	for(u32 i=0; i<strsiz; i++)
	{
		((char*)soc->bufferptr + bufsoc_pak_data_offset)[i] = finalstr[i];
	}

	soc->setPakSize(strsiz);
	soc->wribuf();
	return;
}

void netfunc(void* __dummy_arg__)
{
	osTickCounterStart(&tick_ctr_1);
	osTickCounterStart(&tick_ctr_2_dma);

	double timems_processframe = 0;
	double timems_writetosocbuf = 0;
	double timems_formatconvert = 0;

    u32 siz = 0x80;
    u32 bsiz = 1;
    u32 scrw = 1;
    u32 bits = 8;
    
    int scr = 0;
    
    if(isold)
    {
    	// Commented-out before I got here. -C
    	// screenbuf = (u32*)k->data;
    }
    else
    {
    	osSetSpeedupEnable(1);
    }
    
    PatStay(0x00FF00); // Notif LED = Green
    
    format[0] = 0xF00FCACE; //invalidate
    
    u32 procid = 0;

    // Note: in modern libctru, DmaConfig is its own object type.
    u8 dmaconf[0x18];
    memset(dmaconf, 0, sizeof(dmaconf));

    // https://www.3dbrew.org/wiki/Corelink_DMA_Engines
    dmaconf[0] = -1; // -1 = Auto-assign to a free channel (Arm11: 3-7, Arm9:0-1)
    //dmaconf[1] = 0; // Endian swap size. 0 = None, 2 = 16-bit, 4 = 32-bit, 8 = 64-bit
    //dmaconf[2] = 0b11000000; // Flags. Here, SRC_IS_RAM and DST_IS_RAM
    //dmaconf[3] = 0; // Padding.

    // Destination Config block
    //dmaconf[4] is peripheral ID. FF for ram (it's forced to FF anyway)
    //dmaconf[5] is Allowed Burst Sizes. Defaults to "1|2|4|8" (15). Also acceptable = 4, 8, "4|8" (12)
    //dmaconf[6] and [7] are a u16 int, "gather_granule_size"
    //dmaconf[8] and [9] are a u16 int, "gather_stride"
    //dmaconf[10] and [11] are a u16 int, "scatter_granule_size"
    //dmaconf[12] and [13] are a u16 int, "scatter_stride"
    
    //screenInit();
    
    PatPulse(0x7F007F); // Notif LED = Medium Purple
    threadrunning = 1;
    
    // Note: This is a compile-optimization trick.
    // But it could be more elegant.
    do
    {
    	soc->setPakType(0x02);
    	soc->setPakSubtype(0x00);
    	soc->setPakSize(16); // or 4 * 4? Why were we ever calculating that?
        
        u32* kdata = (u32*)(soc->bufferptr+bufsoc_pak_data_offset);
        
        kdata[0] = 1;
        kdata[1] = 240 * 3;
        kdata[2] = 1;
        kdata[3] = 240 * 3;
        soc->wribuf();
    }
    while(0);
    
    // Infinite loop unless it crashes or is halted by another application.
    while(threadrunning)
    {
        if(soc->avail())
        while(1)
        {
            if((kHeld & (KEY_SELECT | KEY_START)) == (KEY_SELECT | KEY_START))
            {
                delete soc;
                soc = nullptr;
                break;
            }
            
            puts("Reading incoming packet...");
            // Consider declaring 'cy' within this function instead of globally.
            int r;
            r = soc->readbuf();
            if(r <= 0)
            {
                printf("Failed to recvbuf: (%i) %s\n", errno, strerror(errno));
                delete soc;
                soc = nullptr;
                break;
            }
            else
            {
            	u8 i = soc->getPakSubtype();
            	u8 j = soc->bufferptr[bufsoc_pak_data_offset];
            	// Only used in one of these, but want to be declared up here.
            	u32 k;
            	u32 l;

                switch(soc->getPakType())
                {
                	case 0x02: // Init (New CHmod / CHokiMod Packet Specification)
                		cfgblk[0] = 1;
                		// TODO: Maybe put sane defaults in here, or in the variable init code.
                		break;

                	case 0x03: // Disconnect (new packet spec)
                		cfgblk[0] = 0;
                		puts("forced dc");
                		delete soc;
                		soc = nullptr;
                		break;
                        
                	case 0x04: // Settings input (new packet spec)

                		switch(i)
                		{
                			case 0x01: // JPEG Quality (1-100%)
                				// Error-Checking
                				if(j > 100)
                					cfgblk[1] = 100;
                				else if(j < 1)
                					cfgblk[1] = 1;
                				else
                					cfgblk[1] = j;
								break;

                			case 0x02: // CPU Cap value / CPU Limit / App Resource Limit

                				// Redundancy check
                				if(j == cfgblk[2])
                					break;

                				// Maybe this is percentage of CPU time? (https://www.3dbrew.org/wiki/APT:SetApplicationCpuTimeLimit)
                				// In which case, values can range from 5% to 89%
                				// (The respective passed values are 5 and 89, respectively)
                				// So I don't know if 0x7F (127) will work.
                				//
                				// Maybe I'm looking at two different things by accident.

                				// Also, it may be required to set the 'reslimitdesc' in exheader a certain way (in cia.rsf)

                				if(j > 0x7F)
                					j = 0x7F;
                				else if(j < 5)
                					j = 5;

                				// This code doesn't work, lol.
                				// Functionality dummied out for now.
                				//setCpuResourceLimit((u32)j);

                				cfgblk[2] = j;

                				break;

                			case 0x03: // Which Screen
                				if(j < 1 || j > 3)
                					cfgblk[3] = 1; // Default to top screen only
                				else
                					cfgblk[3] = j;
                				break;

                			case 0x04: // Image Format (JPEG or TGA?)
                				if(j > 1)
                					cfgblk[4] = 0;
                				else
                					cfgblk[4] = j;
                				break;

                			case 0x05: // Request to use Interlacing (yes or no)
                				if(j == 0)
                					cfgblk[5] = 0;
                				else
                					cfgblk[5] = 1;
                				break;

                			default:
                				// Invalid subtype for "Settings" packet-type
                				break;
                		}
                		break;
                        
                    case 0xFF: // Debug info. Prints to log file, interpreting the Data as u8 char objects.
                    	// Note: packet subtype is ignored, lol.

                    	// Size
                    	k = soc->getPakSize();
                    	// Current offset
                    	l = 0;

                    	if(k > 255) // Error checking; arbitrary limit on text characters.
                    		k = 255;

                    	while(k > 0)
                    	{
                    		printf( (char*)(soc->bufferptr + bufsoc_pak_data_offset) + l);
                    		k--;
                    		l++;
                    	}
                    	break;

                    default:
                        printf("Invalid packet ID: %i\n", soc->getPakType());
                        delete soc;
                        soc = nullptr;
                        break;
                }
                
                break;
            }
        }
        
        if(!soc) break;
        
        sendDebugFrametimeStats(timems_processframe,timems_writetosocbuf,&timems_dmaasync,timems_formatconvert);

        // If index 0 of the config block is non-zero (we are signaled by the PC to init)
        // And this ImportDisplayCaptureInfo function doesn't error...
        if(cfgblk[0] && GSPGPU_ImportDisplayCaptureInfo(&capin) >= 0)
        {
            //test for changed framebuffers
            if\
            (\
                capin.screencapture[0].format != format[0]\
                ||\
                capin.screencapture[1].format != format[1]\
            )
            {
                PatStay(0xFFFF00); // Notif LED = Teal
                
                //fbuf[0] = (u8*)capin.screencapture[0].framebuf0_vaddr;
                //fbuf[1] = (u8*)capin.screencapture[1].framebuf0_vaddr;
                format[0] = capin.screencapture[0].format;
                format[1] = capin.screencapture[1].format;
                
                soc->setPakType(0xFF);
                soc->setPakSubtype(02);
                // Not proud of this. Just pretend it's not here lol
                //"Hello world"
                char ca[12] = {0x48,0x65,0x6C,0x6C,0x6F,0x20,0x77,0x6F,0x00,0x72,0x6C,0x64};
                for(int i=0; i<13; ++i)
                	((char*)soc->bufferptr + bufsoc_pak_data_offset)[i] = ca[i];
                soc->setPakSize(12);

                soc->wribuf();
                
                char cb[20] = {0x46,0x72,0x61,0x6D,0x65,0x62,0x75,0x66,0x66,0x65,0x72,0x20,0X73,0X74,0X72,0X69,0X64,0X65,0X20,0X20};
				for(int i=0; i<21; ++i)
					((char*)soc->bufferptr + bufsoc_pak_data_offset)[i] = cb[i];
				soc->setPakSize(20);

				soc->wribuf();

                //u32* kdata = (u32*)(soc->bufferptr + bufsoc_pak_data_offset);
                
                // framebuf_widthbytesize = stride. distance between the start of two framebuffer rows.
                // (must be a multiple of 8)
                // so: the amount to add to the framebuffer pointer after displaying a scanline

                soc->setPakSubtype(01);

                // Both of these lines of code are interchangeable. They both always return
                // 0x4672616D and I don't know why. ):
                ((u32*)soc->bufferptr)[3] = capin.screencapture[scr].framebuf_widthbytesize;
                //GSPGPU_ReadHWRegs(0x1EF00090, &((u32*)soc->bufferptr)[3],4);

                soc->setPakSize(4);
                soc->wribuf();
                
                //kdata[0] = format[0];
                //kdata[1] = capin.screencapture[0].framebuf_widthbytesize;
                //kdata[2] = format[1];
                //kdata[3] = capin.screencapture[1].framebuf_widthbytesize;
                //soc->wribuf();

                //soc->setPakSize(sizeof(capin));
                //*( (GSPGPU_CaptureInfo*)(kdata) ) = capin;
                //soc->wribuf();
                
                
                if(dmahand)
                {
                    svcStopDma(dmahand);
                    svcCloseHandle(dmahand);
                    dmahand = 0;
                }
                
                procid = 0;
                
                
                //test for VRAM
                if\
                (\
                    (u32)capin.screencapture[0].framebuf0_vaddr >= 0x1F000000\
                    &&\
                    (u32)capin.screencapture[0].framebuf0_vaddr <  0x1F600000\
                )
                {
                    //nothing to do?
                }
                else //use APT fuckery, auto-assume application as all retail applets use VRAM framebuffers
                {
                	// Notif LED = Flashing red and green
                    memset(&pat.r[0], 0xFF, 16);
                    memset(&pat.r[16], 0, 16);
                    memset(&pat.g[0], 0, 16);
                    memset(&pat.g[16], 0xFF, 16);
                    memset(&pat.b[0], 0, 32);
                    pat.ani = 0x2004;
                    PatApply();
                    
                    u64 progid = -1ULL;
                    bool loaded = false;
                    
                    while(1)
                    {
                        loaded = false;
                        while(1)
                        {
                        	// loaded = Registration Status(?) of the specified application.
                            if(APT_GetAppletInfo((NS_APPID)0x300, &progid, nullptr, &loaded, nullptr, nullptr) < 0) break;
                            if(loaded) break;
                            
                            svcSleepThread(15e6);
                        }
                        
                        if(!loaded) break;
                        
                        if(NS_LaunchTitle(progid, 0, &procid) >= 0) break;
                    }
                    
                    if(loaded)
                    {
                    	// Commented-out before I got here. -C
                    	// svcOpenProcess(&prochand, procid);
                    }
                    else
                    {
                    	format[0] = 0xF00FCACE; //invalidate
                    }
                }
                
                PatStay(0x00FF00); // Notif LED = Green
            }
            

            int loopcnt = 2;
            
            // Note: We control how often this loop runs
            // compared to how often the capture info is checked,
            // by changing the loopcnt variable.
            // By default it's 2, which means the ratio is actually 1:1
            while(--loopcnt)
            {
                if(format[scr] == 0xF00FCACE)
                {
                    scr = !scr;
                    continue;
                }
                
                soc->setPakSize(0);
                

                if(dmahand)
                {
                    svcStopDma(dmahand);
                    svcCloseHandle(dmahand);
                    dmahand = 0;
                    // Purpose is to clear the cached data, so we aren't accidentally processing and sending old framebuffer data.
                    // On Old-3DS this isn't necessary because the cache is so small anyway.
                    //
                    // Note that removing this instruction causes strange behavior...
                    if(!isold)
                    {
                    	svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);

                    	if(cfgblk[5] == 1) // If Interlacing requested
                    	{
                    		// Note, I'm not sure if this will be required or not. -C
                    		//svcFlushProcessDataCache(0xFFFF8001, pxarraytwo, 2*120*400);
                    	}
                    }
                }
                
                int imgsize = 0;
                
                u8* kdata = soc->bufferptr + bufsoc_pak_data_offset; // Leaving this as-is should be just fine.

                u8 subtype_aka_flags = 0;



                // TGA
                if(cfgblk[4] == 01)
                {
                	timems_formatconvert = 0;
                	osTickCounterUpdate(&tick_ctr_1);

                	// Note: interlacing not yet implemented here.
                    init_tga_image(&img, (u8*)screenbuf, scrw, stride[scr], bits);
                    img.image_type = TGA_IMAGE_TYPE_BGR_RLE;
                    img.origin_y = (scr * 400) + (stride[scr] * offs[scr]);
                    tga_write_to_FILE(kdata, &img, &imgsize);
                    

                    osTickCounterUpdate(&tick_ctr_1);
                    timems_processframe = osTickCounterRead(&tick_ctr_1);


                    subtype_aka_flags = 0b00001000 + (scr * 0b00010000) + (format[scr] & 0b111);
                    soc->setPakType(01); // Image
                    soc->setPakSubtype(subtype_aka_flags);
                    soc->setPakSize(imgsize);
                }
                // JPEG
                else // This is written profoundly stupid, courtesy of yours truly. I wouldn't have it any other way. -ChainSwordCS
                {
                	u32 f = format[scr] & 0b111;
                	subtype_aka_flags = 0b00000000 + (scr * 0b00010000) + f;

                	int tjpf = 0;

                	// I don't know if this temp variable is required.
                	// I don't know if using 'siz' produces correct results
                	// or if it breaks... -C
                	// TODO: Not this. Do anything but this. Optimize pls.
                	u32 siz_2 = (capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);

                	osTickCounterUpdate(&tick_ctr_1);

                	if(f == 0) // RGBA8
                	{
                		forceInterlaced = -1; // Function not yet implemented
                		tjpf = TJPF_RGBX;
                		//bsiz = 4;
                		//scrw = 240;
                	}
                	else if(f == 1) // RGB8
                	{
                		forceInterlaced = -1; // Function not yet implemented
                		tjpf = TJPF_RGB;
                		//bsiz = 3;
                		//scrw = 240;
                	}
                	else if(f == 2) // RGB565
                	{
                		forceInterlaced = 0;

                		if(cfgblk[5] == 1)
                		{
                			if(isold)
                			{
                				//fastConvert16to32andInterlace2_rgb565(stride[scr]);
                				lazyConvert16to32andInterlace(2,siz_2);
								tjpf = TJPF_RGBX;
								bsiz = 4;
                			}
                			else
                			{
                				convert16to24andInterlace(2,siz_2);
								tjpf = TJPF_RGB;
								bsiz = 3;
                			}
							scrw = 120;
							subtype_aka_flags += 0b00100000 + (interlace_px_offset?0:0b01000000);
                		}
                		else
                		{
                			convert16to24_rgb565(stride[scr]);
                			tjpf = TJPF_RGB;
                			bsiz = 3;
                			scrw = 240;
                		}

                	}
                	else if(f == 3) // RGB5A1
                	{
                		forceInterlaced = 0;

                		if(cfgblk[5] == 1) // Interlace time (:
                		{
							if(isold)
							{
								lazyConvert16to32andInterlace(3,siz_2);
								tjpf = TJPF_RGBX;
								bsiz = 4;
							}
							else
							{
								convert16to24andInterlace(3,siz_2);
								tjpf = TJPF_RGB;
								bsiz = 3;
							}
							scrw = 120;
							subtype_aka_flags += 0b00100000 + (interlace_px_offset?0:0b01000000);
                		}
                		else // Progressive mode
                		{
							convert16to24_rgb5a1(stride[scr]);
							tjpf = TJPF_RGB;
							bsiz = 3;
							scrw = 240;
                		}
                	}
                	else if(f == 4) // RGBA4
                	{
                		forceInterlaced = 0;

                		if(cfgblk[5] == 1)
                		{
							if(isold)
							{
								// We are tight on RAM. Does the old method work better?
								lazyConvert16to32andInterlace(4,siz_2);
								tjpf = TJPF_RGBX;
								bsiz = 4;
							}
							else
							{
								convert16to24andInterlace(4,siz_2);
								tjpf = TJPF_RGB;
								bsiz = 3;
							}
							scrw = 120;
							subtype_aka_flags += 0b00100000 + (interlace_px_offset?0:0b01000000);
                		}
                		else
                		{
                			convert16to24_rgba4(stride[scr]);
                			tjpf = TJPF_RGB;
							bsiz = 3;
							scrw = 240;
                		}
                	}
                	else
                	{
                		// Invalid format, should never happen, but put a failsafe here anyway.
                		//
                		// This failsafe is just taken from the 24-bit code. I don't know if that's the
                		// safest or not, it's just a placeholder. -C
                		tjpf = TJPF_RGB;
                		//bsiz = bsiz;
                		//scrw = 240;
                		forceInterlaced = -1; // Trying to interlace an unknown format would not go well.
                	}

                    osTickCounterUpdate(&tick_ctr_1);
                    timems_formatconvert = osTickCounterRead(&tick_ctr_1);

                	//*(u32*)&k->data[0] = (scr * 400) + (stride[scr] * offs[scr]);
                	//u8* dstptr = &k->data[8];
                	// destination pointer is "kdata" from now on

                	// Original Line:
                 // int r = tjCompress2(jencode, (u8*)screenbuf, scrw, bsiz * scrw, stride[scr], format[scr] ? TJPF_RGB : TJPF_RGBX, &dstptr, (u32*)&imgsize, TJSAMP_420, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT))

                	// "width" is supposed to be 240 always,
                	// unless of course we're doing interlacing shenanigans.

                	// TODO: Important!
                	// For some unknown reason, Mario Kart 7 requires the "width" (height)
                	// to be 128 when interlaced. And possibly 256 or something similar
                	// when not interlaced. No I don't know why.
                	// But I would love to get to the bottom of it.
                	// If I can't, I'll add a debug feature to force-override the number.

                	// stride was always variable (different on Old-3DS vs New-3DS)
                	// so I am not doing anything to that.
                	if(!tjCompress2(jencode, (u8*)screenbuf, scrw, bsiz*scrw, stride[scr], tjpf, &kdata, (u32*)&imgsize, TJSAMP_420, cfgblk[1], TJFLAG_NOREALLOC | TJFLAG_FASTDCT))
                	{
                        osTickCounterUpdate(&tick_ctr_1);
                        timems_processframe = osTickCounterRead(&tick_ctr_1);
                		soc->setPakSize(imgsize);
                	}
                	else
                	{
                		timems_processframe = 0;
                	}

                	soc->setPakType(01); //Image
                	soc->setPakSubtype(subtype_aka_flags);
                }

                // Commented-out before I got here. -C
                //
                //k->size += 4;
                //
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, fbuf[0] + fboffs, siz, dmaconf);
                //svcFlushProcessDataCache(prochand ? prochand : 0xFFFF8001, capin.screencapture[0].framebuf0_vaddr, capin.screencapture[0].framebuf_widthbytesize * 400);
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, (u8*)capin.screencapture[0].framebuf0_vaddr + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, 0x600000 + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, dbgo, siz, dmaconf);
                
                // Current progress through one complete frame
                // (Only applicable to Old-3DS)
                if(++offs[scr] == limit[scr]) offs[scr] = 0;
                
                // TODO: I feel like I could do a much better job optimizing this,
                // but I need to refactor a lot of code to do so. -C
                if(cfgblk[3] == 01) // Top Screen Only
                	scr = 0;
                else if(cfgblk[3] == 02) // Bottom Screen Only
                	scr = 1;
                else if(cfgblk[3] == 03) // Both Screens
                	scr = !scr;
                //else if(cfgblk[0] == 04)
                // Planning to add more complex functionality with prioritizing one
                // screen over the other, like NTR. Maybe.
                
                // TODO: This code will be redundant in the future, if not already.
                // Size of the entire frame (in bytes)
                // TODO: Does this even return a correct value? Even remotely?
                siz = (capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);
                // Size of a single pixel in bytes(????)
                bsiz = capin.screencapture[scr].framebuf_widthbytesize / 240;
                // Screen "Width" (usually 240)
                scrw = capin.screencapture[scr].framebuf_widthbytesize / bsiz;

                bits = 4 << bsiz; // ?
                

                // Intentionally mis-reporting our color bit-depth to the PC client (!)

                // Framebuffer Color Format = RGB565
                //if((format[scr] & 0b111) == 2)
                //{
                //	bits = 17;
                //}
                // Framebuffer Color Format = RGBA4
                //if((format[scr] & 0b111) == 4)
                //{
                //	bits = 18;
                //}

                Handle prochand = 0;
                if(procid) if(svcOpenProcess(&prochand, procid) < 0) procid = 0;
                
                // DMA Code!!!

                // TODO: Future Plan: Rework this DMA function call;
                // try to use special parameters for certain
                // color formats and/or settings (like Interlaced, if requested)
                //
                // Note: I don't currently know enough to make those adjustments.
                // Lots more testing and fine tuning is needed. -ChainSwordCS

                //TODO: Consider optimizing this better in the future too.

                //int fmt = format[scr] & 0b0111;

                if(forceInterlaced != -1 && cfgblk[5] == 1 && interlace_px_offset != 0)
                {
                	// Don't do the DMA lol.

                }
                else
                {
                	u32 srcprochand = prochand ? prochand : 0xFFFF8001;
                	u8* srcaddr = (u8*)capin.screencapture[scr].framebuf0_vaddr + (siz * offs[scr]);

                	osTickCounterUpdate(&tick_ctr_2_dma);
                	int r = svcStartInterProcessDma(&dmahand,0xFFFF8001,screenbuf,srcprochand,srcaddr,siz,dmaconf);

                	if(r < 0)
                	{
                		procid = 0;
						format[scr] = 0xF00FCACE; //invalidate
                	}
                	else
                	{
                		if(dmastatusthreadrunning == 0)
                		{
                			threadCreate(waitforDMAtoFinish, nullptr, 0x4000, 0x10, 1, true);
                		}
                	}
                }

                //if( 0 > svcStartInterProcessDma(
                //		&dmahand, // Note: This handle is signaled when the DMA is finished.
				//		0xFFFF8001, // Shortcut for: the handle of this process
				//		screenbuf, // Screenbuffer pointer
				//		prochand ? prochand : 0xFFFF8001, // 'Source Process Handle' ; if prochand = 0, shortcut to handle of this process
                //		(u8*)capin.screencapture[scr].framebuf0_vaddr + (siz * offs[scr]), // Source Address
				//		bytestocopy, // Bytes to copy
				//		dmaconf) ) // DMA Config / Flags
                //{
                //    procid = 0;
                //    format[scr] = 0xF00FCACE; //invalidate
                //}
                
                if(prochand)
                {
                    svcCloseHandle(prochand);
                    prochand = 0;
                }
                
                // Clear cache here?
                // This doesn't solve anything unfortunately. -C
                //if(!isold) svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);

                // If size is 0, don't send the packet.
                if(soc->getPakSize())
                {
                	osTickCounterUpdate(&tick_ctr_1);
                	soc->wribuf();
                	osTickCounterUpdate(&tick_ctr_1);
					timems_writetosocbuf = osTickCounterRead(&tick_ctr_1);
                }

                // Commented-out before I got here. -C
                /*
                k->packetid = 0xFF;
                k->size = 4;
                *(u32*)k->data = dbgo;
                soc->wribuf();
                
                dbgo += 240 * 3;
                if(dbgo >= 0x600000) dbgo = 0;
                */
                
                // Limit this thread to do other things? (On Old-3DS)
                // TODO: Fine-tune Old-3DS performance. Maybe remove this outright.
                if(isold) svcSleepThread(5e6);
            }
        }
        else yield();
    }
    
    // Notif LED = Flashing yellow and purple
    memset(&pat.r[0], 0xFF, 16);
    memset(&pat.g[0], 0xFF, 16);
    memset(&pat.b[0], 0x00, 16);
    memset(&pat.r[16],0x7F, 16);
    memset(&pat.g[16],0x00, 16);
    memset(&pat.b[16],0x7F, 16);
    pat.ani = 0x0406;
    PatApply();
    
    if(soc)
    {
        delete soc;
        soc = nullptr;
    }
    
    if(dmahand)
    {
        svcStopDma(dmahand);
        svcCloseHandle(dmahand);
    }
    
    //if(prochand) svcCloseHandle(prochand);
    //screenExit();
    
    threadrunning = 0;
}

static FILE* f = nullptr;

ssize_t stdout_write(struct _reent* r, void* fd, const char* ptr, size_t len)
{
    if(!f) return 0;
    fputs("[STDOUT] ", f);
    return fwrite(ptr, 1, len, f);
}

ssize_t stderr_write(struct _reent* r, void* fd, const char* ptr, size_t len)
{
    if(!f) return 0;
    fputs("[STDERR] ", f);
    return fwrite(ptr, 1, len, f);
}

static const devoptab_t devop_stdout = { "stdout", 0, nullptr, nullptr, stdout_write, nullptr, nullptr, nullptr };
static const devoptab_t devop_stderr = { "stderr", 0, nullptr, nullptr, stderr_write, nullptr, nullptr, nullptr };

int main()
{
    mcuInit();
    nsInit();
    
    // Isn't this already initialized to null?
    soc = nullptr;
    
    f = fopen("HzLog.log", "a");
    if(f != NULL)
    {
        devoptab_list[STD_OUT] = &devop_stdout;
		devoptab_list[STD_ERR] = &devop_stderr;

		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
    }
    printf("Hello World? Does this work? lol\n");
    
    memset(&pat, 0, sizeof(pat));
    memset(&capin, 0, sizeof(capin));
    memset(cfgblk, 0, sizeof(cfgblk));
    
    isold = APPMEMTYPE <= 5;
    
    if(isold)
    {
    	// On Old-3DS, capture the screen in 8 chunks
        limit[0] = 8;
        limit[1] = 8;
        stride[0] = 50; // Screen / Framebuffer width (divided by 8)
        stride[1] = 40; // Screen / Framebuffer width (divided by 8)
    }
    else
    {
        limit[0] = 1;
        limit[1] = 1;
        stride[0] = 400; // Screen / Framebuffer width
        stride[1] = 320; // Screen / Framebuffer width
    }
    
    
    PatStay(0x0000FF); // Notif LED = Red
    
    // Initialize AC service, for Wifi stuff.
    acInit();
    
    do
    {
    	// Socket buffer size, smaller on Old-3DS
        u32 siz = isold ? 0x10000 : 0x200000;
        ret = socInit((u32*)memalign(0x1000, siz), siz);
    }
    while(0);

    if(ret < 0) *(u32*)0x1000F0 = ret;//hangmacro();
    
    jencode = tjInitCompress();
    if(!jencode) *(u32*)0x1000F0 = 0xDEADDEAD;//hangmacro();
    
    // Initialize communication with the GSP service, for GPU stuff
    gspInit();
    
    //gxInit();
    
    if(isold)
    {
        screenbuf = (u32*)memalign(8, 50 * 240 * 4);
    }
    else
    {
        screenbuf = (u32*)memalign(8, 400 * 240 * 4);
        //scrbuf_two = (u32*)memalign(8, 400 * 120 * 3); too large...
        //u8 pxarr2data[2*120*400];
        //pxarraytwo = pxarr2data;
    }
    
    // If memalign returns null or 0
    if(!screenbuf)
    {
        makerave();
        svcSleepThread(2e9);
        hangmacro();
    }
    
    
    if((__excno = setjmp(__exc))) goto killswitch;
      
#ifdef _3DS
    std::set_unexpected(CPPCrashHandler);
    std::set_terminate(CPPCrashHandler);
#endif
    
    netreset:
    
    if(sock)
    {
        close(sock);
        sock = 0;
    }
    
    // at boot, haznet is set to 0. so skip this on the first run through
    if(haznet && errno == EINVAL)
    {
        errno = 0;
        PatStay(0x00FFFF); // Notif LED = Yellow
        while(checkwifi()) yield();
    }
    
    if(checkwifi())
    {
    	int r;


    	if(debug_useUDP)
    	{
    		// UDP (May not work!)

        	// For third argument, 0 is fine as there's only one form of datagram service(?)
    		// But also, if IPPROTO_UDP is fine, I may stick with that.

        	r = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    	}
    	else
    	{
    		r = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // TCP (This works; don't change it.)
    	}

        if(r <= 0)
        {
            printf("socket error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        sock = r;
        
        struct sockaddr_in sao;
        sao.sin_family = AF_INET;
        sao.sin_addr.s_addr = gethostid();
        sao.sin_port = htons(port);
        
        if(bind(sock, (struct sockaddr*)&sao, sizeof(sao)) < 0)
        {
            printf("bind error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        //fcntl(socketid, F_SETFL, fcntl(socketid, F_GETFL, 0) | O_NONBLOCK);
        
        if(!debug_useUDP) // TCP-only code block
        {
			if(listen(sock, 1) < 0)
			{
				printf("listen error: (%i) %s\n", errno, strerror(errno));
				hangmacro();
			}
        }
    }
    
    
    reloop:
    
    if(!isold) osSetSpeedupEnable(1);
    
    PatPulse(0xFF40FF);
    if(haznet) PatStay(0xCCFF00); // Notif LED = 100% Green, 75% Blue
    else PatStay(0x00FFFF); // Notif LED = Yellow
    
    while(1)
    {
        hidScanInput();
        kDown = hidKeysDown();
        kHeld = hidKeysHeld();
        kUp = hidKeysUp();
        
        //printf("svcGetSystemTick: %016llX\n", svcGetSystemTick());
        
        // If any buttons are pressed, make the Notif LED pulse red
        // (Annoying and waste of CPU time. -C)
        if(kDown) PatPulse(0x0000FF);

        if(kHeld == (KEY_SELECT | KEY_START)) break;
        
        if(!soc)
        {
            if(!haznet)
            {
                if(checkwifi()) goto netreset;
            }
            else if(pollsock(sock, POLLIN) == POLLIN)
            {
            	// Client
                int cli = accept(sock, (struct sockaddr*)&sai, &sizeof_sai);
                if(cli < 0)
                {
                    printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
                    if(errno == EINVAL) goto netreset;
                    PatPulse(0x0000FF); // Notif LED = Red
                }
                else
                {
                    PatPulse(0x00FF00); // Notif LED = Green
                    soc = new bufsoc(cli, isold ? 0xC000 : 0x70000);
                    k = soc->pack();
                    

                    // Priority:
                    // Range from 0x00 to 0x3F. Lower numbers mean higher priority.
                    //
                    // Processor ID:
                    // -2 = Default (Don't bother using this)
                    // -1 = All CPU cores(?)
                    // 0 = Appcore and 1 = Syscore on Old-3DS
                    // 2 and 3 are allowed on New-3DS (for Base processes)
                    //
                    if(isold)
                    {
                    	// Original values; priority = 0x21, CPU = 1
                        netthread = threadCreate(netfunc, nullptr, 0x2000, 0x21, 1, true);
                    }
                    else
                    {
                    	// Original values; priority = 0x08, CPU = 3
                    	// Setting priority around 0x10 (16) makes it stop slowing down Home Menu and games.
                        netthread = threadCreate(netfunc, nullptr, 0x4000, 0x10, 2, true);
                    }
                    
                    if(!netthread)
                    {
                    	// Notif LED = Blinking Red
                        memset(&pat, 0, sizeof(pat));
                        memset(&pat.r[0], 0xFF, 16);
                        pat.ani = 0x102;
                        PatApply();
                        
                        svcSleepThread(2e9);
                    }
                    
                    //Could above and below if statements be combined? lol -H
                    
                    if(netthread)
                    {
                    	// After threadrunning = 1, we continue
                        while(!threadrunning) yield();
                    }
                    else
                    {
                        delete soc;
                        soc = nullptr;
                        hangmacro();
                    }
                }
            }
            else if(pollsock(sock, POLLERR) == POLLERR)
            {
                printf("POLLERR (%i) %s", errno, strerror(errno));
                goto netreset;
            }
        }
        
        if(netthread && !threadrunning)
        {
            //TODO: Is this code broken? Hasn't yet been changed. -C
            netthread = nullptr;
            goto reloop;
        }
        
        // VRAM Corruption function :)
        if((kHeld & (KEY_ZL | KEY_ZR)) == (KEY_ZL | KEY_ZR))
        {
            u32* ptr = (u32*)0x1F000000;
            int o = 0x00600000 >> 2;
            while(o--) *(ptr++) = rand();
        }
        
        yield();
    }
    
    killswitch:
    
    PatStay(0xFF0000); // Notif LED = Blue
    
    if(netthread)
    {
        threadrunning = 0;
        
        volatile bufsoc** vsoc = (volatile bufsoc**)&soc;
        while(*vsoc) yield(); //pls don't optimize kthx
    }
    
    if(soc) delete soc;
    else close(sock);
    
    puts("Shutting down sockets...");
    SOCU_ShutdownSockets();
    
    socExit();
    
    //gxExit();
    
    gspExit();
    
    acExit();
    
    if(f)
    {
        fflush(f);
        fclose(f);
    }
    
    PatStay(0);
    
    nsExit();
    
    mcuExit();
    
    // new code consideration
    // APT_PrepareToCloseApplication(false);

    return 0;
}
