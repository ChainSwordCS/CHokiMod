#include <3ds.h>

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
#include "service/hid.h"
#include "misc/pattern.h"
#include "misc/yuvconvert.h"

#include "tga/targa.h"
#include <turbojpeg.h>
}

#include <exception>

#include "utils.hpp"



#define yield() svcSleepThread(2e9)

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
        hidScanInputDirectIO();\
        if(getkHeld() == (KEY_SELECT | KEY_START))\
        {\
        	fprintf(stderr, "[dbg] STARTSELECT signaled, exiting hangmacro()\n");\
            goto killswitch;\
        }\
        yield();\
    }\
}

static int haznet = 0;
int checkwifi()
{
    haznet = 0;
    u32 wifi = 0;
    hidScanInputDirectIO();
    if(getkHeld() == (KEY_SELECT | KEY_START)) return 0;
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

class bufsoc
{
public:
    
    typedef struct
    {
        u32 packetid : 8;
        u32 size : 24;
        u8 data[0];
    } packet;
    
    int sock;
    u8* buf;
    int bufsize;
    int recvsize;
    
    bufsoc(int sock, int bufsize)
    {
        this->bufsize = bufsize;
        buf = new u8[bufsize];
        
        recvsize = 0;
        this->sock = sock;
    }
    
    ~bufsoc()
    {
        if(!this) return;
        close(sock);
        delete[] buf;
    }
    
    int avail()
    {
        return pollsock(sock, POLLIN) == POLLIN;
    }
    
    int readbuf(int flags = 0)
    {
        u32 hdr = 0;
        int ret = recv(sock, &hdr, 4, flags);
        if(ret < 0) return -errno;
        if(ret < 4) return -1;
        printf("readbuf hdr %08X\n", hdr);
        *(u32*)buf = hdr;
        
        packet* p = pack();
        
        int mustwri = p->size;
        int offs = 4;
        while(mustwri)
        {
            ret = recv(sock, buf + offs , mustwri, flags);
            if(ret <= 0) return -errno;
            mustwri -= ret;
            offs += ret;
        }
        
        recvsize = offs;
        return offs;
    }
    
    int wribuf_old(int flags = 0)
    {
        int mustwri = pack()->size + 4;
        int offs = 0;
        int ret = 0;
        while(mustwri)
        {
            ret = send(sock, buf + offs , mustwri, flags);
            if(ret < 0) return -errno;
            mustwri -= ret;
            offs += ret;
        }
        
        return offs;
    }
    
    int wribuf(int flags = 0)
    {
        int mustwri = pack()->size + 4;
        int offs = 0;
        int ret = 0;
        
        while(mustwri)
        {
            if(mustwri >> 12)
                ret = send(sock, buf + offs , 0x1000, flags);
            else
                ret = send(sock, buf + offs , mustwri, flags);
            if(ret < 0) return -errno;
            mustwri -= ret;
            offs += ret;
        }
        
        return offs;
    }
    
    packet* pack()
    {
        return (packet*)buf;
    }
    
    /**
     * v.2020 decomp todo:
     * "warning: ISO C++ forbids converting a string constant to 'char*' [-Wwrite-strings]"
     * lol. lmao
     */
    int errformat(char* c, ...)
    {
        packet* p = pack();
        
        int len = 0;
        
        va_list args;
        va_start(args, c);
        len = vsprintf((char*)p->data + 4, c, args);
        va_end(args);
        
        if(len < 0)
        {
            puts("errformat: out of memory");
            return -1;
        }
        
        printf("Packet error 0x%08X: %s\n", p->packetid|(p->size<<8), p->data + 4);
        
        p->data[0] = p->packetid; // ?
        p->packetid = 1;
        p->size = len + 6;
        
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

static GSPGPU_CaptureInfo capin;

static int isold = 1;

static Result ret = 0;
static int cx = 0;
static int cy = 0;

static u32 offs[2] = {0, 0};
static u32 limit[2] = {1, 1};
static u32 stride[2] = {80, 80};
static u32 format[2] = {0xF00FCACE, 0xF00FCACE};

static u8 cfgblk[0x100];

static int sock = 0;

static struct sockaddr_in sai;
static socklen_t sizeof_sai = sizeof(sai);

static bufsoc* soc = nullptr;

static bufsoc::packet* k = nullptr;

static u32 bufsocsiz = 0;

static Thread netthread = 0;
static vu32 threadrunning = 0;
static u32 netthread_unkvar; // v.2020 decomp todo

static u32* screenbuf = nullptr;

static tga_image img;
static tjhandle jencode = nullptr;

/**
 * v.2020 decomp todo
 */
void FUN_00104d5c()
{
    // v.2020 decomp todo: is this correct?
    delete k; //FUN_00127f0c(k);
    k = nullptr;
    //close(sock); // speculation
    //close(soc->sock); // implied by "delete soc;"
    delete soc;
    soc = nullptr;
}

int ThreadJoin(Thread threadhandle)
{
    int r = 0;
    if(threadhandle)
    {
        // v.2020 decomp todo
        //if(*(u8*)(threadhandle + 0x11) == 0)
        if(netthread_unkvar == 0)
        {
            // is this right?
            s64 timeout = 0;
            //s64 timeout = -1;
            svcWaitSynchronization((u32)threadhandle, timeout);
        }
    }
    return r;
}

/**
 * v.2020 decomp todo:
 * - code cleanup
 * - does this even work? included here for completeness anyway.
 */
int dmaRemoteWrite(u32 dmahandle, u32 dstaddr, u32 handle2, u32 srcaddr, u32 size)
{
    int r;
    u32 thisdmahandle = 0;
    u8 thisdmaconf[0x18];
    memset(thisdmaconf,0,0x18);
    thisdmaconf[0] = -1;

    r = svcFlushProcessDataCache(dmahandle,(void*)dstaddr,size);
    if(r < 0) return r;
    r = svcFlushProcessDataCache(handle2,(void*)srcaddr,size);
    if(r < 0) return r;
    r = svcStartInterProcessDma(&thisdmahandle, dmahandle, (void*)dstaddr, handle2, (void*)srcaddr, size, thisdmaconf);
    if(r < 0) return r;
    u32 thisdmastate = 0;
    while(true)
    {
        svcGetDmaState(&thisdmastate, thisdmahandle);
        if(thisdmastate & 0xfff00000) break;
        svcSleepThread(20000);
    }
    svcStopDma(thisdmahandle);
    svcCloseHandle(thisdmahandle);
    svcInvalidateProcessDataCache(dmahandle,(void*)dstaddr,size);
    return 0;
}

void netfunc(void* __dummy_arg__)
{
    u32 siz = 0x80;
    u32 bsiz = 1;
    u32 scrw = 1;
    u32 bits = 8;
    
    int scr = 0;
    
    if(isold);// screenbuf = (u32*)k->data;
    else osSetSpeedupEnable(1);
    
    /**
     * see: https://www.3dbrew.org/wiki/SVC#KernelSetState
     * Type 10 = ConfigureNew3DSCPU
     * enable L2 Cache and 804MHz CPU clock speed
     */
    svcKernelSetState(10,0b11);
    //svcKernelSetState(10,3,0);
    
    PatStay(0xFF00);
    
    format[0] = 0xFFFFFFFF; //invalidate
    
    u32 procid = 0;
    Handle dmahand = 0;
    u8 dmaconf[0x18];
    memset(dmaconf, 0, sizeof(dmaconf));
    dmaconf[0] = -1; //don't care
    //dmaconf[2] = 4;
    
    //screenInit();
    
    PatPulse(0x7F007F);
    threadrunning = 1;
    
    while(threadrunning)
    {
        if((kHeld & (KEY_SELECT | KEY_START)) == (KEY_SELECT | KEY_START))
        {
            puts("Thread kill by STARTSELECT");
            break; // equivalent to "goto threadkill;"
        }

        if(soc->avail())
        while(1)
        {
            cy = soc->readbuf();
            if(cy <= 0)
            {
                printf("Failed to recvbuf: (%i) %s\n", errno, strerror(errno));
                goto threadkill;
            }
            else
            {
                reread:
                switch(k->packetid)
                {
                    case 0x00: //CONNECT
                    case 0x01: //ERROR
                        puts("forced dc");
                        goto threadkill;
                        //break; // implied
                        
                    case 0x7E: //CFGBLK_IN
                        memcpy(cfgblk + k->data[0], &k->data[4], (u32)(k->size - 4));
                        break;
                        
                    case 0x80: //NYI (Not Yet Implemented) (from v.2020 decomp)
                        soc->errformat("NYI");
                        break;

                    case 0x81: //DMA_IN (from v.2020 decomp)
                        // v.2020 decomp todo: does this work?
                        if(k->size < 9)
                            soc->errformat("Invalid packet size 0x%X for DMA_IN", k->size);
                        else
                        {
                            // v.2020 decomp todo: cleanup
                            u32* kdata = (u32*)k->data;
                            u32 processid = kdata[0];
                            u32 addr = kdata[1];
                            u32 dmainhandle = 0;
                            int r = 0;
                            if ((int)processid == -1)
                              r = svcDuplicateHandle(&dmainhandle,0xffff8001);
                            else
                              r = svcOpenProcess(&dmainhandle, processid);
                            if (r < 0) {
                              soc->errformat("Failed to open process 0x%X: %08X", processid, r);
                              dmainhandle = 0;
                            }
                            else {
                                r = svcControlProcessMemory(dmainhandle, addr&0xfffff000, addr&0xfffff000, (k->size+0xff7)&0xfffff000, 6, 7);
                                if (r < 0) {
                                    svcCloseHandle(dmainhandle);
                                    dmainhandle = 0;
                                    soc->errformat("Failed to change memory perms for address 0x%08X: %08X", addr, r);
                                }
                                else {
                                    r = dmaRemoteWrite(dmainhandle, addr, 0xffff8001, (u32)k->data+8, k->size-8);
                                    svcCloseHandle(dmainhandle);
                                    dmainhandle = 0;
                                    if(r < 0)
                                        soc->errformat("Failed to DMA to address 0x%08X: %08X",addr,r);
                                }
                            }
                        }
                        break;

                    default:
                        u32 hdr = k->packetid | (k->size << 8);
                        soc->errformat("Invalid header: %08X\n", hdr);
                        goto threadkill;
                        //break; // implied
                }
                
                break;
            }
        }
        
        if(!soc)
        {
            puts("Break thread due to lack of soc");
            //equivalent to "goto threadkill;"
            break;
        }
        
        if(cfgblk[0] && GSPGPU_ImportDisplayCaptureInfo(&capin) >= 0)
        {
            //test for changed framebuffers
            if\
            (\
                (capin.screencapture[0].format & 0xFFFFFF9F) != (format[0] & 0xFFFFFF9F)\
                ||\
                (capin.screencapture[1].format & 0xFFFFFF9F) != (format[1] & 0xFFFFFF9F)\
            )
            {
                PatStay(0xFFFF00);
                
                //fbuf[0] = (u8*)capin.screencapture[0].framebuf0_vaddr;
                //fbuf[1] = (u8*)capin.screencapture[1].framebuf0_vaddr;
                format[0] = capin.screencapture[0].format;
                format[1] = capin.screencapture[1].format;
                
                k->packetid = 2; //MODE
                k->size = 4 * 4;
                
                u32* kdata = (u32*)k->data;
                
                // if TARGA, or top screen format is RGB8 (24bpp)
                if((cfgblk[3] == 0) || ((format[0] & 7) == 1))
                {
                    kdata[0] = format[0];
                    kdata[1] = capin.screencapture[0].framebuf_widthbytesize;
                    kdata[2] = format[1];
                    kdata[3] = capin.screencapture[1].framebuf_widthbytesize;
                }
                else
                {
                    kdata[0] = format[0] & 0xFFFFFFF8;
                    kdata[1] = 960;
                    kdata[2] = format[1] & 0xFFFFFFF8;
                    kdata[3] = 960;
                }
                soc->wribuf();
                
                k->packetid = 0xFF;
                k->size = sizeof(capin);
                *(GSPGPU_CaptureInfo*)k->data = capin;
                soc->wribuf();
                
                procid = 0;
                
                
                //test for VRAM
                if\
                (\
                    (u32)capin.screencapture[0].framebuf0_vaddr <  0x1F600000\
                )
                {
                    //nothing to do?
                    if(dmahand)
                    {
                        svcStopDma(dmahand);
                        svcCloseHandle(dmahand);
                        dmahand = 0;
                    }
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
                    bool loaded = false; // misnomer; actually "isRegistered"
                    u8 mediatype = 0;
                    
                    while(1)
                    {
                        loaded = false;
                        while(1)
                        {
                            NS_APPID appid;
                            if(APT_GetAppletManInfo(APTPOS_NONE, nullptr, nullptr, nullptr, &appid) < 0) break;
                            appid = (NS_APPID)(appid & 0xFFFF);
                            if(APT_GetAppletInfo(appid, &progid, &mediatype, &loaded, nullptr, nullptr) < 0) break;
                            if(loaded) break;
                            
                            svcSleepThread(15e6);
                        }
                        
                        if(!loaded) break;
                        
                        if(mediatype == 2) progid = 0; // Game Card

                        if(NS_LaunchTitle(progid, 0, &procid) >= 0) break;
                    }
                    
                    if(loaded);// svcOpenProcess(&prochand, procid);
                    else format[0] = 0xFFFFFFFF; //invalidate
                }
                
                PatStay(0xFF00);
            }
            
            do
            {
                if(cfgblk[0] == 0) break;

                if(format[scr] == 0xFFFFFFFF)
                {
                    soc->errformat("Screen %i FOOFCACE",scr);
                    printf("Screen #%i is F00FCACE\n",scr);
                    break;
                }
                
                siz = (capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);
                bsiz = capin.screencapture[scr].framebuf_widthbytesize / 240;
                scrw = capin.screencapture[scr].framebuf_widthbytesize / bsiz;

                bits = 4 << (bsiz & 0xFF);
                if((format[scr] & 7) == 2) bits = 17;
                if((format[scr] & 7) == 4) bits = 18;

                k->packetid = 0xFF;
                k->size = 0;
                
                if(dmahand)
                {
                    svcStopDma(dmahand);
                    svcCloseHandle(dmahand);
                    dmahand = 0;
                }
                
                if(!isold) svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);

                int imgsize = 0;
                
                if(cfgblk[3] == 0)
                {
                    init_tga_image(&img, (u8*)screenbuf, (uint16_t)scrw, (uint16_t)stride[scr], (u8)bits);
                    img.image_type = TGA_IMAGE_TYPE_BGR_RLE;
                    img.origin_y = (scr * 400) + (stride[scr] * offs[scr]);
                    imgsize = bufsocsiz;
                    tga_write_to_FILE(k->data, &img, &imgsize);
                    
                    k->packetid = 3; //DATA (Targa)
                    k->size = imgsize;
                }
                else
                {
                    int r = 0;
                    *(u32*)&k->data[0] = (scr * 400) + (stride[scr] * offs[scr]);
                    u8* dstptr = &k->data[8];
                    if((format[scr] & 7) == 1) // RGB8 (24bpp)
                    {
                        r = tjCompress2(jencode, (u8*)screenbuf, scrw, bsiz * scrw, stride[scr], TJPF_RGB, &dstptr, (u32*)&imgsize, TJSAMP_420, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT);
                    }
                    else
                    {
                        /** v.2020 decomp todo
                         *
                         * ???????????
                         * how old is that version of libjpeg-turbo?
                         * because i don't think this will work,
                         * not with the 2017-ish version i thought HzMod would be using.
                         * (setting aside known issues with HzMod v.2020)
                         */
                        int unkVar;
                        if((format[scr] & 7) == 2) // RGB565 (16bpp)
                        {
                            yuvConvertFromRGB565(screenbuf, 240, scrw-240, stride[scr]);
                            unkVar = stride[0] << 1;
                        }
                        else // RGB5A1 (16bpp), RGBA4 (16bpp), RGBA8 (32bpp)
                        {
                            // TODO: Known bug with RGB5A1 and RGBA8. Behavior unchanged for posterity.
                            yuvConvertFromRGBA4(screenbuf, 240, scrw-240, stride[scr]);
                            unkVar = stride[0] << 2;
                        }
                        int yuvstrides[3];
                        yuvstrides[0] = 120;
                        yuvstrides[1] = 120;
                        yuvstrides[2] = 240;

                        u8* yuvplanes[3];
                        yuvplanes[0] = (u8*)(scrw * 1 + screenbuf);
                        yuvplanes[1] = (u8*)(yuvplanes[0] + stride[0] * 30);
                        yuvplanes[2] = (u8*)screenbuf;

                        // I assume it's supposed to be the former, but the decomp says it's 3 not 2.
                        TJSAMP subsamp = TJSAMP_420;
                        //TJSAMP subsamp = TJSAMP_GRAY;

                        *(u32*)&k->data[0] = stride[0] * offs[0];
                        u8* dstptr = &k->data[8];
                        r = tjCompressFromYUVPlanes(jencode, yuvplanes, 240, yuvstrides, stride[scr], subsamp, &dstptr, (u32*)&imgsize, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT);

                    }

                    if(r == 0)
                    {
                        k->size = imgsize + 8;
                        k->packetid = 4; //DATA (JPEG)
                    }
                }
                //k->size += 4;
                
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, fbuf[0] + fboffs, siz, dmaconf);
                //svcFlushProcessDataCache(prochand ? prochand : 0xFFFF8001, capin.screencapture[0].framebuf0_vaddr, capin.screencapture[0].framebuf_widthbytesize * 400);
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, (u8*)capin.screencapture[0].framebuf0_vaddr + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, 0x600000 + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, dbgo, siz, dmaconf);
                
                if(++offs[scr] == limit[scr])
                {
                    offs[scr] = 0;
                    //scr = !scr;
                    if(isold) svcSleepThread(1000000);
                }
                
                Handle prochand = 0;
                if((procid) && (svcOpenProcess(&prochand, procid) < 0))
                {
                    printf("Failed to open process %i\n",procid);
                    procid = 0;
                }
                
                if(capin.screencapture[0].framebuf0_vaddr != 0)
                {
                    int r = 0;
                    do
                    {
                        r = svcStartInterProcessDma\
                            (\
                                &dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001,\
                                (u8*)capin.screencapture[scr].framebuf0_vaddr + (siz * offs[scr]), siz, dmaconf\
                            );
                        if(r > -1)
                            break;

                        printf("DMA fail %08X w/ handle %08X, screen #%i\n",r,prochand,scr);
                        procid = 0;
                        format[scr] = 0xFFFFFFFF; //invalidate
                    } while(true); // is this right? and does this actually work?
                }
                
                if(prochand)
                {
                    svcCloseHandle(prochand);
                    prochand = 0;
                }
                
                if(k->size) soc->wribuf();
                /*
                k->packetid = 0xFF;
                k->size = 4;
                *(u32*)k->data = dbgo;
                soc->wribuf();
                
                dbgo += 240 * 3;
                if(dbgo >= 0x600000) dbgo = 0;
                */
                
                //if(isold) svcSleepThread(5e6);
            } while(offs[scr] != 0);
        }
        if(cfgblk[255] != 0)
        {
            k->packetid = 0xFF;
            k->size = 0;
            soc->wribuf();
            if(cfgblk[255] == 0xFF)
            {
                /**
                 * v.2020 decomp todo: not (yet?) implemented
                 * this seems to be a libctru function,
                 * and probably something gsp and/or GPU
                 * (so, something GPU-related).
                 */
                //FUN_00123ff0(2,1);
            }
            else
            {
                /**
                 * v.2020 decomp todo: not (yet?) implemented
                 * unsure what any of this is.
                 */
                //u32 uVar9 = VectorSignedToFloat((uint)cfgblk[255],(byte)(in_fpscr >> 0x15) & 3);
                //coprocessor_function(0xb,2,0,in_cr7,in_cr7,in_cr8);
                //u32 slp = FUN_00103a50((int)uVar9,(int)((ulonglong)uVar9 >> 0x20));
                //svcSleepThread(slp);
            }
        }
    }
    
    threadkill:

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
        soc->errformat("Thread killed");
    }
    
    if(dmahand)
    {
        puts("Stopping DMA");
        svcStopDma(dmahand);
        svcCloseHandle(dmahand);
    }
    
    //if(prochand) svcCloseHandle(prochand);
    //screenExit();
    
    puts("Thread stopping...");
    threadrunning = 0;
}

