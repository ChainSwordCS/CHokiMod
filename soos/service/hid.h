#pragma once

#include <3ds.h>
#include <stdio.h>

extern void debugPrint(u8, const char *);
extern FILE* f;

u32 getHidPadState();
u64 getHidTimeLastUpdated();

void hidThread(void*);
Result cHid_HID_GetIPCHandles(Handle*, Handle*);
