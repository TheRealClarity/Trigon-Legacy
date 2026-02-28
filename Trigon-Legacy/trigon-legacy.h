// trigon-legacy.h
// Trigon-Legacy, 2025

#ifndef TRIGON_LEGACY_H
#define TRIGON_LEGACY_H

#include <stdint.h>
#include <dlfcn.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <time.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

#include "macho.h"
#include "surface.h"
#include "kern_rw.h"

#define SLEEP_TOKEN_MAGIC 0x52554E4E4D4F5358 // XSOMNNUR
#define TASK_SEATBELT_PORT	7

ret_t trigon_legacy(void);

#endif /* TRIGON_LEGACY_H */
