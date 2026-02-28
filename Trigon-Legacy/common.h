#ifndef common_h
#define common_h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <syslog.h>

#include <mach/mach.h>

typedef enum {
    RET_ERR = -1,
    RET_SUCCESS = 0
} ret_t;

#define LOG(str, args...) do { printf(str "\n", ##args); } while(0)
#define ERR(str, args...) do { fprintf(stderr, str "\n", ##args); } while(0)

#ifdef DEBUG
#undef DEBUG
#define DEBUG(str, args...) do { printf(str "\n", ##args); } while(0)
#else
#define DEBUG(str, args...) do {} while(0)
#endif

#define SYSLOG(x, ...) \
do { \
syslog(LOG_ERR, ""x"\n", ##__VA_ARGS__); \
} while(0)

#endif /* common_h */
