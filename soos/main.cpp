#include <3ds.h>
//#include <3ds/services/hid.h> // Not necessary. Might be good for documenting, but that's all.
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
#include <ctime>

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
#include "service/gx.h"

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

void corruptVramLol();
void initializeThingsWeNeed();
void initializeGraphics();

// WIP reimplementation functions

// from test-branch-2 (2022-08-09)
void initializeSocketBufferMem(u32**,u32*); // Unfinished, likely broken.
void initializeOurScreenBufferMem(u32**,u32*); // Unfinished, likely broken.
//int mainGetNewSocketEtc(int,struct sockaddr_in*,socklen_t*,SocketBuffer**,SocketBuffer::PacketStruct**); // Unfinished, likely broken.
int netResetFunc(int*); // Unfinished, likely broken.
int mainTryConnectToSocket(int*); // Unfinished, likely broken.


// from test-branch-1 (2022-08-08)
int gpuTriggerDisplayTransfer(u32*,u32*,u16,u16,u32); // Unfinished, likely broken.
u32 memFindAddressWithin(u32,u32,u32); // Unfinished.
void allocateRamAndShareWithGpu(); // Unfinished, likely broken.
u32 bluffMemoryAllocation(u32); // Unfinished, likely broken.
u32 stealAndWasteMemoryLol(); // Unfinished, likely broken.

int checkwifi();
int pollSocket(int,int,int);
void CPPCrashHandler();
void netfunc(void*);
void printMsgWithTime(const char*);
int main(); // So you can call main() from main() (:

static u32 mem_shared_address;

// GSP GPU stuff
static u32 gsp_gpu_handle;
static Handle gpu_mem_shared_handle;
static u32 gpu_mem_shared;
static u32 my_gsp_event;
static u8 my_gsp_thread_id;
static Thread my_gsp_event_thread;

static u32 buttons_pressed = 0;

static u32 socketbuffer_busy = 0; // 1 if currently being read from, 2 if currently being written to

// = 0 if New-3DS (128MB more RAM, 2 more CPU cores, higher CPU clock speed, CPU L2-Cache)
// = 1 if Old-3DS (IIRC... -C)
// Default to 1 (if only for backwards-compatibility with old code)
static u32 is_old_3ds = 1;

const int port = 6464;
static Handle hid_user_mem_handle;

static bool enable_debug_logging = true;

//I think this is over-commented -H

// You are not wrong my friend! It's now even worse than before, so that's good.
// No but, right now I have a million comments so I remember what does and
// doesn't work, and exactly *how* things break, etc... It would definitely be
// good for me to move a lot of this into external documentation instead.
// But for right now, I can't be bothered. And a good portion of these comments
// will become obsolete on their own. -C (2022-08-10)

class SocketBuffer
{
public:
    typedef struct
    {
    	// Note: Previous version of code below (but maybe was broken anyway)
    	u32 packet_type_byte : 8;
    	u32 size : 24;

        //u8 packet_type_byte; // This seems to work


        //u8 size; // The size of a given packet *does* change.
        //u16 unused_bytes_for_compatibility;

        // This variable declaration is unchanged from the old code, FYI.
        u8 data[0];

    } PacketStruct;

    int socket_id;

    // SocketBuffer-buffer-pointer/array. Different from Packet-data-pointer/array (???) -C
    u8* buffer_bytearray_aka_pointer;

    int buffer_size; // The total buffer size doesn't change after boot.
    //int recvsize; // Effectively useless, even in old code.

    // Old Version:
    // SocketBuffer(int passed_sock, int passed_bufsize)
    // Version from Test-Branch-1 (2022-08-07):
    //SocketBuffer(u32 passed_sock, u32 passed_buffer_address, u32 passed_bufsize = 0)
    SocketBuffer(int passed_sock, int passed_bufsize, u32 passed_buffer_address = 0)
    {
        buffer_size = passed_bufsize;
        socket_id = passed_sock;

        // Old code:
        //buffer_bytearray_aka_pointer = new u8[passed_bufsize];
        //recvsize = 0;
        // Maybe revert or remove because of regressions. -C (2022-08-10)
        //if(passed_buffer_address != 0)
        //	buffer_bytearray_aka_pointer = (u8*)passed_buffer_address; // Convert the u32 address into a usable pointer

        // I shouldn't have changed this... It's part of the mem allocation procedure.
        buffer_bytearray_aka_pointer = new u8[passed_bufsize];
    }

    ~SocketBuffer() // Destructor
    {
    	// If this SocketBuffer is already null,
    	// don't attempt to delete it again.
    	// (Could this be safely omitted? -C)
        if(!this)
        	return;
        close(socket_id);
        delete[] buffer_bytearray_aka_pointer;
    }

    // Made this super verbose for no reason. -C
    int isAvailable()
    {
    	int r_pollsocket = pollSocket(socket_id, POLLIN, 0);
    	bool pollsocket_pollin_condition = (r_pollsocket == POLLIN);
    	return (pollsocket_pollin_condition);
    }

    // Returns the number of bytes read, or -1 or -errno or maybe 0 on failure.
    int readbuf(int flags = 0)
    {
        u32 header = 0;
        // IIRC, setting this to 0 is functionally no different from how this code
        // used to work. But maybe keep this in mind... -C (2022-10-08)
        u32 received_size = 0;

        // Get header byte
        int ret = recv(socket_id, &header, 4, flags);
        if(ret < 0) return -errno;
        if(ret < 4) return -1;
        // This line is logically sound.
        *(u32*)buffer_bytearray_aka_pointer = header;
        // Copy the 4 header bytes to the start of the buffer. And the size
        PacketStruct* packet_in_buffer = getPointerToBufferAsPacketPointer();
        // This is the size idiot. I'm dumb. I borked this last time I worked on it. -C (2022-08-18)
        int num_reads_remaining = packet_in_buffer->size;
        // Depending on how exactly the code is written, this may need to be 4.
        int offs = 4;
        while(num_reads_remaining != 0)
        {
        	ret = recv(socket_id, buffer_bytearray_aka_pointer+offs, num_reads_remaining, flags);
            // If we get -1, it failed. If we get 0, we got zero bytes just now.
        	if(ret <= 0)
            	return -errno;
            num_reads_remaining -= ret;
            offs += ret;
        }
        // 'recvsize' variable was written to but never read from, AFAIK. -C (2022-08-10)
        //recvsize = offs;

        // Return value of this function is how many bytes we just read.
        return offs;
    }

    // Returns the number of bytes written, or -1 or -errno or maybe 0 on failure.
    int wribuf(int flags = 0)
    {
    	// Basically all this code is untouched. -C
        int mustwri = getPointerToBufferAsPacketPointer()->size + 4;
        // offs was set to 0 in the original code too. -C
        int offs = 0;
        int ret = 0;

        while(mustwri)
        {
            // if(mustwri >> 12) // bitmask 11111111 11111111 11110000 00000000
        	// Long story short: Don't accidentally buffer-overflow.
        	if(mustwri >= 0x1000)
        	{
                ret = send(socket_id, buffer_bytearray_aka_pointer + offs , 0x1000, flags);
        	}
            else
            {
                ret = send(socket_id, buffer_bytearray_aka_pointer + offs , mustwri, flags);
            }

        	if(ret < 0) // If it failed
        		return -errno;

        	mustwri -= ret;
        	offs += ret;
        }

        return offs;
    }