static FILE* f = nullptr;

ssize_t stdout_write(struct _reent* r, int fd, const char* ptr, size_t len)
{
    if(!f) return 0;
    fputs("[STDOUT] ", f);
    return fwrite(ptr, 1, len, f);
}

ssize_t stderr_write(struct _reent* r, int fd, const char* ptr, size_t len)
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
    
    soc = nullptr;
    
    f = fopen("/HzLog.log", "w");
    if(f <= 0) f = nullptr;
    else
    {
        devoptab_list[STD_OUT] = &devop_stdout;
		devoptab_list[STD_ERR] = &devop_stderr;

		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
    }
    
    memset(&pat, 0, sizeof(pat));
    memset(&capin, 0, sizeof(capin));
    memset(cfgblk, 0, sizeof(cfgblk));
    
    isold = APPMEMTYPE <= 5;
    
    const char* str3ds;
    if(isold)
        str3ds = "an old3DS";
    else
        str3ds = "a new3DS";
    printf("APPMEMTYPE is %i, so we must be on %s\n",APPMEMTYPE,str3ds);
    
    if(isold)
    {
        limit[0] = 25;
        limit[1] = 20;
        stride[0] = 16;
        stride[1] = 16;
    }
    else
    {
        limit[0] = 1;
        limit[1] = 1;
        stride[0] = 400;
        stride[1] = 320;
    }
    
    
    PatStay(0xFF);
    
    acInit();
    
    do
    {
        u32 siz = 0x10000;
        ret = socInit((u32*)memalign(0x1000, siz), siz);
    }
    while(0);
    if(ret < 0) *(u32*)0x1000F0 = ret;//hangmacro();
    
    jencode = tjInitCompress();
    if(!jencode) *(u32*)0x1000F0 = 0xDEADDEAD;//hangmacro();
    
    gspInit();
    
    //gxInit();
    
    if(isold)
        screenbuf = (u32*)memalign(8, 50 * 240 * 4);
    else
        screenbuf = (u32*)memalign(8, 400 * 240 * 4);
    
    if(!screenbuf)
    {
        makerave();
        svcSleepThread(2e9);
        if(f) fflush(f);
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
    
    if(haznet && errno == EINVAL)
    {
        puts("Waiting for wifi to die");
        errno = 0;
        PatStay(0xFFFF);
        while(checkwifi()) yield();
    }
    
    if(checkwifi())
    {
        cy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if(cy <= 0)
        {
            fprintf(stderr, "socket error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        sock = cy;
        
        struct sockaddr_in sao;
        sao.sin_family = AF_INET;
        sao.sin_addr.s_addr = gethostid();
        sao.sin_port = htons(port);
        
        if(bind(sock, (struct sockaddr*)&sao, sizeof(sao)) < 0)
        {
            fprintf(stderr, "bind error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        //fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
        
        if(listen(sock, 1) < 0)
        {
            fprintf(stderr, "listen error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
    }
    
    
    reloop:
    
    if(!isold) osSetSpeedupEnable(1);
    
    PatPulse(0xFF40FF);
    if(haznet) PatStay(0xCCFF00);
    else PatStay(0xFFFF);
    
    while(1)
    {
        hidScanInputDirectIO();
        kDown = getkDown();
        kHeld = getkHeld();
        
        //printf("svcGetSystemTick: %016llX\n", svcGetSystemTick());
        
        if(kDown) PatPulse(0xFF);
        if((kHeld & (KEY_SELECT|KEY_START)) == (KEY_SELECT | KEY_START))
        {
            fprintf(stderr, "STARTSELECT signaled, exiting main()\n");
            break; //hangmacro();
        }
        
		// see: https://www.3dbrew.org/wiki/NSS:TerminateTitle
		// 00040130-00002A02 is NP Services (mp:u)
        NS_TerminateProcessTID(0x0004013000002A02);

        if(!soc)
        {
            if(!haznet)
            {
                if(checkwifi()) goto netreset;
            }
            else
            {
                if(netthread)
                {
                    if(threadrunning == 0)
                    {
                        puts("Clearning up remnants");
                    }
                    else
                    {
                        puts("Waiting for network thread to die");
                        printf("ThreadJoin %08X\n", ThreadJoin(netthread));
                    }
                    threadrunning = 0;
                    netthread = nullptr;
                    if(errno)
                    {
                        printf("Netreset (%i): %s\n",errno,strerror(errno));
                        puts("Relooping netreset");
                        goto netreset;
                    }
                    else
                    {
                        puts("Relooping");
                        goto reloop;
                    }
                }
                if(pollsock(sock, POLLIN) == POLLIN)
                {
                    int cli = accept(sock, (struct sockaddr*)&sai, &sizeof_sai);
                    if(cli < 0)
                    {
                        PatPulse(0xFF);
                        printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
                        if(errno == EINVAL) goto netreset;
                    }
                    else
                    {
                        PatPulse(0xFF00);
                        soc = new bufsoc(cli, isold ? 0xC000 : 0x70000);
                        k = soc->pack();
                        bufsocsiz = isold ? (0xC000 - 4) : (0x70000 - 4);

                        if(isold)
                        {
                            netthread = threadCreate(netfunc, nullptr, 0x2000, 0x14, 1, true);
                        }
                        else
                        {
                            netthread = threadCreate(netfunc, nullptr, 0x4000, 0x10, 3, true);
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
                            puts("Waiting for netthread to start");
                            while(!threadrunning) yield();
                            puts("Netthread started"); //puts("Netthread tarted");
                        }
                        else
                        {
                            FUN_00104d5c();
                            hangmacro();
                        }
                    }
                }
            }
        }
        
        if(netthread && !threadrunning)
        {
            puts("Fixing up after thread death");
            if(soc)
            {
                puts("Killing orphaned socket connection");
                FUN_00104d5c();
            }
            netthread = nullptr;
            goto netreset;
        }
        
        // non-functional; we don't interface with ir in v.2020
        if((kHeld & (KEY_ZL | KEY_ZR)) == (KEY_ZL | KEY_ZR))
        {
            u32* ptr = (u32*)0x1F000000;
            int o = 0x00600000 >> 2;
            while(o--) *(ptr++) = rand();
        }
        
        yield();
    }
    
    killswitch:
    
    PatStay(0xFF0000);
    
    if(f) fflush(f);

    if(netthread)
    {
        threadrunning = 0;
        
        puts("Waiting for thread kill");
        int r = ThreadJoin(netthread);
        printf("ThreadJoin %08X\n", r);
        puts("assuming thread-kill");
    }
    
    if(soc)
    {
        if(soc->sock != sock) // ?
            close(sock);
        FUN_00104d5c();
        //delete soc;
    }
    else
    {
        close(sock);
    }
    
    puts("Closing leftover sockets...");
    SOCU_CloseSockets();
    puts("Shutting down sockets...");
    SOCU_ShutdownSockets();
    
    puts("socExit");
    socExit();
    
    //gxExit();
    
    gspExit();
    
    acExit();
    
    if(f)
    {
        fflush(f);
        fclose(f);
        f = nullptr;
    }
    
    PatStay(0);
    
    nsExit();
    
    mcuExit();
    
    return 0;
}
