//
//  memory.h
//  Trigon-Legacy
//

#ifndef memory_h
#define memory_h

#include "common.h"
#include "kern_rw.h"

kern_return_t mach_vm_map(vm_map_t target_task, mach_vm_address_t *address,
                          mach_vm_size_t size, mach_vm_offset_t mask, int flags,
                          mem_entry_name_port_t object, memory_object_offset_t offset,
                          boolean_t copy, vm_prot_t cur_protection, vm_prot_t max_protection,
                          vm_inherit_t inheritance);
kern_return_t mach_vm_copy(vm_map_t target_task, mach_vm_address_t source_address, mach_vm_size_t size, mach_vm_address_t dest_address);
kern_return_t mach_vm_allocate(vm_map_t target, mach_vm_address_t *address, mach_vm_size_t size, int flags);
kern_return_t mach_vm_deallocate(vm_map_t target, mach_vm_address_t address, mach_vm_size_t size);
kern_return_t mach_vm_read(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, vm_offset_t *data, mach_msg_type_number_t *dataCnt);
kern_return_t mach_vm_read_overwrite(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t *outsize);
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt);

extern uint64_t gMappingBase;
extern mach_port_t largeMemoryEntry;
extern struct device_info* dev_info;

mach_vm_address_t map_at_offset(uint64_t, size_t);
ret_t physreadbuf(uint64_t, void*, size_t);
ret_t physwritebuf(uint64_t, void*, size_t);
uint64_t physread64(uint64_t);
uint32_t physread32(uint64_t);
ret_t physwrite64(uint64_t, uint64_t);
ret_t physwrite32(uint64_t, uint32_t);

uint64_t early_rk64(uint64_t va);
uint32_t early_rk32(uint64_t va);
ret_t early_wk64(uint64_t va, uint64_t val);
ret_t early_wk32(uint64_t va, uint32_t val);

uint64_t kvtophys(uint64_t virt);
uint64_t find_port(uint64_t self_proc_phys, mach_port_name_t port);
uint64_t find_port_from_task(uint64_t task, mach_port_name_t port);
uint64_t find_port_from_is_table(uint64_t is_table, mach_port_name_t port);
uint64_t find_phys_kbase_static(void);

#define IO_BITS_ACTIVE      0x80000000
#define IKOT_TASK           2

typedef struct {
    struct {
        uintptr_t data;
        uintptr_t pad;
        uintptr_t type;
    } lock; // mutex lock
    uint32_t ref_count;
    int active;
    int halting;
    uintptr_t map;
    char pad[0x308 /* TASK_BSDINFO */ - sizeof(uintptr_t) - (2 * sizeof(int)) - sizeof(uint32_t) - (3 * sizeof(uintptr_t))];
    uintptr_t bsd_info;
} ktask_t;

#define IS_PHYS_ADDR(arg) (((arg) & 0x0000FFFF00000000) == 0x800000000)

#endif /* memory_h */