    // This code is so weird dude. I promise I only refactored the names. -C
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

void corruptVramLol()
{
	u32* ptr = (u32*)0x1F000000;
	int o = 0x00600000 >> 2;
    while(o--)
    {
    	*(ptr++) = rand();
    }
    return;
}

void initializeThingsWeNeed()
{
	// These top two, we maybe shouldn't need. But if they are enabled,
	// they sometimes cause crashes. -C

	//aptExit();
	//nsExit();
	//hidExit();
	//yield();
	//yield();
	//yield();
	//aptInit();
	//nsInit();
	//hidInit();

	mcuInit(); // Notif LED
	// Notif LED = Orange (Boot just started, no fail yet...)
	PatStay(0x0037FF);

	acInit(); // Wifi

	//initializeGraphics();
	//allocateRamAndShareWithGpu(); // No.

	//HIDUSER_GetHandles(&hid_user_mem_handle,nullptr,nullptr,nullptr,nullptr,nullptr);

	return;
}

// Lots of code borrowed and slightly modified from libctru - gspgpu.c
void initializeGraphics()
{
	// gspInit() crashes if we call it without calling gspExit() first.

	//gspExit();
	//yield();
	//yield();
	//yield();
	//GSPGPU_ResetGpuCore();
	//yield();
	//yield();
	//yield();
	//gspInit();
	//if(r != 0) // Just in case
		//PatStay(0x0000FF);

	//void* gsp_ref_cnt_ptr;

	//AtomicPostIncrement(gsp_ref_cnt_ptr); //
	int r = srvGetServiceHandle(&gsp_gpu_handle, "gsp::Gpu");

	if(enable_debug_logging)
	{
		if(r<0)
			printMsgWithTime(&"Failed to get gsp::Gpu handle."[0]);
		else
			printMsgWithTime(&"Successfully got gsp::Gpu handle."[0]);

		printf(" handle(u32) = %i ; return code = %i\n",gsp_gpu_handle,r);
	}


	//gsp_gpu_handle = *(gspGetSessionHandle());

	// Only one process can have rendering rights at a time.

	//r = GSPGPU_AcquireRight(0);
	// I don't know if this is important but it fails.
	//r = gspHasGpuRight();
	//if(r)
	//	PatStay(0x00FF00);
	//else
	//	PatStay(0x0000FF);

	return;
}

// Pass this function pointers to your variables.
void initializeSocketBufferMem(u32** address_pointer_pointer, u32* size_pointer)
{
    // FYI, this must be page-aligned (0x1000)
    if(is_old_3ds)
    	*size_pointer = 0x10000; // If Old-3DS
    else
    	*size_pointer = 0x200000; // If New-3DS

    // Would it benefit optimization to replace 'memalign' with native syscalls?
    // We'd have more fine control over what we compile into, but
    // may help us a net-zero amount. -C
    *address_pointer_pointer = (u32*)memalign(0x1000, *size_pointer);

    // Initialize the SOC service.
    int ret = socInit(*address_pointer_pointer, *size_pointer);

    if(ret < 0)
    {
    	*(u32*)0x1000F0 = ret;
    	//hangmacro();
    }

	return;
}

void initializeOurScreenBufferMem(u32** address_pointer_pointer, u32* size_pointer)
{
	if(is_old_3ds)
    {
		*size_pointer = 50 * 240 * 4; // On Old-3DS (we have 128MB less FCRAM)
    }
    else
    {
    	*size_pointer = 400 * 240 * 4;
    }

	*address_pointer_pointer = (u32*)memalign(8,*size_pointer);

    if(!(*address_pointer_pointer)) // If memalign returns null or 0
    {
        makerave();
        svcSleepThread(2e9);
        //hangmacro();
    }
    return;
}

// Returns 0 on success.
// If this returns -1, please call netResetFunc()
// If this returns -2, please call hangmacro()
int mainGetNewSocketEtc(int input_sock, struct sockaddr_in* ptr_to_sai, socklen_t* ptr_to_sai_size, SocketBuffer** ptr_2_ptr_soc, SocketBuffer::PacketStruct** ptr_2_ptr_k)
{
	int r;
	r = pollSocket(input_sock, POLLIN, 0);
	if(r == POLLIN)
	{
		// FD stands for File Descriptor
		s32 newsocket_fd = accept(input_sock, (struct sockaddr*)ptr_to_sai, ptr_to_sai_size);
		if(newsocket_fd < 0)
		{
			printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
		    if(errno == EINVAL)
		    {
		    	return -1;
		    }
		    //PatPulse(0x0000FF);
		    return -2;
		}
		else
		{
			//PatPulse(0x00FF00);
			// Size of new socket buffer, smaller if on Old-3DS.
			u32 bufsize;
			if(is_old_3ds)
			{
				bufsize = 0xC000;
			}
			else
			{
				bufsize = 0x70000;
			}

			*ptr_2_ptr_soc = new SocketBuffer(newsocket_fd,bufsize);
			*ptr_2_ptr_k = (*ptr_2_ptr_soc)->getPointerToBufferAsPacketPointer();
			return 0;
		}
	}
	else if(pollSocket(input_sock, POLLERR, 0) == POLLERR)
	{
		printf("POLLERR (%i) %s", errno, strerror(errno));
		return -1;
	}
	else if(r == -1)
		return -1;
	else
		return -2;
	return -2; // We should never fall all the way through to here. Logic or syntax error.
}

// Returns 0 on success.
// If returns -1, please call hangmacro()
int netResetFunc(int* ptr_to_sock)
{
	if(*ptr_to_sock)
	{
	    close(*ptr_to_sock);
	    *ptr_to_sock = 0;
	}

	int r;
	u32 wifi;
	r = ACU_GetStatus(&wifi);

	if(wifi && (errno == EINVAL || r < 0) )
	{
	    errno = 0;
	    //PatStay(0x00FFFF);
	    while(r < 0)
	    {
	    	r = ACU_GetStatus(&wifi);
	    	yield();
	    }
	}

	ACU_GetStatus(&wifi);
	if(wifi != 0)
	{
		return mainTryConnectToSocket(ptr_to_sock);
	}
	return 0;
}

// Returns 0 on success.
// If returns -1, please call hangmacro()
int mainTryConnectToSocket(int* ptr_to_sock)
{
	s32 filedescriptor_for_socket;
	// formerly, "PF-INET" was "AF-INET"
	filedescriptor_for_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(filedescriptor_for_socket <= 0)
	{
		printf("socket error: (%i) %s\n", errno, strerror(errno));
		return -1;
	}

	*ptr_to_sock = filedescriptor_for_socket;

	struct sockaddr_in sao;
	sao.sin_family = PF_INET;
	sao.sin_addr.s_addr = gethostid();
	sao.sin_port = htons(port);

	if(bind(*ptr_to_sock, (struct sockaddr*)&sao, sizeof(sao)) < 0)
	{
		printf("bind error: (%i) %s\n", errno, strerror(errno));
		return -1;
	}

	//fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if(listen(*ptr_to_sock, 1) < 0)
	{
		printf("listen error: (%i) %s\n", errno, strerror(errno));
		return -1;
    }
	return 0;
}

// Returns 0 if no error (iirc)
// For flags, pass 0 to fall back to default
int gpuTriggerDisplayTransfer(u32* vram_address, u32* out_address, u16 width, u16 height, u32 input_flags)
{

	u32 color_out = 3; // RGB5_A1
	u32 flags = 0;

	if(input_flags != 0)
		flags = input_flags;
	else
	{
		flags = 0 + (color_out << 12);
	}

	u32 input_dimensions = (((u32)height) << 16) + width;
	u32 output_dimensions = input_dimensions;

	// Setting flags to all 0 is worth considering...
	// I really doubt THAT'S the one thing that makes
	// this whole function broken though. -C (2022-08-10)
	return GX_DisplayTransfer(vram_address,input_dimensions,out_address,output_dimensions,flags);
}

// Copied from libctru "mappable.c"
u32 memFindAddressWithin(u32 start, u32 end, u32 size)
{
	MemInfo info;
	PageInfo pgInfo;

	u32 addr = start;
	while (addr >= start && (addr + size) < end && (addr + size) >= addr)
	{
		if (R_FAILED(svcQueryMemory(&info, &pgInfo, addr)))
			return 0;

		if (info.state == MEMSTATE_FREE)
		{
			u32 sz = info.size - (addr - info.base_addr); // a free block might cover all the memory, etc.
			if (sz >= size)
				return addr;
		}

		addr = info.base_addr + info.size;
	}

		return 0;
}

// This function is relatively unsafe, with little error handling.
// Since we just run it once at the beginning, that won't be an issue.
// If we fail here of all places, we either crash or shutdown the process.
// There are no consequences for crashing though (here, anyway). -C
void allocateRamAndShareWithGpu()
{
	// Also note this function may be full of logic errors and typos. Sorry. -C

	// Maybe don't do this? (Commented-out 2022-08-08)
	//mem_shared_address = (u32)mappableAlloc(0x1000);
	//u32 mem_request_address = mem_shared_address;

	u32 memblock_size; // Size of memory block that we request (in bytes)

	// Size is slightly hard to determine...
	// If copied directly from GPU, each pixel
	// is 2 bytes (with my settings.)
	//
	// Currently, my code asks for just one frame / capture each time.
	// So the size of this buffer will account for that.
	//
	// Also, 32 bytes extra just in case. (TODO: Remove later once verified working)
	//
	if(is_old_3ds)
	{
		memblock_size = (50 * 240) * 4 + 64;
	}
	else
	{
		memblock_size = (400 * 240) * 4 + 64;
	}

	// the socket buffer will be located right after in memory
	// Old-3DS offset = 50*240*2+32
	// New-3DS offset = 400*240*2+32

	// Round up to nearest multiple of 0x1000 (never round down)
	memblock_size = (memblock_size+0x999) & 0x11111000; // uses bitwise-AND operator

	//u32 size_of_mem_bluffing = memblock_size - 0x1000;
	//u32 received_potential_mem_address;
	//u32 allocated_mem_within_gpu_range = 0;
	//do
	//{
	//	size_of_mem_bluffing = size_of_mem_bluffing + 0x1000;
	//	received_potential_mem_address = (u32)mappableAlloc(size_of_mem_bluffing);
	//	allocated_mem_within_gpu_range = 0;
	//}
	//while(allocated_mem_within_gpu_range < memblock_size);

	//TODO: If we wanted to ever do compat with old firmware,
	// use OS_OLD_FCRAM_VADDR instead on those firms. -C
	u32 mem_search_start = OS_FCRAM_VADDR;
	u32 mem_search_end = OS_FCRAM_VADDR + 0x06800000;
	mem_shared_address = memFindAddressWithin(mem_search_start,mem_search_end,memblock_size+0x999);

	// Align the memory address (multiple of 0x1000)
	mem_shared_address = (mem_shared_address + 0x999) & 0x11111000;

	// If we didn't find enough free memory, fall-back to normal procedure.
	if(mem_shared_address == 0)
		mem_shared_address = (u32)mappableAlloc(memblock_size);

	u32 mem_request_address = mem_shared_address;

	MemPerm p1 = MEMPERM_READWRITE; // This process's permissions in the memory block. (my / our permissions)
	MemPerm p2 = MEMPERM_READWRITE; // Other processes' permissions in the memory block.

	// Probably needs to be in the APPLICATION Memory Region
	// Because the GPU doesn't have RW access to a lot of the
	// SYSTEM region and has zero access to the BASE region.
	//
	// Note: MEMOP_REGION_APP might be ignored. ):
	// Which would mean, if we're not an Application,
	// then the GPU effectively can't actually access
	// this memory for different reasons. ):
	//
	// Note: This is probably VERY BROKEN in its current state. -C (2022-08-10)
	MemOp flags = (MemOp)(MEMOP_REGION_APP|MEMOP_ALLOC_LINEAR); // Note: I don't know if 'MEMOP_ALLOC' is also required or not. I wonder if one may accidentally override the other.


	// Note: I was experimenting if the order of these two commands was making a difference.
	// Remind me next time to be less stupid and get debug log write-to-file
	// working first. -C (2022-08-10)

	//svcControlMemory(&mem_shared_address,mem_request_address,0,memblock_size,flags,p1);

	//svcCreateMemoryBlock(&mem_shared_handle,mem_shared_address,memblock_size,p1,p2);
	//svcControlMemory(&mem_shared_address,mem_request_address,0,memblock_size,flags,p1);

	// Note to self. Try svcControlMemory op=COMMIT?

	// Note: This flag is called "COMMIT" on 3dbrew.org,
	// but something different in libctru. I assume the ones with
	// matching indices are functionally identical,
	// but I actually haven't checked the libctru source code
	// and I honestly should. -C (2022-08-10)


	u32 result;
	result = svcControlMemory(&mem_shared_address,mem_request_address,0,memblock_size,flags,p1);
	if(result)
		//hangmacro();

	//result = svcCreateMemoryBlock(&mem_shared_handle,mem_shared_address,memblock_size,p1,p2);
	//if(result)
	//	hangmacro();


	// I think I got confused, I think this call isn't necessary.
	//svcMapMemoryBlock(mem_shared_handle,mem_shared_address,p1,p2);

	// Dummied out for now.
	// I think this code is moving the GPU Command Buffer itself to
	// the start of this shared memory block. I don't actually
	// know if or why we need to do that. -ChainSwordCS
	//
	// For what it's worth, maybe that code was unused for a reason.
	// (IIRC it was in gx.c, not main.cpp.)  -C (2022-08-10)
	//
	//
	// This code effectively ported from Sono's code.
	//
	//u8 gsp_thread_id;
	//Handle gsp_cmd_event_handle;
	//svcCreateEvent(&gsp_cmd_event_handle, RESET_ONESHOT);
	//GSPGPU_RegisterInterruptRelayQueue(gsp_cmd_event_handle,0x1,&gpu_shared_mem_handle,&gsp_thread_id);

	// Obsoleted this code by rewriting mem allocation
	    //
	    //if(is_old_3ds)
	    //{
	    //    screenbuf = (u32*)memalign(8, 50 * 240 * 4); // On Old-3DS
	    //}
	    //else
	    //{
	    //    screenbuf = (u32*)memalign(8, 400 * 240 * 4); // On New-3DS
	    //}



	if(!mem_shared_address) // If we ended up failing, maybe hang instead idk
		//hangmacro();

	return;
}

// How ironic, I knew this code was unnecessary and broken,
// so I kept it unused even in the first commit the code was added. -C (2022-08-10)
u32 bluffMemoryAllocation(u32 input_mem_size)
{
	u32 received_potential_mem_address;
	u32 allocated_mem_within_gpu_range = 0;
	u32 size_of_mem_bluffing = input_mem_size;

	received_potential_mem_address = (u32)mappableAlloc(size_of_mem_bluffing);
	allocated_mem_within_gpu_range = 0;

	while(allocated_mem_within_gpu_range < size_of_mem_bluffing)
	{
		size_of_mem_bluffing = size_of_mem_bluffing + 0x1000;
		received_potential_mem_address = (u32)mappableAlloc(size_of_mem_bluffing);
		allocated_mem_within_gpu_range = 0;
	}

	return received_potential_mem_address;
}

// returns a handle to the memory block just allocated
// *Currently unused, but should work. -C
u32 stealAndWasteMemoryLol()
{
	// How much memory do we want to eat up each time?
	// Don't make this too small, or else we might
	// theoretically overflow the Handle stack.(?)
	u32 mem_increment_size = 0x10000;
	u32 this_mem_address = (u32)mappableAlloc(mem_increment_size);

	u32 r; //-esult
	r = svcControlMemory(&this_mem_address,this_mem_address,0,mem_increment_size,MEMOP_ALLOC,MEMPERM_READWRITE);
	if(r)
		return 0; // This error should be handled by the calling function, please.
	else
		return this_mem_address;
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

// Returns 0 on error
// Returns the returned-events, with a bitmask of the passed-events applied
// (i.e. if you're checking for 01, it'll return 01 or 00.)
int pollSocket(int passed_socket_id, int passed_events, int timeout = 0)
{
	struct pollfd pfds = pollfd(); // Initialize it first... (?????)

    pfds.fd = passed_socket_id;
    pfds.events = passed_events;

    nfds_t num_of_file_descriptors = 1;

    // poll(); would return 1 on a _net_error_code 'E2BIG', in theory. (described in soc_common.c)
    // poll(); would return the value of nfds (num_of_file_descriptors) on success?
    // We always pass in 1, so it should return 1 on success(?)
    //
    // poll(); will only return 0 if cmdbuf[1] is 0 and ALSO cmdbuf[2] is 0.
    // poll(); will return -1 on error.
    // poll(); will occasionally return a different negative int on error.
    // poll(); will occasionally return the result of svcSendSyncRequest, if it was non-zero. (Is this ever 1 in practice? Perhaps not)
    //
    // For more info, see:
    // https://github.com/devkitPro/libctru/blob/master/libctru/source/services/soc/soc_poll.c
    // https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/svc.h
    // https://github.com/devkitPro/libctru/blob/master/libctru/source/services/soc/soc_common.c
    //
    // -ChainSwordCS (2022-08-18)
    if(poll(&pfds, num_of_file_descriptors, timeout) == 1)
    {
    	// If we are passed 01 (POLLIN), and it succeeds, the pollSocket function returns 01.
    	// If we are passed 08 (POLLERR), and it succeeds, the pollSocket function returns 08.
        return pfds.revents & passed_events;
    }
    else
    {
    	// Simplify everything about poll();
    	// and just return 0 on error.
    	return 0;
    }
}


// These variables were at one point declared up here(?) Don't remember why. -C
//static bufsoc* soc = nullptr;
//static bufsoc::packet* k = nullptr;

// And IIRC these two were always here, or otherwise at least present. IDK. -C
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
    
    //PatStay(0xFFFFFF);
    //PatPulse(0xFF);
    
    svcSleepThread(1e9);
    
    hangmacro();
    
    killswitch:
    longjmp(__exc, 1);
}

// what
extern "C" u32 __get_bytes_per_pixel(GSPGPU_FramebufferFormat format);

static u32 kDown = 0;
static u32 kHeld = 0;
static u32 kUp = 0;

static GSPGPU_CaptureInfo my_gpu_capture_info;

static Result ret = 0;
//static int cx = 0;
static int cy = 0;

static u32 offs[2] = {0, 0};
static u32 limit[2] = {1, 1};
static u32 stride[2] = {80, 80};
// I hate this so much but fine
static u32 format[2] = {0xF00FCACE, 0xF00FCACE};

static u8 cfgblk[0x100];

static int sock = 0;

static struct sockaddr_in sai;
static socklen_t sizeof_sai = sizeof(sai);

static SocketBuffer* socketbuffer_object_pointer = nullptr;

static SocketBuffer::PacketStruct* k = nullptr;

static Thread netthread = 0;
static Handle os_thread_handle_of_netthread = 0;
static vu32 threadrunning = 0;

// screenbuf = Beginning of the data in the packet we are making...
// I don't remember if this should be a void*, a u8*, a u32*...
static void* screenbuf = nullptr;
//static u32 screenbuf_address;

static tga_image img;
static tjhandle turbo_jpeg_instance_handle = nullptr;


void netfunc(void* __dummy_arg__)
{
    u32 siz = 0x80;
    u32 bsiz = 1;
    u32 scrw = 1;
    u32 bits = 8;
    
    int scr = 0;
    
    if(is_old_3ds)
    {
    	// Commented-out before I got here. -C
    	// screenbuf = (u32*)k->data;
    }
    else
    {
    	// This instruction *might* crash. Leaving it commented for now is harmless.
    	// It may not even be needed in the first place.
    	//osSetSpeedupEnable(1);
    }
    
    // Written before I got here. I forget what or why. -C
    k = socketbuffer_object_pointer->getPointerToBufferAsPacketPointer(); //Just In Case (tm)
    
    //PatStay(0x00FF00); // Notif LED = Green

    // Note to self: execution gets here at least. -C (2022-08-17)
    
    format[0] = 0xF00FCACE; //invalidate
    
    u32 procid = 0;
    Handle dmahand = 0;
    
    // Set up DMA Config (Needed for memcpy stuff)
	DmaConfig dma_config;
	dmaConfigInitDefault(&dma_config);
	dma_config.channelId = -1; // auto-assign to a free channel (Arm11: 3-7, Arm9: 0-1)
	dma_config.endianSwapSize = 0;
	dma_config.flags = 0;


	// This was commented out before I got here. -C
    //dmaconf[2] = 4;
    //screenInit();
    
    //PatPulse(0x7F007F); // Notif LED = Purple-ish
    threadrunning = 1;
    
    //initializeGraphics(); // Does this work? Does it solve anything?

    // why???
    //
    // Note: If this code runs in the middle of receiving packets and copying to cfgblk,
    // It can effectively trash the contents of cfgblk on accident.
    // I will implement a variable or something, to denote when
    // another thread is already accessing the SocketBuffer.
    //
    // Note 2: I don't actually know if that's correct... -C (2022-08-17)
    // At any rate, this code seems to work*
    do
    {
        k->packet_type_byte = 2; //MODE
        k->size = 4 * 4;
        
        u32* kdata = (u32*)k->data;
        
        kdata[0] = 1;
        kdata[1] = 240 * 3;
        kdata[2] = 1;
        kdata[3] = 240 * 3;
        socketbuffer_object_pointer->wribuf();
    }
    while(0);
    
    // Is this something I want to do? Will this even work?
    // I think *this thread* may need its own handle for the GSP.
    // -C (2022-08-19)
    // JK, lol, this causes the thread to crash / hang.
    //gspInit();

    // We have a thread opened.
    // It'll loop through this code indefinitely,
    // unless it hits a 'break' command,
    // or if the main thread says threadrunning = 0
    //
    // Infinite loop unless halted by an outside force.
    //
    // TODO: Consider changing this to or adding
    // AptMainLoop() (or whatever it was called).
    // However, that very well may break things.
    // It might have caused things to break when I
    // added it to the main() function. -C (2022-08-10)
    //PatStay(0x001C1C); // Debug LED very dim Yellow (Thread is indeed running)
    while(threadrunning)
    {
    	// TODO: Checking for data sent from PC every loop is
    	// a waste of CPU time. I'll change it somehow eventually. -C
        if(socketbuffer_object_pointer->isAvailable())
        {
        	// why? Fine, I guess. lol.
			while(1)
			{
				//PatStay(0x003800); // Debug Code, Debug LED Green
				if((buttons_pressed & (KEY_SELECT | KEY_START)) == (KEY_SELECT | KEY_START))
				{
					delete socketbuffer_object_pointer;
					socketbuffer_object_pointer = nullptr;
					break;
					// By the way, does this break out of
					// both while loops or just one? -C
				}

				//while(socketbuffer_busy)
				//	yield();
				//socketbuffer_busy = 1;

				puts("reading");
				// Just using variable cy as another "res". why
				cy = socketbuffer_object_pointer->readbuf();
				if(cy <= 0)
				{
					printf("Failed to recvbuf: (%i) %s\n", errno, strerror(errno));
					delete socketbuffer_object_pointer;
					socketbuffer_object_pointer = nullptr;
					//socketbuffer_busy = 0;
					break;
				}
				else // readbuf(); returns a positive integer on success.
				{
					printf("#%i 0x%X | %i\n", k->packet_type_byte, k->size, cy);

					//reread: // unused label IIRC. -C

					u8 h;
					void* memcpy_address_to_copy_to;
					void* memcpy_address_to_copy_from;
					u32 memcpy_size_in_bytes = 0;

					switch(k->packet_type_byte)
					{
						case 0x00: //CONNECT
							// For compatibility, just initialize ourselves anyway...
							// I don't expect to actually ever receive this IIRC
							//socketbuffer_busy = 0;
							cfgblk[0] = 1; // Manually enable ourselves.
							cfgblk[3] = 70; // Manually set JPEG quality to 70
							break;
						case 0x01: //ERROR
							// I don't know if ChokiStream actually
							// sends any of these. This might be redundant.
							puts("forced dc");
							delete socketbuffer_object_pointer;
							socketbuffer_object_pointer = nullptr;
							//socketbuffer_busy = 0;
							break;

						case 0x7E: //CFGBLK_IN
							//PatStay(0x00387F); // Debug Code, Debug LED Orange

							// Old Code!
							//memcpy(cfgblk + k->data[0], &k->data[4], min((u32)(0x100 - k->data[0]), (u32)(k->size - 4)));

							// Refactoring this again because I hate it.

							// Note: the "packet type" and "packet size" are chopped off and not copied.
							// Note 2: the first byte of the "Data" is expected to be the location to which data is copied(?????) (the starting point for memcpy i think?)

							// The first byte of Data (or "Data") is an offset, i.e. at what index of cfgblk do we want to start writing to?
							h = k->data[0];
							memcpy_address_to_copy_to = &(cfgblk[h]);

							// The fifth byte of Data (or "Data") is where we start copying from. Note, we are copying *FROM* k, which is already in RAM, which is why we get the address of the fifth byte in the array.
							memcpy_address_to_copy_from = &(k->data[4]);

							// The number of bytes to copy! Simple (TM)
							//
							// The lesser of the two:
							// a. Received size (k->data[3]) minus 4 because we throw away an extra four bytes above
							// b. Maximum size of cfgblk (0x100) minus the offset (k->data[0])
							//
							// Notes
							// 1. If the reported size is 0-3, we will underflow (and this will error-handle that).
							// 2. If the copied data would go beyond the end of the cfgblk array (due to high offset and size), this will error-handle that(?)
							// 3. the offset (k->data[0]) will never be greater than 255 (0xFF) so we will never underflow there.
							//
							// Subtracting 4 from the size is correct.
							memcpy_size_in_bytes = min( (u32)((u32)0x100 - k->data[0]), (u32)(k->size - 4));

							memcpy(memcpy_address_to_copy_to, memcpy_address_to_copy_from, memcpy_size_in_bytes);

							//TODO: This is stupid. Placeholder.
							cfgblk[0] = 1;
							//cfgblk[3] = 70;

							// The first byte *of the packet* is the packet-type
							// Bytes 6-8 are the size of the data, in bytes. (24-bit integer)
							// And everything after is perceived as data.
							//
							// The first byte of *data* indicates at what index we copy to.
							// Skip three bytes for no reason... (TODO: Please redo this later)
							// Copy <size> bytes
							//
							//cfg_copy_data_size = (u32)(k->size) - 8; // This should probably work

							// Note that it's *possible* to receive 256 bytes and write them to config block
							// But that much was never used in practice.
							// TODO: When I reimplement this, simplify and remove that extra functionality.

							yield(); // Wait a little bit, let the orange actually be visible.
							//PatStay(0x001C1C);
							// Also, if the user is incrementally changing JPEG quality, we don't
							// need to try and process *all* of that info anyway. -C
							//
							// Note: That's not how that works. Now the 3DS lags behind when the
							// PC sends too many packets in quick succession. -C
							// TODO: Refactor this and fix it.

							break;

						default:
							printf("Invalid packet ID: %i\n", k->packet_type_byte);
							// TODO: Uncomment these two lines assuming they're not in the way.
							// They're from the old code.
							//delete socketbuffer_object_pointer;
							//socketbuffer_object_pointer = nullptr;
							//socketbuffer_busy = 0;
							break;
					}

					break;
				}
				break; // Just in case
			}
		// Having added this bracket SHOULDN'T break anything...
		// Because previously, "if(socketbuffer_object_pointer->isAvailable())"
		// Didn't have brackets and just implicitly led to the "while(1)" statement.
		// But keep in mind, there's a small chance this is breaking something... -C (2022-08-10)
        }
        
        //PatStay(0x001C1C); // Debug LED very dim Yellow (Thread is indeed running)
        //PatStay(0x003800); // Debug Code, Debug LED Green

        if(!socketbuffer_object_pointer)
        {
        	break;
        }


        // Note to self: consider trying different functions too.
        // Like the code just below this message. -C (2022-08-19)
        //
        // u32 screen0_framebuffer1_address = 0;
        // r = GSPGPU_ReadHWRegs(addr,&screen0_framebuffer1_address,len);
        //
        // Try to read GPU external hardware registers directly?
        //
        // User VA     Physical Address  Name
        // 0x1EF00400  0x10400400        Framebuffer Setup (Top Screen) (Length $100)
        // 0x1EF00500  0x10400500        Framebuffer Setup (Bottom Screen) (Length $100)
        //
        // https://www.3dbrew.org/wiki/GPU/External_Registers#LCD_Source_Framebuffer_Setup
        //
        // offset $68 = Framebuffer A 1st Address (left eye if 3D)
        // offset $6C = Framebuffer A 2nd Address (left eye if 3D)
        // offset $70 = Framebuffer format (and other settings?)
        // offset $94 = Framebuffer B 1st Address (right eye if 3D)
        // offset $98 = Framebuffer B 2nd Address (right eye if 3D)



        // Old Code:
        // It is important that GSPGPU_ImportDisplayCaptureInfo does not fail.
        //if((cfgblk[0] != 0x00) && (GSPGPU_ImportDisplayCaptureInfo(&my_gpu_capture_info) >= 0))
        //{
        // New code might be broken, IDK. -C
        //
        // If index 0 of config-block is non-zero! (Set by initialization sorta packet)
        // And this ImportDisplayCaptureInfo function doesn't error out...

        int r;
        //gspWaitForVBlank(); // Maybe this will help?
        //r = GSPGPU_SaveVramSysArea(); // This seems to crash, maybe not allowed? idk
        //if(r>=0)
        	//PatStay(0x00FF00);


        // This function seems to always fail. I'm investigating why. -C (2022-08-19)
        // https://www.3dbrew.org/wiki/GSPGPU:ImportDisplayCaptureInfo

        r = GSPGPU_ImportDisplayCaptureInfo(&my_gpu_capture_info); // TODO: Currently broken here. -C (2022-08-18)
        // Note: This function from libctru hasn't changed since 2017.

        printf("Address of my_gpu_capture_info = %i\n", &my_gpu_capture_info);
        printf("GSPGPU_ImportDisplayCaptureInfo function called...\n");
        printf("Function return value = %i\n", r);

        if(r < 0)
        {
        	//PatStay(0x00007F);
        	yield();
        }

        if(r >= 0)// if GSPGPU_ImportDisplayCaptureInfo succeeds
        {
        	//PatStay(0x003800); // Debug Code, Debug LED Green
        	// (Sono's Comment)
			//test for changed framebuffers
			if\
			(\
				my_gpu_capture_info.screencapture[0].format != format[0]\
				||\
				my_gpu_capture_info.screencapture[1].format != format[1]\
			)
			{
				//PatStay(0xFFFF00); // Notif LED = Teal (Green + Blue)

				// Already commented out before I got here. -C
				//
				//fbuf[0] = (u8*)capin.screencapture[0].framebuf0_vaddr;
				//fbuf[1] = (u8*)capin.screencapture[1].framebuf0_vaddr;

				format[0] = my_gpu_capture_info.screencapture[0].format;
				format[1] = my_gpu_capture_info.screencapture[1].format;

				//while(socketbuffer_busy)
				//	yield();
				//socketbuffer_busy = 2;

				// Note this one line of code used to be like 4 lines up. Small chance it's broken.
				k->packet_type_byte = 2; //MODE

				k->size = 4 * 4; // Why? Just hard-code it to 16???

				u32* kdata = (u32*)k->data; // Pointer to k->data as if it was an array of u32 objects.

				kdata[0] = format[0];
				kdata[1] = my_gpu_capture_info.screencapture[0].framebuf_widthbytesize;
				kdata[2] = format[1];
				kdata[3] = my_gpu_capture_info.screencapture[1].framebuf_widthbytesize;
				socketbuffer_object_pointer->wribuf();

				// The above code should be safe. For some reason, the code just below this might be broken.

				k->packet_type_byte = 0xFF; // Debug Packet (sending to PC)
				k->size = sizeof(my_gpu_capture_info); // This should be hard-coded to something but whatever.

				//*(GSPGPU_CaptureInfo*)k->data = my_gpu_capture_info;
				//
				// Write to k->data[0] as if k->data was an array of GSPGPU_CaptureInfo objects.
				// I think... -C (2022-08-16)
				//void* k_gpu_cap = (void*)(k->data); // Scratch this line of code. -C
				//

				// This should be functionally the same but I'm reverting this change.
				//((GSPGPU_CaptureInfo*)(k->data))[0] = my_gpu_capture_info;
				*(GSPGPU_CaptureInfo*)k->data = my_gpu_capture_info;

				socketbuffer_object_pointer->wribuf();

				//socketbuffer_busy = 0;


				// what
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
					(u32)my_gpu_capture_info.screencapture[0].framebuf0_vaddr >= 0x1F000000\
					&&\
					(u32)my_gpu_capture_info.screencapture[0].framebuf0_vaddr <  0x1F600000\
				)
				{
					//nothing to do?
				}
				else //use APT fuckery, auto-assume application as all retail applets use VRAM framebuffers
				{
					// Debug LED Pattern: Red, Green, Red, Green...
					//memset(&pat.r[0], 0xFF, 16);
					//memset(&pat.r[16], 0, 16);
					//memset(&pat.g[0], 0, 16);
					//memset(&pat.g[16], 0xFF, 16);
					//memset(&pat.b[0], 0, 32);
					//pat.ani = 0x2004;
					//PatApply();

					u64 app_program_id = -1ULL;
					bool application_registered = false;

					while(1)
					{
						application_registered = false; // ?
						while(1)
						{
							// &application_registered = pRegistered / Pointer to output the registration status to
							// application_registered = Registration Status(?) of the specified application(?)
							if(APT_GetAppletInfo(APPID_APPLICATION, &app_program_id, nullptr, &application_registered, nullptr, nullptr) < 0)
							{
								break; // Break on failure
							}
							if(application_registered == true)
							{
								break; // Break if true. Otherwise, svcSleepThread and repeat.
							}

							svcSleepThread(15e6);
						}

						if(!application_registered)
						{
							break; // Break if this has become false.
						}

						if(NS_LaunchTitle(app_program_id, 0, &procid) >= 0)
						{
							break; // If successful, break. If we fail, loop.
						}
					}

					if(application_registered)
					{
						// Commented out before I got here. -C
						// svcOpenProcess(&prochand, procid);
					}
					else
					{
						format[0] = 0xF00FCACE; //invalidate
					}
				}

				//PatStay(0x00FF00); // Notif LED = Green
			}

        }
        //else if(cfgblk[0] && r >= 0)

        // if cfgblk[0] is non-zero, and make sure we didn't just fail
        if(cfgblk[0] != 0 && r >= 0)
        {
        	//PatStay(0x007F00); // Debug LED: green at 50% brightness
        	u8* destination_ptr; // Consider moving this down.

            int loopcnt = 2;
            //lmao, I think this loop runs once. -H
            // "--loopcnt" returns the value of loopcnt.
            // So true if 1, false if 0. -C
            //
            // Ohhhhh. This is, like, a ratio of
            // how often we process frames vs
            // how often we check if settings have changed.
            // By default it's 1:1, but I'll likely change
            // it to 2 or more if I can. -C
            while(--loopcnt)
            {
                if(format[scr] == 0xF00FCACE)
                {
                    scr = !scr;
                    continue;
                }
                
                //while(socketbuffer_busy)
                //	yield();
                //socketbuffer_busy = 2;

                // Crap. I forget why this is where it is........ -C (2022-08-16)
                // Default value...
                k->size = 0;
                
                if(dmahand)
                {
                    svcStopDma(dmahand);
                    svcCloseHandle(dmahand);
                    dmahand = 0;

                    // Looks like I broke this, and I just fixed it again hopefully. -C
                    //
                    //if(!is_old_3ds) svcFlushProcessDataCache(0xFFFF8001, (u8*)screenbuf, capin.screencapture[scr].framebuf_widthbytesize * 400);
					//if(!is_old_3ds) svcFlushProcessDataCache(0xFFFF8001, (u32)&screenbuf, my_gpu_capture_info.screencapture[scr].framebuf_widthbytesize * 400);
                    //
                    if(!is_old_3ds)
                    {
                    	svcFlushProcessDataCache(0xFFFF8001,\
                    	(u32)screenbuf,\
						my_gpu_capture_info.screencapture[scr].framebuf_widthbytesize * 400);
                    }
                }
                
                int imgsize = 0;
                
                // TODO: Maybe remove this line for regression reasons? -C
                // Legit don't remember what this does or why it's here. lmao. -C (2022-08-16)
                //destination_ptr = &k->data[8];

                //if(!cfgblk[3])
                //
                // (format[scr] & 0b110)
                // This conditional statement checks the framebuffer color format...
                // True if the color format is:
                // GL_RGB565_OES (2),  GL_RGB5_A1_OES (3),  or GL_RGBA4_OES (4)
                // False if the color format is:
                // GL_RGBA8_OES (0),  GL_RGB8_OES (1)
                //
                // These color formats will be forced to output as Targa, because
                // encoding them to JPEG is not yet implemented.
                //
                // This conditional statement also forces output as Targa
                // if the "JPEG Quality" value received from the PC application
                // equals 0.
                // (That is the intended behavior anyway...)

                //PatStay(0x003838); // Debug LED: yellow at 25% brightness

                // I know this is stupid, just let me be stupid in peace )...: -C
                bool tga_conditional = false;
                // If requested JPEG quality is 0, we do Targa instead
                if(cfgblk[3] == 0)
                	tga_conditional = true;
                // If the framebuffer color format is not supported
                // by our current JPEG code (formats listed above),
                // then force-enable Targa and not JPEG.
                if((format[scr] & 0b110) != 0)
                	tga_conditional = true;

                if(tga_conditional) // The code is messing up here or something
                {
                	//PatStay(0x007F00); // Debug LED: green at 50% brightness
                    init_tga_image(&img, (u8*)screenbuf, (u16)scrw, (u16)stride[scr], (u8)bits);
                    img.image_type = TGA_IMAGE_TYPE_BGR_RLE;
                    img.origin_y = (scr * 400) + (stride[scr] * offs[scr]);
                    tga_result r_tga = tga_write_to_FILE(k->data, &img, &imgsize);

                    if(r_tga == (tga_result)TGA_NOERR)
                    {
                    	//PatPulse(0x00);
                    	k->size = imgsize;
                    	//PatStay(0x007F00); // Debug LED: green at 50% brightness
                    }
                    else
                    {
                    	//PatPulse(0x0000FF);
                    	//PatStay(0x00007F); // Debug LED: red at 50% brightness
                    }

                    k->packet_type_byte = 0x03; //DATA (Targa)
                    //k->size = imgsize;
                }
                else
                {
                	// slightly changed this line, shouldn't break. -C
                    *((u32*)(k->data)) = (scr * 400) + (stride[scr] * offs[scr]);

                    // (Renamed) line of old code:
                    destination_ptr = &(k->data[8]); // The first 8 bytes are a "header". The 9th byte is the beginning of data.

                    // Please make this not all one line. -C
                    int ret3;

                    // Unfortunately, I have no idea what this bit of code does.
                    int arg_a = 0;
                    if(format[scr])
                    	arg_a = (int)TJPF_RGB;
                    else
                    	arg_a = (int)TJPF_RGBX;
                    //Exact old code:
                    // if(!tjCompress2(jencode, (u8*)screenbuf, scrw, bsiz * scrw, stride[scr], format[scr] ? TJPF_RGB : TJPF_RGBX, &dstptr, (u32*)&imgsize, TJSAMP_420, cfgblk[3], TJFLAG_NOREALLOC | TJFLAG_FASTDCT))
                    //
                    //     tjCompress2(void *,               (u8*) const unsigned char *,  int,         int,         int,                        int,unsigned char * *, unsigned long int *,   int,              int, int)
                    ret3 = tjCompress2((void*)turbo_jpeg_instance_handle, (u8*)screenbuf, (int)(scrw), (int)(bsiz * scrw), (int)(stride[scr]), arg_a, &destination_ptr, (u32*)&imgsize, (int)TJSAMP_420, (int)(cfgblk[3]), (int)(TJFLAG_NOREALLOC | TJFLAG_FASTDCT));

                    if(ret3 == 0) // Expecting tjCompress2() returns 0 on success, -1 on failure
                    {
                    	//PatStay(0x007F00); // Debug LED: green at 50% brightness
                    	k->size = imgsize + 8; // Formerly +8, not +4.
                    }
                    else // We fail here, right now. -C (2022-08-18)
                    {
                    	//PatStay(0x00007F); // Debug LED: red at 50% brightness
                    	//k->size = 0; // I added this, it may not be at all necessary.
                    	//PatPulse(0x00FFFF);
                    }
                    k->packet_type_byte = 0x04; //DATA (JPEG)
                }

                // Commented out before I got here, IIRC. -C
                //
                // k->size += 4;
                // svcStartInterProcessDma(&dmahand, 0xFFFF8001, (u32)screenbuf, prochand ? prochand : 0xFFFF8001, fbuf[0] + fboffs, siz, dmaconf);
                // svcFlushProcessDataCache(prochand ? prochand : 0xFFFF8001, capin.screencapture[0].framebuf0_vaddr, capin.screencapture[0].framebuf_widthbytesize * 400);
                // svcStartInterProcessDma(&dmahand, 0xFFFF8001, (u32)screenbuf, prochand ? prochand : 0xFFFF8001, (u8*)capin.screencapture[0].framebuf0_vaddr + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, 0x600000 + fboffs, siz, dmaconf);
                //screenDMA(&dmahand, screenbuf, dbgo, siz, dmaconf);
                
                if(++offs[scr] == limit[scr])
                {
                	offs[scr] = 0;
                }
                
                scr = !scr;
                
                siz = (my_gpu_capture_info.screencapture[scr].framebuf_widthbytesize * stride[scr]);
                
                bsiz = my_gpu_capture_info.screencapture[scr].framebuf_widthbytesize / 240;
                scrw = my_gpu_capture_info.screencapture[scr].framebuf_widthbytesize / bsiz;
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

                // what
                if(procid)
                {
                	if(svcOpenProcess(&prochand, procid) < 0)
                	{
                		procid = 0;
                	}
                }
                
                // Note: unexpected behavior may occur if data is read before this DMA is finished.
                // The dma-handle will be signaled when it's complete.
                // Note 2: Changed destination process handle from 0xFFFF8001 to
                // the handle stored in netthread (this thread's handle)
                //
                os_thread_handle_of_netthread = threadGetHandle(netthread);
                int r_dma = svcStartInterProcessDma(
                		&dmahand,
						os_thread_handle_of_netthread, // "Destination Process Handle"
						(u32)screenbuf, // Destination Address
						(prochand ? prochand : os_thread_handle_of_netthread), // "Source Process Handle"
                        (u32)my_gpu_capture_info.screencapture[scr].framebuf0_vaddr + (siz * offs[scr]), // Source Address
						siz, // Amount of data to copy (bytes)
						&dma_config);

                if(r_dma < 0)
                {
                    procid = 0;
                    format[scr] = 0xF00FCACE; //invalidate
                }
                else
                {
                	//PatStay(0x00FF00);
                	//PatStay(0x007F1C); // Debug LED: Yellow-green (50% green, 12.5% red)
                }
                
                // Why do we do this?
                if(prochand)
                {
                    svcCloseHandle(prochand);
                    prochand = 0;
                }


                // GpuTriggerDisplayTransfer code (broken, unfinished)
                //TODO: Set height and width correctly again when this works.
                // (If this ever works, lol. -C, 2022-08-10)
                //
                //int ret4 = gpuTriggerDisplayTransfer((u32*)screenbuf, (u32*)destination_ptr, 100, 100, 0);
                //if(ret4 < 0)
                //	PatStay(0x0000FF);
                //gspWaitForEvent(GSPGPU_EVENT_PPF, false);


                // TODO: Should this be done *before* we do DMA stuff?
                // Why do we do DMA stuff before this?
                // Both are after we copied everything we wanted to k...
                // -C (2022-08-16)
                //
                if(k->size > 0) // If size is 0, we decide to send nothing.
                {
                	socketbuffer_object_pointer->wribuf();
                }
                //socketbuffer_busy = 0;

                // Commented out before I got here. -C
                /*
                k->packetid = 0xFF;
                k->size = 4;
                *(u32*)k->data = dbgo;
                soc->wribuf();
                
                dbgo += 240 * 3;
                if(dbgo >= 0x600000) dbgo = 0;
                */
                
                if(is_old_3ds)
                {
                	svcSleepThread(5e6);
                }
            }
        }
        else yield();
    }
    
