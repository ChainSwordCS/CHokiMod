#include <3ds.h>

#include "miscdef.h"


void __system_allocateHeaps(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;
    
    extern u32 __ctru_heap;
    extern u32 __ctru_heap_size;
    extern u32 __ctru_linear_heap;
    extern u32 __ctru_linear_heap_size;
    
    extern int __stacksize__;
    
    u32 tmp = 0;
    Result res = 0;
    
    u32 mem = APPMEMTYPE > 5 ? 0x320000 : 0x64000; //max is 0x88000 on old3DS, but it prevents programs from starting
    
    // Distribute available memory into halves, aligning to page size.
    //u32 size = (osGetMemRegionFree(MEMREGION_SYSTEM) / 2) & 0xFFFFF000;
    __ctru_heap_size = (mem + 0xFFF) & ~0xFFF;
    __ctru_linear_heap_size = APPMEMTYPE > 5 ? 0x2A0000 : 0;
    
    if(APPMEMTYPE > 5) __stacksize__ = 0x10000; else __stacksize__ = 0x8000;
    
    //*(u32*)0x00100998 = size;
    
    
    // Allocate the application heap6
    __ctru_heap = 0x08000000;
    res = svcControlMemory(&tmp, __ctru_heap, 0x0, __ctru_heap_size, (MemOp)MEMOP_ALLOC, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));
    if(res < 0) *(u32*)0x00100100 = res;
    
    // Allocate the linear heap
    //__ctru_linear_heap = 0x14000000;
    //svcControlMemory(&tmp, 0x1C000000 - __ctru_linear_heap_size, 0x0, __ctru_linear_heap_size, (MemOp)MEMOP_FREE, (MemPerm)(0));
    if(__ctru_linear_heap_size)
    {
        res = svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size, (MemOp)MEMOP_ALLOC_LINEAR, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));
        if(res < 0) *(u32*)0x00100200 = res;
        if(__ctru_linear_heap < 0x10000000) *(u32*)0x00100071 = __ctru_linear_heap;
    }
    
    // Set up newlib heap
    fake_heap_start = (char*)__ctru_heap;
    fake_heap_end = fake_heap_start + __ctru_heap_size;
}
