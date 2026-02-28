//
//  macho.h
//  Trigon-Legacy
//

#ifndef macho_h
#define macho_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <unistd.h>

#include "patchfinder.h"
#include "memory.h"

bool isKernelHeader(mach_vm_address_t);
ret_t parse_kernel_header(struct device_info*, mach_vm_address_t);

#endif /* macho_h */