    //PatStay(0x0010FF); // Thread shut down, Debug LED very-Red -ish Orange
    //
    // Debug Info: Thread has shut down. (Will restart on its own)
    // Debug LED Color Pattern: Yellow, Purple, Yellow, Purple...
    memset(&pat.r[0], 0xFF, 16);
    memset(&pat.g[0], 0xFF, 16);
    memset(&pat.b[0], 0x00, 16);
    memset(&pat.r[16],0x7F, 16);
    memset(&pat.g[16],0x00, 16);
    memset(&pat.b[16],0x7F, 16);
    pat.ani = 0x0406;
    PatApply();
    
    if(socketbuffer_object_pointer)
    {
        delete socketbuffer_object_pointer;
        socketbuffer_object_pointer = nullptr;
    }
    
    // why
    if(dmahand)
    {
        svcStopDma(dmahand);
        svcCloseHandle(dmahand);
    }
    
    // Commented out before I got here. -C
    //
    //if(prochand) svcCloseHandle(prochand);
    //screenExit();
    
    // Is this redundant? Perhaps not.
    // I didn't know code execution even got here. -C (2022-08-10)
    threadrunning = 0;
}

static FILE* log_file_ptr = nullptr;

static ssize_t stdout_write(struct _reent* r, void* fd, const char* ptr, size_t len) //used to be "int fd" not "void* fd"
{
    if(log_file_ptr == NULL)
    	return 0;

    fputs("[STDOUT] ", log_file_ptr);
    return fwrite(ptr, 1, len, log_file_ptr);
}

