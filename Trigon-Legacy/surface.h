// surface.h
// Trigon-Legacy, 2025

#ifndef surface_h
#define surface_h

#include <IOSurface/IOSurfaceRef.h>

#include "common.h"
#include "iokit.h"
#include "device.h"

#define IOSafeRelease(x) \
do \
{ \
    if(MACH_PORT_VALID(x)) \
    { \
        IOObjectRelease(x); \
        x = MACH_PORT_NULL; \
    } \
} while(0)

typedef struct
{
    mach_msg_header_t head;
    struct
    {
        mach_msg_size_t size;
        natural_t type;
        uintptr_t ref[8];
    } notify;
    struct
    {
        kern_return_t ret;
        uintptr_t ref[8];
    } content;
    mach_msg_max_trailer_t trailer;
} msg_t;

extern struct device_info* dev_info;

IOSurfaceRef create_purplegfxmem_iosurface(void);
void* get_base_address(IOSurfaceRef surface);
uint64_t leak_port_addr(mach_port_t port, IOSurfaceRef surface);

#endif /* surface_h */
