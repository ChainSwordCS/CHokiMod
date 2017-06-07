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
    
    int errformat(char* c, ...)
    {
        packet* p = pack();
        
        int len = 0;
        
        va_list args;
        va_start(args, c);
        len = vsprintf((char*)p->data + 1, c, args);
        va_end(args);
        
        if(len < 0)
        {
            puts("out of memory"); //???
            return -1;
        }
        
        //printf("Packet error %i: %s\n", p->packetid, p->data + 1);
        
        p->data[0] = p->packetid;
        p->packetid = 1;
        p->size = len + 2;
        
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

static Thread netthread = 0;
static vu32 threadrunning = 0;

static u32* screenbuf = nullptr;

static tga_image img;
static tjhandle jencode = nullptr;


void netfunc(void* __dummy_arg__)
{
    u32 siz = 0x80;
    u32 bsiz = 1;
    u32 scrw = 1;
    u32 bits = 8;
    
    int scr = 0;
    
    if(isold);// screenbuf = (u32*)k->data;
    else osSetSpeedupEnable(1);
    
    k = soc->pack(); //Just In Case (tm)
    
    PatStay(0xFF00);
    
    format[0] = 0xF00FCACE; //invalidate
    
    u32 procid = 0;
    Handle dmahand = 0;
    u8 dmaconf[0x18];
    memset(dmaconf, 0, sizeof(dmaconf));
    dmaconf[0] = -1; //don't care
    //dmaconf[2] = 4;
    
    //screenInit();
    
    PatPulse(0x7F007F);
    threadrunning = 1;
    
    do
    {
        k->packetid = 2; //MODE
        k->size = 4 * 4;
        
        u32* kdata = (u32*)k->data;
        
        kdata[0] = 1;
        kdata[1] = 240 * 3;
        kdata[2] = 1;
        kdata[3] = 240 * 3;
        soc->wribuf();
    }
    while(0);
    
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
            
            puts("reading");
            cy = soc->readbuf();
            if(cy <= 0)
            {
                printf("Failed to recvbuf: (%i) %s\n", errno, strerror(errno));
                delete soc;
                soc = nullptr;
                break;
            }
            else
            {
                printf("#%i 0x%X | %i\n", k->packetid, k->size, cy);
                
                reread:
                switch(k->packetid)
                {
                    case 0x00: //CONNECT
                    case 0x01: //ERROR
                        puts("forced dc");
                        delete soc;
                        soc = nullptr;
                        break;
                        
                    case 0x7E: //CFGBLK_IN
                        memcpy(cfgblk + k->data[0], &k->data[4], min((u32)(0x100 - k->data[0]), (u32)(k->size - 4)));
                        break;
                        
                    default:
                        printf("Invalid packet ID: %i\n", k->packetid);
                        delete soc;
                        soc = nullptr;
                        break;
                }
                
                break;
            }
        }
        
        if(!soc) break;
        
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
                PatStay(0xFFFF00);
                
                //fbuf[0] = (u8*)capin.screencapture[0].framebuf0_vaddr;
                //fbuf[1] = (u8*)capin.screencapture[1].framebuf0_vaddr;
                format[0] = capin.screencapture[0].format;
                format[1] = capin.screencapture[1].format;
                
                k->packetid = 2; //MODE
                k->size = 4 * 4;
                
                u32* kdata = (u32*)k->data;
                
                kdata[0] = format[0];
                kdata[1] = capin.screencapture[0].framebuf_widthbytesize;
                kdata[2] = format[1];
                kdata[3] = capin.screencapture[1].framebuf_widthbytesize;
                soc->wribuf();
                
                k->packetid = 0xFF;
                k->size = sizeof(capin);
                *(GSPGPU_CaptureInfo*)k->data = capin;
                soc->wribuf();
                
                
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
                            if(APT_GetAppletInfo((NS_APPID)0x300, &progid, nullptr, &loaded, nullptr, nullptr) < 0) break;
                            if(loaded) break;
                            
                            svcSleepThread(15e6);
                        }
                        
                        if(!loaded) break;
                        
                        if(NS_LaunchTitle(progid, 0, &procid) >= 0) break;
                    }
                    
                    if(loaded);// svcOpenProcess(&prochand, procid);
                    else format[0] = 0xF00FCACE; //invalidate
                }
                
                PatStay(0xFF00);
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
                    if(!isold) svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);
                }
                
                int imgsize = 0;
                
                if((format[scr] & 7) >> 1 || !cfgblk[3])
                {
                    init_tga_image(&img, (u8*)screenbuf, scrw, stride[scr], bits);
                    img.image_type = TGA_IMAGE_TYPE_BGR_RLE;
                    img.origin_y = (scr * 400) + (stride[scr] * offs[scr]);
                    tga_write_to_FILE(k->data, &img, &imgsize);
                    
                    k->packetid = 3; //DATA (Targa)
                    k->size = imgsize;
                }
                else
                {
                    *(u32*)&k->data[0] = (scr * 400) + (stride[scr] * offs[scr]);
                    u8* dstptr = &k->data[8];
                    if(!tjCompress2(jencode, (u8*)screenbuf, scrw, bsiz * scrw, stride[scr], format[scr] ? TJPF_RGB : TJPF_RGBX, &dstptr, (u32*)&imgsize, TJSAMP_420, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT))
                        k->size = imgsize + 8;
                    k->packetid = 4; //DATA (JPEG)
                }
                //k->size += 4;
                
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, fbuf[0] + fboffs, siz, dmaconf);
                //svcFlushProcessDataCache(prochand ? prochand : 0xFFFF8001, capin.screencapture[0].framebuf0_vaddr, capin.screencapture[0].framebuf_widthbytesize * 400);
                //svcStartInterProcessDma(&dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001, (u8*)capin.screencapture[0].framebuf0_vaddr + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, 0x600000 + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, dbgo, siz, dmaconf);
                
                if(++offs[scr] == limit[scr]) offs[scr] = 0;
                
                scr = !scr;
                
                siz = (capin.screencapture[scr].framebuf_widthbytesize * stride[scr]);
                
                bsiz = capin.screencapture[scr].framebuf_widthbytesize / 240;
                scrw = capin.screencapture[scr].framebuf_widthbytesize / bsiz;
                bits = 4 << bsiz;
                
                if((format[scr] & 7) == 2) bits = 17;
                if((format[scr] & 7) == 4) bits = 18;
                
                Handle prochand = 0;
                if(procid) if(svcOpenProcess(&prochand, procid) < 0) procid = 0;
                
                if\
                (\
                    svcStartInterProcessDma\
                    (\
                        &dmahand, 0xFFFF8001, screenbuf, prochand ? prochand : 0xFFFF8001,\
                        (u8*)capin.screencapture[scr].framebuf0_vaddr + (siz * offs[scr]), siz, dmaconf\
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
                
                if(k->size) soc->wribuf();
                /*
                k->packetid = 0xFF;
                k->size = 4;
                *(u32*)k->data = dbgo;
                soc->wribuf();
                
                dbgo += 240 * 3;
                if(dbgo >= 0x600000) dbgo = 0;
                */
                
                if(isold) svcSleepThread(5e6);
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
    
    //if(prochand) svcCloseHandle(prochand);
    //screenExit();
    
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
    
    
    if(isold)
    {
        limit[0] = 8;
        limit[1] = 8;
        stride[0] = 50;
        stride[1] = 40;
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
        u32 siz = isold ? 0x10000 : 0x200000;
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
        errno = 0;
        PatStay(0xFFFF);
        while(checkwifi()) yield();
    }
    
    if(checkwifi())
    {
        cy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if(cy <= 0)
        {
            printf("socket error: (%i) %s\n", errno, strerror(errno));
            hangmacro();
        }
        
        sock = cy;
        
        struct sockaddr_in sao;
        sao.sin_family = AF_INET;
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
    
    if(!isold) osSetSpeedupEnable(1);
    
    PatPulse(0xFF40FF);
    if(haznet) PatStay(0xCCFF00);
    else PatStay(0xFFFF);
    
    while(1)
    {
        hidScanInput();
        kDown = hidKeysDown();
        kHeld = hidKeysHeld();
        kUp = hidKeysUp();
        
        //printf("svcGetSystemTick: %016llX\n", svcGetSystemTick());
        
        if(kDown) PatPulse(0xFF);
        if(kHeld == (KEY_SELECT | KEY_START)) break;
        
        if(!soc)
        {
            if(!haznet)
            {
                if(checkwifi()) goto netreset;
            }
            else if(pollsock(sock, POLLIN) == POLLIN)
            {
                int cli = accept(sock, (struct sockaddr*)&sai, &sizeof_sai);
                if(cli < 0)
                {
                    printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
                    if(errno == EINVAL) goto netreset;
                    PatPulse(0xFF);
                }
                else
                {
                    PatPulse(0xFF00);
                    soc = new bufsoc(cli, isold ? 0xC000 : 0x70000);
                    k = soc->pack();
                    
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
            else if(pollsock(sock, POLLERR) == POLLERR)
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
    
    PatStay(0xFF0000);
    
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
    
    return 0;
}