static ssize_t stderr_write(struct _reent* r, void* fd, const char* ptr, size_t len)
{
    if(!log_file_ptr) return 0;
    fputs("[STDERR] ", log_file_ptr);
    return fwrite(ptr, 1, len, log_file_ptr);
}

// Note: Changing "stdout_write" to "&stdout_write" does not, in fact, fix it. -C (2022-08-10)
                                     //{name, structSize, *open r, *close r, *write r, *read r, *seek r, *fstat_r}
static const devoptab_t devop_stdout = { "stdout", 0, NULL, NULL, stdout_write, NULL, NULL, NULL};
static const devoptab_t devop_stderr = { "stderr", 0, NULL, NULL, stderr_write, NULL, NULL, NULL};

void initSdLogStuff()
{
	// Changed to "a", for "Append". So we add new text to the file.
	log_file_ptr = fopen("/HzLog.log", "a");
	//if((s32)log_file_ptr <= 0)  //Maybe switch condition to (log_file_ptr == NULL)?? -H
	if(log_file_ptr != NULL)
	{
		//devoptab_list is from sys/iosupport.h. Idk what it does. -H
		devoptab_list[STD_OUT] = &devop_stdout;
		devoptab_list[STD_ERR] = &devop_stderr;

		// Redirect stdout and stderr to the file? -C (2022-08-19)
		freopen("HzLog.log", "a", stdout);
		freopen("HzLog.log", "a", stderr);

		//Turn off buffering for stdout and stderr.
		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
	}

}

