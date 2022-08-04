#include "mcu.h"


static Handle mcuHandle = 0;

// Returns a ServiceHandle for the MCU
Result mcuInit()
{
    return srvGetServiceHandle(&mcuHandle, "mcu::HWC");
}

// Close the ServiceHandle for the MCU
Result mcuExit()
{
    return svcCloseHandle(mcuHandle);
}

// Reads from a specified MCU register. Arguments are: register, pointer to your data, and size of your data.
Result mcuReadRegister(u8 reg, void* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x10082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xC;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

// Writes to a specified MCU register. Arguments are: register, pointer to your data, and size of your data.
Result mcuWriteRegister(u8 reg, void* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x20082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xA;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuHandle);
    if(ret < 0) return ret;
    return ipc[1];
}
