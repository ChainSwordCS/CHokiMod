#include <3ds.h>
//#include <3ds/services/hid.h> // Maybe not necessary. This may not help at all.
/*
    HorizonM - utility background process for the Horizon operating system
    Copyright (C) 2017 MarcusD (https://github.com/MarcuzD)

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
#include "turbojpeg.h"
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

void initializeThingsWeNeed();
void initializeGraphics();

static u32 gsp_gpu_handle;

const int port = 6464;

//I think this is over-commented -H

void initializeThingsWeNeed()
{
	mcuInit(); // Notif LED
	nsInit();
	aptInit();
	acInit(); // Wifi

	PatStay(0x0000FF); // Notif LED = red

	initializeGraphics();

	return;
}

// Note that we don't check for the possible error that the
// gsp process isn't running. It always is, from boot. -C
void initializeGraphics()
{
	PatStay(0x00FF00);
	Result ret1 = srvGetServiceHandle(&gsp_gpu_handle, "gsp::Gpu");
	if(ret1)
	{
		PatStay(0x0000FF);
	}

	// I don't know if we need GPU rights.
	if(GSPGPU_AcquireRight(0))
	{
		PatStay(0x0000FF);
	}
	return;
}


static int haznet = 0; // Is connected to wifi?
int checkwifi()
{
    haznet = 0;
    u32 wifi = 0;
    // This crashes the system! Why? I don't exactly know yet.
    // Also curiously, hidKeysHeld() doesn't crash.
    // I'm 80% sure this function crashes... Not 100%
    //hidScanInput();

    // Start + Select forcibly return 0
    // There has got to be a better way of doing that.
    if(hidKeysHeld() == (KEY_SELECT | KEY_START))
    {
    	return 0;
    }

    ACU_GetStatus(&wifi);
    if(wifi == 3) // formerly ACU_GetWifiStatus
    {
    	haznet = 1;
    }
    return haznet; // whyy use haznet in the first place
}


int pollSocket(int passed_socket_id, int passed_events, int timeout = 0)
{
    struct pollfd pfds;
    pfds.fd = passed_socket_id;
    pfds.events = passed_events;
    
    if(poll(&pfds, 1, timeout) == 1)
        return pfds.revents & passed_events;
    else
    	return 0;
}

class SocketBuffer
{
public:
    typedef struct
    {
        u32 packet_type_byte : 8;
        u32 size : 24; // The size of a given packet *does* change.
        u8 data[0];
    } PacketStruct;
    
    int socket_id;
    u8* buffer_bytearray_aka_pointer;
    int buffer_size; // The total buffer size doesn't change after boot.
    //int recvsize;
    
    SocketBuffer(int passed_sock, int passed_bufsize)
    {
        buffer_size = passed_bufsize;
        buffer_bytearray_aka_pointer = new u8[passed_bufsize];
        //recvsize = 0;
        socket_id = passed_sock;
    }
    
    ~SocketBuffer() // Destructor
    {
    	// If this SocketBuffer is already null,
    	// don't attempt to delete it again.
    	// (Could this be safely omitted> -C)
        if(!this)
        	return;
        close(socket_id);
        delete[] buffer_bytearray_aka_pointer;
    }
    
    int isAvailable()
    {
    	return pollSocket(socket_id, POLLIN) == POLLIN;
    }
    
    int readbuf(int flags = 0)
    {
        u32 header = 0;
        u32 received_size = 0;
        // Get header byte
        int ret = recv(socket_id, &header, 4, flags);
        if(ret < 0) return -errno;
        if(ret < 4) return -1;
        // Get size byte
        ret = recv(socket_id, &received_size, 4, flags);
        
        // Copy the 4 header bytes to the start of the buffer. And the size
        PacketStruct* packet_in_buffer = getPointerToBufferAsPacketPointer();
        *(u32*)buffer_bytearray_aka_pointer = header;
        packet_in_buffer->size = received_size;

        // Previously had a bug here (:
        //
        // Size is not obtained from anything the incoming packet just said to us.
        // Size was last set by 'passed_bufsize' in the SocketBuffer constructor.
        // (Or last set by an instance of PacketStruct or SocketBuffer idk)
        //
        // So in practice: when we want to read a packet, the size is
        // wrong and we usually end up reading past the data just sent to us.
        // This could result in an error, or just wasted CPU and RAM time.

        int num_reads_remaining = packet_in_buffer->size;
        
        int offs = 8;
        while(num_reads_remaining)
        {
            ret = recv(socket_id, buffer_bytearray_aka_pointer + offs , num_reads_remaining, flags);
            if(ret <= 0) return -errno;
            num_reads_remaining -= ret;
            offs += ret;
        }
        
        //recvsize = offs;
        return offs;
    }
    
    int wribuf(int flags = 0)
    {
        int mustwri = getPointerToBufferAsPacketPointer()->size + 4;
        int offs = 0;
        int ret = 0;
        
        while(mustwri)
        {
            if(mustwri >> 12)
                ret = send(socket_id, buffer_bytearray_aka_pointer + offs , 0x1000, flags);
            else
                ret = send(socket_id, buffer_bytearray_aka_pointer + offs , mustwri, flags);
            if(ret < 0) return -errno;
            mustwri -= ret;
            offs += ret;
        }
        
        return offs;
    }
    
    PacketStruct* getPointerToBufferAsPacketPointer()
    {
        return (PacketStruct*)buffer_bytearray_aka_pointer;
    }
    
    int errformat(char* c, ...)
    {
        PacketStruct* wip_packet = getPointerToBufferAsPacketPointer();
        
        int len = 0;
        
        va_list args;
        va_start(args, c);
        len = vsprintf((char*)wip_packet->data + 1, c, args);
        va_end(args);
        
        if(len < 0)
        {
            puts("out of memory"); //???
            return -1;
        }
        
        //printf("Packet error %i: %s\n", wip_packet->packetid, wip_packet->data + 1);
        
        wip_packet->data[0] = wip_packet->packet_type_byte;
        wip_packet->packet_type_byte = 1;
        wip_packet->size = len + 2;
        
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


extern "C" u32 __get_bytes_per_pixel(GSPGPU_FramebufferFormat format);

static u32 kDown = 0;
static u32 kHeld = 0;
static u32 kUp = 0;

static GSPGPU_CaptureInfo capin;

static int isold = 1;

static Result ret = 0;
//static int cx = 0;
static int cy = 0;

static u32 offs[2] = {0, 0};
static u32 limit[2] = {1, 1};
static u32 stride[2] = {80, 80};
static u32 format[2] = {0xF00FCACE, 0xF00FCACE};

static u8 cfgblk[0x100];

static int sock = 0;

static struct sockaddr_in sai;
static socklen_t sizeof_sai = sizeof(sai);

static SocketBuffer* soc = nullptr;

static SocketBuffer::PacketStruct* k = nullptr;

static Thread netthread = 0;
static vu32 threadrunning = 0;

static void* screenbuf = nullptr;
static u32 screenbuf_address;

static tga_image img;
static tjhandle turbo_jpeg_instance_handle = nullptr;


void netfunc(void* __dummy_arg__)
{
    u32 siz = 0x80;
    u32 bsiz = 1;
    u32 scrw = 1;
    u32 bits = 8;
    
    int scr = 0;
    
    if(isold)
    {
    	// screenbuf = (u32*)k->data;
    }
    else
    {
    	// This instruction *might* crash. Leaving it commented for now is harmless.
    	// It may not even be needed in the first place.
    	//osSetSpeedupEnable(1);
    }
    
    k = soc->getPointerToBufferAsPacketPointer(); //Just In Case (tm)
    
    PatStay(0x00FF00); // Notif LED = Green
    
    format[0] = 0xF00FCACE; //invalidate
    
    u32 procid = 0;
    Handle dmahand = 0;
    
	//u8 dmaconf[0x18];
	DmaConfig dmaconf = {}; // zeroes itself out (:
	dmaconf.channelId = -1; // auto-assign to a free channel (Arm11: 3-7, Arm9: 0-1)
	
    //memset(dmaconf, 0, sizeof(dmaconf));
    //dmaconf[0] = -1; //don't care

	// This was commented out before I got here. -C
    //dmaconf[2] = 4;
    //screenInit();
    
    PatPulse(0x7F007F); // Notif LED = Purple-ish
    threadrunning = 1;
    
    // why???
    do
    {
        k->packet_type_byte = 2; //MODE
        k->size = 4 * 4;
        
        u32* kdata = (u32*)k->data;
        
        kdata[0] = 1;
        kdata[1] = 240 * 3;
        kdata[2] = 1;
        kdata[3] = 240 * 3;
        soc->wribuf();
    }
    while(0);
    
    // This might be malfunctional and might be an infinite loop.
    // Even if it were, I don't know if that's actually *bad*
    // or if it's intended behavior. -C
    while(threadrunning)
    {
        if(soc->isAvailable())
        {
        	// why
			while(1)
			{
				if((kHeld & (KEY_SELECT | KEY_START)) == (KEY_SELECT | KEY_START))
				{
					delete soc;
					soc = nullptr;
					break;
					// By the way, does this break out of
					// both while loops or just one? -C
				}

				puts("reading");
				// Just using variable cy as another "res". why
				cy = soc->readbuf();
				if(cy <= 0)
				{
					// I legitimately don't know where debug output is defined.
					// It might as well not exist to be honest.
					// But I'm also Dumb Lol, so I won't remove these (yet) -C
					printf("Failed to recvbuf: (%i) %s\n", errno, strerror(errno));
					delete soc;
					soc = nullptr;
					break;
				}
				else
				{
					printf("#%i 0x%X | %i\n", k->packet_type_byte, k->size, cy);

					//reread: // unused label IIRC. -C
					int cfg_copy_data_size = 1;
					// Is there any performance benefit to making this
					// a switch-case instead of if-else statements?
					// Genuine question. I could go either way to be honest. -C
					switch(k->packet_type_byte)
					{
						case 0x00: //CONNECT
							// For compatibility, just initialize ourselves anyway...
							// I don't expect to actually ever receive this IIRC
							cfgblk[0] = 1; // Manually enable ourselves.
							break;
						case 0x01: //ERROR
							// I don't know if ChokiStream actually
							// sends any of these. This might be redundant.
							puts("forced dc");
							delete soc;
							soc = nullptr;
							break;

						case 0x7E: //CFGBLK_IN
							//memcpy(cfgblk + k->data[0], &k->data[4], min((u32)(0x100 - k->data[0]), (u32)(k->size - 4)));
							//
							// Refactored this code. Should be less borked.
							//
							// The first byte *of the packet* is the packet-type
							// Bytes 6-8 are the size of the data, in bytes. (24-bit integer)
							// And everything after is perceived as data.
							//
							// The first byte of *data* indicates at what index we copy to.
							// Skip three bytes for no reason... (TODO: Please redo this later)
							// Copy <size> bytes
							//
							cfg_copy_data_size = (u32)(k->size) - 8; // This should probably work

							// Error-checking for if we possibly underflow due to invalid data.
							// Note that it's *possible* to receive 256 bytes and write them to config block
							// But that much was never used in practice.
							// TODO: When I reimplement this, simplify and remove that extra functionality.
							if(cfg_copy_data_size > 200)
								cfg_copy_data_size = 1;

							memcpy(cfgblk + (k->data[0]), &k->data[4], cfg_copy_data_size);

							break;

						default:
							printf("Invalid packet ID: %i\n", k->packet_type_byte);
							//delete soc;
							//soc = nullptr;
							break;
					}

					break;
				}
			}
        }
        
        if(!soc)
        {
        	break;
        }
        
        // If index 0 of config-block is non-zero! (Set by initialization sorta packet)
        // And this ImportDisplayCaptureInfo function doesn't error out...
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
                PatStay(0xFFFF00); // Notif LED = Teal (Green + Blue)
                
                // Already commented out before I got here. -C
                //
                //fbuf[0] = (u8*)capin.screencapture[0].framebuf0_vaddr;
                //fbuf[1] = (u8*)capin.screencapture[1].framebuf0_vaddr;

                format[0] = capin.screencapture[0].format;
                format[1] = capin.screencapture[1].format;
                
                k->packet_type_byte = 2; //MODE
                k->size = 4 * 4;
                
                u32* kdata = (u32*)k->data;
                
                kdata[0] = format[0];
                kdata[1] = capin.screencapture[0].framebuf_widthbytesize;
                kdata[2] = format[1];
                kdata[3] = capin.screencapture[1].framebuf_widthbytesize;
                soc->wribuf();
                
                k->packet_type_byte = 0xFF;
                k->size = sizeof(capin);
                *(GSPGPU_CaptureInfo*)k->data = capin;
                soc->wribuf();
                
                // Yeah. If the framebuffers have changed (places?)
                // Then stop Direct Memory Access there.
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
                        	// &loaded = pRegistered / Pointer to output the registration status to
                        	// loaded = Registration Status(?) of the specified application(?)
                        	// If this function returns negative (s32), then break?
                            if(APT_GetAppletInfo((NS_APPID)0x300, &progid, nullptr, &loaded, nullptr, nullptr) < 0)
                            {
                            	break;
                            }
                            if(loaded)
                            {
                            	break;
                            }
                            
                            svcSleepThread(15e6);
                        }
                        
                        if(!loaded)
                        {
                        	break;
                        }
                        
                        if(NS_LaunchTitle(progid, 0, &procid) >= 0)
                        {
                        	break;
                        }
                    }
                    
                    if(loaded)
                    {
                    	// Commented out before I got here. -C
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
            
            while(--loopcnt)
            {
                if(format[scr] == 0xF00FCACE)
                {
                    scr = !scr;
                    continue;
                }
                
                k->size = 0;
                
                if(dmahand)
                {
                    svcStopDma(dmahand);
                    svcCloseHandle(dmahand);
                    dmahand = 0;

                    // Looks like I broke this, and I just fixed it again hopefully. -C
                    //
                    //if(!isold) svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);
					if(!isold) svcFlushProcessDataCache(0xFFFF8001, (u32)&screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);
                }
                
                int imgsize = 0;
                
                // This is nice and all, but absolutely not.
                // This has to be reorganized.
                // I still like Targa as an option but y'know. An option.
                // And right now ChokiStream doesn't support "TGAHz" yet either. But it will.
                // And I'll bugfix the Targa implementation in this code too. -C

                if((format[scr] & 7) >> 1 || !cfgblk[3])
                {
                    init_tga_image(&img, (u8*)screenbuf, scrw, stride[scr], bits);
                    img.image_type = TGA_IMAGE_TYPE_BGR_RLE;
                    img.origin_y = (scr * 400) + (stride[scr] * offs[scr]);
                    tga_write_to_FILE(k->data, &img, &imgsize);
                    
                    k->packet_type_byte = 3; //DATA (Targa)
                    k->size = imgsize;
                }
                else
                {
                    *(u32*)&k->data[0] = (scr * 400) + (stride[scr] * offs[scr]);
                    u8* dstptr = &k->data[8];
                    // Please make this not all one line. -C
                    if(!tjCompress2(turbo_jpeg_instance_handle, (u8*)screenbuf, scrw, bsiz * scrw, stride[scr], format[scr] ? TJPF_RGB : TJPF_RGBX, &dstptr, (u32*)&imgsize, TJSAMP_420, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT))
                    {
                    	k->size = imgsize + 8;
                    }
                    k->packet_type_byte = 4; //DATA (JPEG)
                }

                // Commented out before I got here. -C

                //k->size += 4;
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, fbuf[0] + fboffs, siz, dmaconf);
                //svcFlushProcessDataCache(prochand ? prochand : 0xFFFF8001, capin.screencapture[0].framebuf0_vaddr, capin.screencapture[0].framebuf_widthbytesize * 400);
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, (u8*)capin.screencapture[0].framebuf0_vaddr + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, 0x600000 + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, dbgo, siz, dmaconf);
                
                if(++offs[scr] == limit[scr])
                {
                	offs[scr] = 0;
                }
                
                scr = !scr;
                
                siz = (capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);
                
                bsiz = capin.screencapture[scr].framebuf_widthbytesize / 240;
                scrw = capin.screencapture[scr].framebuf_widthbytesize / bsiz;
                bits = 4 << bsiz;
                
                if((format[scr] & 7) == 2)
                {
                	bits = 17;
                }
                if((format[scr] & 7) == 4)
                {
                	bits = 18;
                }
                
                Handle prochand = 0;
                if(procid)
                {
                	if(svcOpenProcess(&prochand, procid) < 0)
                	{
                		procid = 0;
                	}
                }
                

                // this is written so badly i wanna cry -C
                if\
                (\
                    svcStartInterProcessDma\
                    (\
                        &dmahand, 0xFFFF8001, (u32)&screenbuf, prochand ? prochand : 0xFFFF8001,\
                        (u32)(capin.screencapture[scr].framebuf0_vaddr + (siz * offs[scr])), siz, &dmaconf\
                    )\
                    < 0 \
                )
                {
                    procid = 0;
                    format[scr] = 0xF00FCACE; //invalidate
                }
                
                if(prochand)
                {
                    svcCloseHandle(prochand);
                    prochand = 0;
                }
                
                if(k->size)
                {
                	soc->wribuf();
                }

                // Commented out before I got here. -C
                /*
                k->packetid = 0xFF;
                k->size = 4;
                *(u32*)k->data = dbgo;
                soc->wribuf();
                
                dbgo += 240 * 3;
                if(dbgo >= 0x600000) dbgo = 0;
                */
                
                // Free up this thread to do other things? (On Old-3DS)
                if(isold)
                {
                	svcSleepThread(5e6);
                }
            }
        }
        else yield();
    }
    
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
    
    // Commented out before I got here. -C
    //
    //if(prochand) svcCloseHandle(prochand);
    //screenExit();
    
    threadrunning = 0;
}