// Pass this function a string, and it will printf() that string with the date and time a line above
void printMsgWithTime(const char* passed_text)
{
	timeval my_timeval_obj;
	__SYSCALL(gettod_r)(nullptr,&my_timeval_obj,nullptr);
	char* time_text = std::asctime(std::localtime(&(my_timeval_obj.tv_sec)));

	// I did this because the time text has a newline control character at the end of it
	// And it looks fine and I can't be bothered figuring out how to remove it. -C
	printf("[TIME]: %s[DEBUG]: %s",time_text,passed_text);
}

int main()
{
	// Note! This successfully executing is dependent on timing, I think. ): -C (2022-08-20)

	mcuInit(); // Notif LED
	// Notif LED = Orange (Boot just started, no fail yet...)
	//PatStay(0x0037FF);
	nsInit();
	//hidInit(); // Might break
	//aptInit(); // Might break

	// Isn't this already initialized to null?
    socketbuffer_object_pointer = nullptr;

	//initializeThingsWeNeed();

    if(enable_debug_logging)
    {
    	initSdLogStuff();
    	printMsgWithTime(&"Execution started, log initialized. Hello world! :3\n"[0]);
    }
    
    memset(&pat, 0, sizeof(pat));
    memset(&my_gpu_capture_info, 0, sizeof(my_gpu_capture_info));
    memset(cfgblk, 0, sizeof(cfgblk));
    
    //is_old_3ds, or is_old, tells us if we are running on Original/"Old" 3DS (reduced clock speed and RAM...)
    if(APPMEMTYPE <= 5) // Perhaps a weird way of checking this, but it does work.
    {
        is_old_3ds = 1;
    }

    if(is_old_3ds)
    {
        limit[0] = 8; // Multiply by this to get the full horizontal res of a screen.
        limit[1] = 8; // I assume we're capturing it in chunks. On Old-3DS this makes it look awful.
        stride[0] = 50; // Width of the framebuffer we use
        stride[1] = 40; // On Old-3DS, this is pitiful... But I get it
    }
    else
    {
        limit[0] = 1;
        limit[1] = 1;
        stride[0] = 400; // Width of the framebuffer we use
        stride[1] = 320;
    }

    // Notif LED = Orange (Boot just started, no fail yet...)
    PatStay(0x0037FF);

    acInit();

    do
    {
        u32 siz = is_old_3ds ? 0x10000 : 0x200000;
        ret = socInit((u32*)memalign(0x1000, siz), siz);
    }
    while(0);
    // Web socket stuff. Size of a buffer; is page-aligned (0x1000)
    //u32 soc_buffer_size;

    //if(is_old_3ds)
    //{
    	//soc_buffer_size = 0x10000; // If Old-3DS
    //}
    //else
    //{
    	//soc_buffer_size = 0x200000; // If New-3DS
    //}

    // Initialize the SOC service.
    // Potential issue: userland-privileged programs can't change the buffer address (pointer) after creation.
    // This may or may not ever be an issue, but I'm documenting it for completeness. -C (2022-08-10)
    //ret = socInit((u32*)memalign(0x1000, soc_buffer_size), soc_buffer_size);

    if(ret < 0)
    {
    	// The returned value of the socInit function
    	// is written at 0x001000F0 in RAM (for debug). (...?)
    	*(u32*)0x1000F0 = ret;
    	//hangmacro();
    }


    turbo_jpeg_instance_handle = tjInitCompress();

    if(!turbo_jpeg_instance_handle) // if tjInitCompress() returned null, an error occurred.
    {
    	// Write a debug error code in RAM (at 0x001000F0) (...?)
    	*(u32*)0x1000F0 = 0xDEADDEAD;
    	//hangmacro();
    }

    // Call this function exactly here(?) -C (2022-08-20)
    //gspInit();
    initializeGraphics();

    // Hoping to obsolete the next two 'if' statements
    // When re-re-rewriting memory allocation... lol. -C (2022-08-10)
    //
    // Might adjust the mem allocation code for real this time. -C (2022-08-18)
    //
    if(is_old_3ds)
    {
    	// If this is broken, revert to (u8*)?
        screenbuf = (u32*)memalign(8, 50 * 240 * 4); // On Old-3DS
    }
    else
    {
    	// If this is broken, revert to (u8*)?
        screenbuf = (u32*)memalign(8, 400 * 240 * 4); // On New-3DS
    }

    if(!screenbuf) // If memalign() returns null or 0 (fails)
    {
        makerave();
        svcSleepThread(2e9);
        hangmacro();
    }
    
    // I'm just leaving this here. I don't know what it does.
    if((__excno = setjmp(__exc))) goto killswitch;

#ifdef _3DS
    std::set_unexpected(CPPCrashHandler);
    std::set_terminate(CPPCrashHandler);
#endif

    // This label is used...
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


    // at the beginning of boot, does this consistently return 0?
    // (by which i mean, haznet = 0, etc.)
    if(checkwifi())
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
    if(!is_old_3ds)
    {
    	//osSetSpeedupEnable(1);
    }
    
    //PatPulse(0xFF40FF);

    if(haznet)
    {
    	//PatStay(0xCCFF00);
    }
    else
    {
    	//PatStay(0x00FFFF);
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
    //
    // Well, this might have become broken when I tried changing it
    // to use AptMainLoop() (or whatever it was called). So maybe leave it. -C (2022-08-10)

    while(1)
    {
    	// This hidScanInput() function call might crash.
        //hidScanInput();

        //kDown = hidKeysDown();
        //kHeld = hidKeysHeld();
        buttons_pressed = hidKeysHeld();
        //kUp = hidKeysUp();

        //printf("svcGetSystemTick: %016llX\n", svcGetSystemTick());

        // If any buttons are pressed, make the Notif LED pulse red?
        // Pure waste of CPU time for literally no reason
        // Also it's annoying -C
        if(buttons_pressed) PatPulse(0x0000FF);

        if(buttons_pressed == (KEY_SELECT | KEY_START))
        	break;

        if(!socketbuffer_object_pointer)
        {
            if(!haznet)
            {
                if(checkwifi()) goto netreset;
            }
            else if(pollSocket(sock, POLLIN, 0) == POLLIN)
            {
            	//formerly this variable was named "cli"
                int new_socket_file_descriptor = accept(sock, (struct sockaddr*)&sai, &sizeof_sai);
                if(new_socket_file_descriptor < 0)
                {
                    printf("Failed to accept client: (%i) %s\n", errno, strerror(errno));
                    if(errno == EINVAL) goto netreset;
                    //PatPulse(0x0000FF);
                }
                else
                {
                    //PatPulse(0x00FF00);

                    //TODO: Uhh, make sure this area of code isn't completely broken... -C (2022-08-10)
                    //
                    //TODO: Change these to other variables or constants so the formulas automatically sync up.
                    int memoffset = 0;
                    if(is_old_3ds)
                    	memoffset=50*240*2+32;
                    else
                    	memoffset=400*240*2+32;

                    // Note: I don't know if me rewriting SocketBuffer ends up breaking this.
                    // Because I changed the number of arguments and possibly their order.
                    // I forget, sorry. -C (2022-08-10)
                    //
                    // Old:
                    // socketbuffer_object_pointer = new SocketBuffer(cli, is_old_3ds ? 0xC000 : 0x70000);
                    // Test-Branch-1 (2022-08-07):
                    // socketbuffer_object_pointer = new SocketBuffer(cli, mem_shared_address+memoffset, memoffset); // third argument passed used to be "is_old_3ds ? 0xC000 : 0x70000
                    socketbuffer_object_pointer = new SocketBuffer(new_socket_file_descriptor, is_old_3ds ? 0xC000 : 0x70000);
                    k = socketbuffer_object_pointer->getPointerToBufferAsPacketPointer();

                    if(is_old_3ds)
                    {
                        netthread = threadCreate(netfunc, nullptr, 0x2000, 0x21, 1, true);
                    }
                    else
                    {
                        netthread = threadCreate(netfunc, nullptr, 0x4000, 8, 3, true);
                    }

                    if(!netthread)
                    {
                        //memset(&pat, 0, sizeof(pat));
                        //memset(&pat.r[0], 0xFF, 16);
                        //pat.ani = 0x102;
                        //PatApply();

                        svcSleepThread(2e9);
                    }

                    //Could above and below if statements be combined? lol -H

                    if(netthread)
                    {
                    	// After thread_continue_running = 1,
                    	// we can continue execution
                        while(!threadrunning) yield();
                    }
                    else
                    {
                        delete socketbuffer_object_pointer;
                        socketbuffer_object_pointer = nullptr;
                        hangmacro();
                    }
                }
            }
            else if(pollSocket(sock, POLLERR, 0) == POLLERR)
            {
                printf("POLLERR (%i) %s", errno, strerror(errno));
                goto netreset;
            }
        }

        if(netthread && !threadrunning)
        {
            //TODO: Is this code broken? I haven't changed it yet, but it may be. -C
            netthread = nullptr;
            goto reloop;
        }

        if((buttons_pressed & (KEY_ZL | KEY_ZR)) == (KEY_ZL | KEY_ZR))
        {
        	corruptVramLol();
            //u32* ptr = (u32*)0x1F000000;
            //int o = 0x00600000 >> 2;
            //while(o--) *(ptr++) = rand();
        }

        //yield();
    }
    
    killswitch:

    PatStay(0xFF0000); // If we ever actually reach killswitch, make the Notif LED blue

    if(netthread)
    {
        threadrunning = 0;
        
        volatile SocketBuffer** vsoc = (volatile SocketBuffer**)&socketbuffer_object_pointer;
        // Note from ChainSwordCS: I didn't write that comment. lol.
        // But I'd make a note of it and also ask why
        while(*vsoc) yield(); //pls don't optimize kthx
    }
    
    if(socketbuffer_object_pointer)
    	delete socketbuffer_object_pointer;
    else
    	close(sock);

    puts("Shutting down sockets...");
    SOCU_ShutdownSockets();

    socExit();

    //gxExit();

    // With the current state of the code, we never init the GSP service...
    //gspExit();

    acExit();

    if(log_file_ptr)
    {
        fflush(log_file_ptr);
        fclose(log_file_ptr);
    }

    //hidExit();
    PatStay(0);
    mcuExit(); // Probably don't change
    APT_PrepareToCloseApplication(false);
    //aptExit();
    return 0;
}
