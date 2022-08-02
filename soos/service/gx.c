#include "gx.h"


extern u8 gfxThreadID;
extern u8* gfxSharedMemory;
extern Handle gspEvent, gspSharedMemHandle;

void gxInit()
{
    gfxSharedMemory = (u8*)mappableAlloc(0x1000);
    svcCreateEvent(&gspEvent, RESET_ONESHOT);
    GSPGPU_RegisterInterruptRelayQueue(gspEvent, 0x1, &gspSharedMemHandle, &gfxThreadID);
    svcMapMemoryBlock(gspSharedMemHandle, (u32)gfxSharedMemory, (MemPerm)0x3, (MemPerm)0x10000000);
    gpuCmdBuf=(u32*)(gfxSharedMemory+0x800+gfxThreadID*0x200);
	// This function was removed in May 2022 lol
    //gspInitEventHandler(gspEvent, (vu8*) gfxSharedMemory, gfxThreadID);
	gspInit();
    gspWaitForVBlank();
}

void gxExit()
{
    //gspExitEventHandler();
	gspExit();
    svcUnmapMemoryBlock(gspSharedMemHandle, (u32)gfxSharedMemory);
    GSPGPU_UnregisterInterruptRelayQueue();
    svcCloseHandle(gspSharedMemHandle);
    mappableFree(gfxSharedMemory);
    svcCloseHandle(gspEvent);
}
