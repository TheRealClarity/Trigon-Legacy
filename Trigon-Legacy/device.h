//
//  device.h
//  Trigon-Legacy
//

#ifndef device_h
#define device_h

#include <sys/sysctl.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>

#include "common.h"
#include "utils.h"

typedef struct boot_args {
    uint16_t        revision;
    uint16_t        version;
    uint64_t        virtBase;
    uint64_t        physBase;
    uint64_t        memSize;
    uint64_t        topOfKernelData;
    uint64_t        video[6];
    uint32_t        machineType;
    void            *deviceTreeP;
    uint32_t        deviceTreeLength;
    char            commandLine[256];
    uint64_t        boot_flags;
} boot_args;

struct kernel_config {
    size_t pagesize;
    size_t phys_align;  // real physical alignment (0x4000 for A8X, pagesize otherwise)
    uint64_t sleep_token_buffer_base;
    uint8_t boot_args_ver;
    uint8_t boot_args_rev;
};

struct kernel_state {
    uint64_t kPhysBase;
    uint64_t kVirtBase;
    uint64_t kVirtEnd;
    uint64_t kVirtSlide;
    uint64_t dram_base;
    uint64_t dram_end;
    uint64_t kPhysBootArgs;
    uint64_t cpu_ttep;
    uint64_t kernproc;
    struct boot_args bootargs;
    mach_port_t tfp0;
};

struct kstruct_offsets {
    /* proc */
    uint64_t proc_task;
    uint64_t proc_p_fd;
    uint64_t proc_pid;
    /* task */
    uint64_t task_prev; // queue_chain_t tasks->prev
    uint64_t task_bsd_info;
    uint64_t task_itk_self;
    uint64_t task_itk_bootstrap;
    uint64_t task_itk_seatbelt;
    uint64_t task_itk_space;
    /* ipc_space */
    uint64_t is_table;
    uint64_t is_task;
    /* ipc_port */
    uint64_t ipc_port_ip_receiver;
    uint64_t ipc_port_ip_kobject;
    uint64_t ipc_port_ip_srights;
    /* filedesc -> fileproc -> fglob */
    uint64_t fd_ofiles;
    uint64_t fp_fglob;
    uint64_t fg_data;
    /* pipe */
    uint64_t pb_buffer;
};

struct pte_info {
    uint8_t offset_bit;
    uint64_t offset_mask;
    uint8_t table_index;
    uint64_t table_mask;
};

struct pipebuf {
    u_int cnt;
    u_int in;
    u_int out;
    u_int size;
    uint64_t buffer;
};

struct pipe_exploit_state {
    int pipe1[2];
    int pipe2[2];
    uint64_t pipe1_addr;
    uint64_t pipe2_addr;
    struct pipebuf pipe1_pb;
    struct pipebuf pipe2_pb;
};

struct device_info {
    cpu_subtype_t cpu_family;
    char model[64];
    uint8_t major, minor, patch;
    struct kernel_config kernel_config;
    struct pte_info pteinfo;
    struct kstruct_offsets kstruct_offsets;
    struct kernel_state kernel_state;
    struct pipe_exploit_state pipe_state;
};

ret_t init_device_info(struct device_info*);

#endif /* device_h */
