#pragma once

#include "nfhelp.h"
#include <3ds.h>

// Basically copied from original code written by Sono
inline void tryStopDma(Handle* dmahandle)
{
	if(*dmahandle)
	{
		svcStopDma(*dmahandle);
		svcCloseHandle(*dmahandle);
		*dmahandle = 0;
	}
}