static FILE* file = nullptr;

// I think I got it. "fd" is declared here, but just kept null.
// So of course it doesn't matter what type it is.
// Note to future me and anyone else though,
// Whatever's going on here is really stupid I bet. -C
//
ssize_t stdout_write(struct _reent* r, void* fd, const char* ptr, size_t len) //used to be "int fd" not "void* fd"
{
    if(!file) return 0;
    fputs("[STDOUT] ", file);
    return fwrite(ptr, 1, len, file);
}

ssize_t stderr_write(struct _reent* r, void* fd, const char* ptr, size_t len)
{
    if(!file) return 0;
    fputs("[STDERR] ", file);
    return fwrite(ptr, 1, len, file);
}

                                     //{name, structSize, *open r, *close r, *write r, *read r, *seek r, *fstat_r}
static const devoptab_t devop_stdout = { "stdout", 0, nullptr, nullptr, stdout_write, nullptr, nullptr, nullptr };
static const devoptab_t devop_stderr = { "stderr", 0, nullptr, nullptr, stderr_write, nullptr, nullptr, nullptr };

int main()
{
	initializeThingsWeNeed();

    soc = nullptr;
    
    // This is dumb, we don't even do anything with the file.
    file = fopen("/HzLog.log", "w");
    if((s32)file <= 0)  //Maybe switch condition to (file == NULL)?? -H
		file = nullptr;
    else
    {
        //devoptab_list is from sys/iosupport.h. Idk what it does. -H
        devoptab_list[STD_OUT] = &devop_stdout;
		devoptab_list[STD_ERR] = &devop_stderr;

        //Turn off buffering for stdout and stderr.
		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
    }
    
    memset(&pat, 0, sizeof(pat));
    memset(&capin, 0, sizeof(capin));
    memset(cfgblk, 0, sizeof(cfgblk));
    
    //isold, or is_old, tells us if we are running on Original/"Old" 3DS (reduced clock speed and RAM...)
    if(APPMEMTYPE <= 5) // weird way of doing this.
    {
    	isold = 1;
    }
    
    
    if(isold)
    {
        limit[0] = 8; // Multiply by this to get the full horizontal res of a screen.
        limit[1] = 8; // I assume we're capturing it in chunks. On Old-3DS this makes it look awful.
        stride[0] = 50; // Width of the framebuffer we use(?)
        stride[1] = 40; // On Old-3DS, this is pitiful... But I get it
    }
    else
    {
        limit[0] = 1;
        limit[1] = 1;
        stride[0] = 400; // Width of the framebuffer we use(?)
        stride[1] = 320;
    }

    //acInit(); // Initialize AC service; 3DS's service for connecting to Wifi
    
    // whyyyyyyyy? You don't even do this more than once.
    // This code currently works. So I'm gonna modify it to be readable.
//  do
//  {
//      u32 siz = isold ? 0x10000 : 0x200000; // If Old-3DS,
//      ret = socInit((u32*)memalign(0x1000, siz), siz);
//  }
//  while(0);

    // Web socket stuff...
    // Size of a buffer
    // Is page-aligned (0x1000)
    u32 soc_buffer_size;

    if(isold)
    {
    	soc_buffer_size = 0x10000; // If Old-3DS
    }
    else
    {
    	soc_buffer_size = 0x200000; // If New-3DS
    }

    // Initialize the SOC service.
    // Note: Programs stuck in userland don't have permission to
    // change the buffer address after creation(?)
    // This is probably a non-issue for us.
    ret = socInit((u32*)memalign(0x1000, soc_buffer_size), soc_buffer_size);
    
    if(ret < 0)
    {
    	// The returned value of the socInit function
    	// is written at 0x001000F0 in RAM (for debug).
    	*(u32*)0x1000F0 = ret;
    	//hangmacro();
    }


    turbo_jpeg_instance_handle = tjInitCompress();

    if(!turbo_jpeg_instance_handle) // if tjInitCompress() returned null, an error occurred.
    {
    	// Write a debug error code in RAM (at 0x001000F0)
    	*(u32*)0x1000F0 = 0xDEADDEAD;
    	//hangmacro();
    }

    // As it is right now, this can't be called here.
    // Crashes the system.
    // We should maybe be able to do this...
    // The old code was doing this too...
    //
    //gspInit(); // Initialize GSP GPU Service

    // This function isn't guaranteed to work by any means,
    // but I'm separating it and rewriting it
    // near the top of the file. -C
    //initializeGraphicsInLegacyMain();


    // Note: this function is part of gx.c in this repo.
    // But it's dummied out, apparently. Huh.
    //gxInit();

    if(isold)
    {
        screenbuf = (u32*)memalign(8, 50 * 240 * 4); // On Old-3DS
    }
    else
    {
        screenbuf = (u32*)memalign(8, 400 * 240 * 4); // On New-3DS
    }
    
    if(!screenbuf) // If memalign returns null or 0
    {
        makerave();
        svcSleepThread(2e9);
        //hangmacro();
    }
    
    //why
    // Last night I was tired enough that I didn't even know where to begin with rewriting this.
    if((__excno = setjmp(__exc))) goto killswitch;

    //PatStay(0x007F7F); // Debug. -ChainSwordCS

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
        //PatStay(0x00FFFF);
        while(checkwifi())
        {
        	yield();
        }
    }
    

    //PatStay(0x007F7F); // Debug. -ChainSwordCS

    // at the beginning of boot, does this consistently return 0?
    // (by which i mean, haznet = 0, etc.)
    if(checkwifi()) // execution seems to fail around here
    {
        cy = socket(PF_INET, SOCK_STREAM, IPPROTO_IP); // formerly, "PF-INET" was "AF-INET"
        if(cy <= 0)
        {
            printf("socket error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        sock = cy;
        
        struct sockaddr_in sao;
        sao.sin_family = PF_INET;
        sao.sin_addr.s_addr = gethostid();
        sao.sin_port = htons(port);
        
        if(bind(sock, (struct sockaddr*)&sao, sizeof(sao)) < 0)
        {
            printf("bind error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        //fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
        
        if(listen(sock, 1) < 0)
        {
            printf("listen error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
    }

    
    reloop:
    
	// If not on Old-3DS, then increase the clock speed.
	// Actually, since I specified the higher clock speed in the CIA.RSF,
	// that may be causing this to crash... Not sure though.
	// Also not sure why any of these errors couldn't be handled gracefully
	// but thanks nintendo
    if(!isold)
    {
    	//osSetSpeedupEnable(1);
    }
    
    //PatPulse(0xFF40FF);
    
    if(haznet)
    {
    	//todo - uncomment
    	//PatStay(0xCCFF00); // what color is this? 100% green + 75% blue? lol
    }
    else
    {
    	//todo - uncomment
    	//PatStay(0x00FFFF); // bright yellow, this means no wifi yet?
    }

    // This conditional statement was previously "while(1)"
    // Whyyyyy
    // Are we just taking exclusive control over this CPU thread?
    // Maybe that's fine actually; it may not matter.
    // (this IS ChainSwordCS, I'm notoriously slightly uninformed and/or otherwise dumb&stupid so)
    //
    // But if nothing else, we really should change this.
    // You can keep the infinitely running 'while' loop,
    // But, like, split this into more functions so it's possible to navigate this file.

    while(1)
    {
    	// This hidScanInput() function call might crash.
        //hidScanInput();
        kDown = hidKeysDown();
        kHeld = hidKeysHeld();
        kUp = hidKeysUp();
        
        //printf("svcGetSystemTick: %016llX\n", svcGetSystemTick());
        
        // If any buttons are pressed, make the Notif LED pulse red?
        // Pure waste of CPU time for literally no reason
        // Also it's annoying -C
        //if(kDown) PatPulse(0x0000FF);

        if(kHeld == (KEY_SELECT | KEY_START)) break;
        
        if(!soc)
        {
            if(!haznet)
            {
                if(checkwifi()) goto netreset;
            }
            else if(pollSocket(sock, POLLIN) == POLLIN)
            {
                int cli = accept(sock, (struct sockaddr*)&sai, &sizeof_sai);
                if(cli < 0)
                {
                    printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
                    if(errno == EINVAL) goto netreset;
                    PatPulse(0x0000FF);
                }
                else
                {
                    PatPulse(0x00FF00);
                    soc = new SocketBuffer(cli, isold ? 0xC000 : 0x70000);
                    k = soc->getPointerToBufferAsPacketPointer();
                    
                    if(isold)
                    {
                        netthread = threadCreate(netfunc, nullptr, 0x2000, 0x21, 1, true);
                    }
                    else
                    {
                        netthread = threadCreate(netfunc, nullptr, 0x4000, 8, 3, true);
                    }
                    
                    if(!netthread)
                    {
                        memset(&pat, 0, sizeof(pat));
                        memset(&pat.r[0], 0xFF, 16);
                        pat.ani = 0x102;
                        PatApply();
                        
                        svcSleepThread(2e9);
                    }
                    
                    
                    if(netthread)
                    {
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
            else if(pollSocket(sock, POLLERR) == POLLERR)
            {
                printf("POLLERR (%i) %s", errno, strerror(errno));
                goto netreset;
            }
        }
        
        if(netthread && !threadrunning)
        {
            //TODO todo?
            netthread = nullptr;
            goto reloop;
        }
        
        if((kHeld & (KEY_ZL | KEY_ZR)) == (KEY_ZL | KEY_ZR))
        {
            u32* ptr = (u32*)0x1F000000;
            int o = 0x00600000 >> 2;
            while(o--) *(ptr++) = rand();
        }
        
        yield();
    }
    
    killswitch:
    
    PatStay(0xFF0000); // If we ever actually reach killswitch, make the Notif LED blue
    
    if(netthread)
    {
        threadrunning = 0;
        
        volatile SocketBuffer** vsoc = (volatile SocketBuffer**)&soc;
        // Note from ChainSwordCS: I didn't write that comment. lol.
        // But I'd make a note of it and also ask why
        while(*vsoc) yield(); //pls don't optimize kthx
    }
    
    if(soc) delete soc;
    else close(sock);
    
    puts("Shutting down sockets...");
    SOCU_ShutdownSockets();
    
    socExit();
    
    //gxExit();
    
    // With the current state of the code, we never init the GSP service...
    //gspExit();
    
    acExit();
    
    if(file)
    {
        fflush(file);
        fclose(file);
    }
    
    // Why was I thinking of commenting this out again?
    // Maybe just misc debug testing...
    //
    // Commenting this line out *will* break things.
    // Nintendo didn't put much care or error-checking with the Notif LED.
    // (For example, its state at boot is undefined IIRC)
    // This sets the Notif LED to be off, basically.
    // For when we're about to stop execution.
    PatStay(0);
    
    nsExit(); // Don't change
    
    mcuExit(); // Probably don't change
    
    return 0;
}
