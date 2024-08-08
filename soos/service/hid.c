#include "hid.h"

bool hidThreadRunning = false;
bool hidThreadForceQuit = false;

static u32 hidPadState = 0;
static u64 hidTimeLastUpdated = 0;
u32 getHidPadState() { return hidPadState; }
u64 getHidTimeLastUpdated() { return hidTimeLastUpdated; }

bool hidUsrHandleIsValid = false;
Handle hidHandle = 0;
Handle hidSharedMemHandle = 0;
u32 hidSharedMemAddr;
Handle hidUpdateEventHandle = 0;

//Handle gpioHandle = 0;

// Virtual Address of HID I/O Registers
const u32 HID_IO_ADDR_VMEM = 0xFFFC2000; // as of n3DS v11.1.0 (may be wrong)

void hidThread(void* __dummy_arg__)
{
    hidThreadRunning = true;
    Result ret = 0;
    printf("[hid] hidThread started\n");

    srvSetBlockingPolicy(true); // non-blocking

    hidSharedMemAddr=(u32)mappableAlloc(0x2b0);
    if(!hidSharedMemAddr)
    {
        // todo
    }

    u32 loops = -1;
    // main loop
    while(!hidThreadForceQuit)
    {
        //loops = (loops+1)%256;
        loops++;

        // every x loops...
        if(loops%32 == 0)
        {
            if(!hidUsrHandleIsValid)
            {
                // attempt to (re-)connect to hid
                //printf("[hid] (re-)connecting to hid:USER\n");
                ret = srvGetServiceHandle(&hidHandle, "hid:USER");
                if(ret >= 0)
                {
                    hidUsrHandleIsValid = true;
                    ret = cHid_HID_GetIPCHandles(&hidSharedMemHandle, &hidUpdateEventHandle);
                    if(ret < 0)
                    {
                        // todo
                    }
                    ret = svcMapMemoryBlock(hidSharedMemHandle, (u32)hidSharedMemAddr, MEMPERM_READ, MEMPERM_DONTCARE);
                    if(ret < 0)
                    {
                        // todo
                    }
                } else {
                    if(ret == 0xD0401834) {
                        // the requested service port is full
                        // (hid has a limit of 6 sessions at a time :P)
                        printf("[hid] requested service (hid:USER) is full\n");
                    }
                    // if we fail to (re-)connect to hid, attempt a dirty hack instead
                    // todo
                    MemInfo memInfo;
                    PageInfo pageInfo;
                    ret = svcQueryMemory(&memInfo, &pageInfo, HID_IO_ADDR_VMEM);
                    if(ret >= 0)
                    {
                        //if(memInfo.state != 2)
                            printf("[hid] 0xFFFC2000 MemoryState = %i\n", memInfo.state);
                        //if(memInfo.perm != 1)
                            printf("[hid] 0xFFFC2000 MemoryPermission = %i\n", memInfo.perm);

                        // test
                        svcSleepThread(128000000);
                        u32 hidIo = *((u32*)HID_IO_ADDR_VMEM);
                    }
                    else
                        printf("[hid] svcQueryMemory return code = %i\n", ret);
                }
            }
        }
        else if(loops%32 == 24) // x loops later...
        {
            if(hidUsrHandleIsValid)
            {
                /*
                // test
                Handle testHandle;
                ret = srvGetServiceHandleDirect(&testHandle, "hid:USER");
                if(ret >= 0)
                {
                    svcSleepThread(16000000);
                    svcCloseHandle(testHandle);
                }
                else if(ret == 0xD0401834)
                {
                    printf("[hid] hid has no free slots open. freeing up ChirunoMod's slot...\n");
                }
                */

                // Disconnect from hid to free up a slot
                //printf("[hid] disconnecting from hid:USER\n");
                hidUsrHandleIsValid = false;
                svcCloseHandle(hidHandle);
            }
        }

        // update buttons

        // try to peek at IO register (doesn't work; bad perms)
        if(false)
        {
            MemInfo memInfo;
            PageInfo pageInfo;
            ret = svcQueryMemory(&memInfo, &pageInfo, HID_IO_ADDR_VMEM);
            if(ret >= 0)
            {
                //if(memInfo.state != 2)
                    printf("[hid] 0xFFFC2000 MemoryState = %i\n", memInfo.state);
                //if(memInfo.perm != 1)
                    printf("[hid] 0xFFFC2000 MemoryPermission = %i\n", memInfo.perm);
            }
            else
            {
                printf("[hid] svcQueryMemory return code = %i\n", ret);
            }
            hidPadState = *((u32*)HID_IO_ADDR_VMEM);
            printf("[hid] 0xFFFC2000 data = %X", hidPadState);
            hidTimeLastUpdated = svcGetSystemTick();
        }

        if(hidUsrHandleIsValid)
        {
            u64 hidTimeUpdate = *(u64*)(hidSharedMemAddr+0);
            if(hidTimeLastUpdated == hidTimeUpdate)
            {
                ret = svcWaitSynchronization(hidUpdateEventHandle, 999999);
            }
            else
            {
                hidTimeLastUpdated = hidTimeUpdate;
                hidPadState = *(u32*)(hidSharedMemAddr+0x1C);
            }
        }

        if((hidPadState & (KEY_SELECT|KEY_START)) == (KEY_SELECT|KEY_START))
            hidThreadForceQuit = true;

        svcSleepThread(32000000); // about two frames (at 60fps)
    }

    // shut down
    // todo: unmap sharedmem (?)
    if(hidUsrHandleIsValid)
        svcCloseHandle(hidHandle);
    hidThreadRunning = false;
    hidThreadForceQuit = false;
}

// HID:GetIPCHandles
Result cHid_HID_GetIPCHandles(Handle *sharedMemHandle, Handle *eventHandle1)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x000A0000;
    Result ret = svcSendSyncRequest(hidHandle);
    if(ret < 0) return ret;
    *sharedMemHandle = ipc[3];
    *eventHandle1 = ipc[4];
    return ipc[1];
}

Result cHid_GpioInit()
{
    //return srvGetServiceHandle(&gpioHandle, "gpio:HID");
    return 0;
}

Result cHid_GpioExit()
{
    //return svcCloseHandle(gpioHandle);
    return 0;
}

// GPIO:GetGPIOData
Result cHid_GPIO_GetGPIOData(u32 gpio_bitmask, u32 *outputData)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x00070040;
    ipc[1] = gpio_bitmask;
    //Result ret = svcSendSyncRequest(gpioHandle);
    //if(ret < 0) return ret;
    *outputData = ipc[2];
    return ipc[1];
}

/* note to self...

IO, relevant HID registers are at physical address 0x10146000
which is probably virtual address 0xfffc2000 (depends on sys version)

see also:
https://www.3dbrew.org/wiki/HID_Registers
https://www.3dbrew.org/wiki/Memory_layout#Memory_map_by_firmware


see:
https://www.3dbrew.org/wiki/GPIO_Services


for ZL/ZR/C-Stick (on n3DS)....



 */
