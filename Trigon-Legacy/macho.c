//
//  macho.c
//  Trigon-Legacy
//

#include "macho.h"

static uint64_t find_bootargs(uint64_t prelink_end) {
    uint64_t offset = prelink_end - gMappingBase;
    size_t map_size = dev_info->kernel_config.phys_align;
    while (1) {
        mach_vm_address_t mapping_addr = map_at_offset(offset, map_size);
        if (mapping_addr == -1) {
            return 0;
        }
        struct boot_args* ba = (struct boot_args*)mapping_addr;
        if (ba->version == dev_info->kernel_config.boot_args_ver && ba->revision == dev_info->kernel_config.boot_args_rev) {
            if(IS_PHYS_ADDR(ba->topOfKernelData) && IS_PHYS_ADDR(ba->physBase)) {
                mach_vm_deallocate(mach_task_self(), mapping_addr, map_size);
                return offset + gMappingBase;
            }
        }
        mach_vm_deallocate(mach_task_self(), mapping_addr, map_size);
        offset += map_size;
    }
    return 0;
}

static void resolve_kernproc_from_text(struct device_info *dev_info, struct segment_command_64 *seg) {
    struct section_64* sect = (struct section_64*)((char*)seg + sizeof(struct segment_command_64));
    for (int i = 0; i < seg->nsects; i++) {
        if (!strcmp(sect->sectname, "__text")) {
            mach_vm_address_t text_text = map_at_offset(dev_info->kernel_state.kPhysBase + sect->offset - gMappingBase, sect->size);
            if (text_text == -1) {
                ERR("Failed to map in kernel __TEXT__text (wtf?)");
                return;
            }
            
            const uint8_t kernproc_oracle[] = {0xC8, 0x10, 0x82, 0x52}; // MOV W8, #0x1086
            
            // oracle is a bit before seg end, skip 0xD0000
            uint32_t* insn = reverse_memmem((void*)text_text, sect->size, kernproc_oracle, sizeof(kernproc_oracle));
            if (!insn) {
                ERR("Failed to find instruction (wtf?)");
                mach_vm_deallocate(mach_task_self(), text_text, sect->size);
                return;
            }

            for (int j = 0; j < 20; j++) {
                uint32_t* current_insn = insn - j;
                if ((uintptr_t)current_insn < text_text) {
                    break;
                }
                if (insn_is_ldr_imm_64(current_insn)) {
                    // TODO: iOS 12 has multiple LDR, keep last(first?)
                    uint64_t pc_ref = find_pc_rel_value_64((uint8_t*) text_text, sect->size, current_insn + 1, insn_ldr_imm_rn_64(current_insn));
                    uint64_t kernproc_ptr = physread64(dev_info->kernel_state.kPhysBase + sect->offset + pc_ref); // kernproc ptr
                    dev_info->kernel_state.kernproc = kernproc_ptr;
                    DEBUG("kernproc @ %#llx", kernproc_ptr);
                    break;
                }
            }
            mach_vm_deallocate(mach_task_self(), text_text, sect->size);
            return;
        }
        sect++;
    }
}

ret_t parse_kernel_header(struct device_info* dev_info, mach_vm_address_t kHeader) {
    struct mach_header_64* mh = (struct mach_header_64*)kHeader;
    // chinese JBs trash the end of kernel mach header, so we'll use prelink text instead of prelink info
    // we also need to be careful to not access invalid data, in order to not panic
    if (mh->sizeofcmds > dev_info->kernel_config.pagesize - sizeof(struct mach_header_64)) {
        ERR("Error: sizeofcmds is too large, skipping this header");
        return RET_ERR;
    }
    uint64_t lc_offset = 0;
    for (int i = 0; i < mh->ncmds; i++) {
        if (lc_offset + sizeof(struct load_command) > mh->sizeofcmds) {
            ERR("Error: lc_offset out of bounds");
            break;
        }
        struct load_command *lc = (struct load_command*)(kHeader + sizeof(struct mach_header_64) + lc_offset);
        if (lc->cmdsize < sizeof(struct load_command) || lc_offset + lc->cmdsize > mh->sizeofcmds) {
            ERR("Error: invalid cmdsize %u", lc->cmdsize);
            break;
        }

        if(lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)lc;
            if (!strcmp(seg->segname, "__PRELINK_TEXT")) {
                uint64_t prelink_sz = seg->vmsize;
                // we're assuming text is always before prelink info...
                // TODO: not true for 11 (sepfw in the middle), check 10; fix?: start mapping until boot args "magic" is found?
                uint64_t prelink_offset = seg->vmaddr - dev_info->kernel_state.kVirtBase;
                dev_info->kernel_state.kVirtEnd = seg->vmaddr + seg->vmsize; // for kvtophys
                dev_info->kernel_state.kPhysBootArgs = find_bootargs(dev_info->kernel_state.kPhysBase + prelink_offset + prelink_sz);
                DEBUG("Bootargs phys is %#llx", dev_info->kernel_state.kPhysBootArgs);
                struct boot_args* ba = malloc(sizeof(struct boot_args));
                if (!ba) {
                    ERR("Failed to allocate boot_args");
                    return RET_ERR;
                }
                physreadbuf(dev_info->kernel_state.kPhysBootArgs, ba, sizeof(struct boot_args));
                dev_info->kernel_state.bootargs = *ba;
                dev_info->kernel_state.cpu_ttep = ba->topOfKernelData + (5 * dev_info->kernel_config.pagesize); // haxx? 5 iOS 9
                dev_info->kernel_state.dram_base = ba->physBase;
                dev_info->kernel_state.dram_end = ba->physBase + ba->memSize;
                DEBUG("cpu_ttep is %#llx", dev_info->kernel_state.cpu_ttep);
                free(ba);
            }
            if (!strcmp(seg->segname, "__TEXT")) {
                DEBUG("(Static) Virtual Kernel Base %#llx", seg->vmaddr);
                //LOG("seg size %#llx", seg->vmsize);
                dev_info->kernel_state.kVirtBase = seg->vmaddr;
                //dev_info->kernel_info.kVirtSlide = seg->vmaddr - 0xFFFFFF8004004000; // TODO: remove hardcode here
                //resolve_kernproc_from_text(dev_info, seg);
            }
            // if (!strcmp(seg->segname, "__TEXT_EXEC")) { // TODO: iOS 10+, need to clean this up...
            //     struct section_64* sect = (struct section_64*)((char*)seg + sizeof(struct segment_command_64));
            //     //LOG("%#llx %#llx", seg->fileoff, sect->offset);
            //     for (int i = 0; i < seg->nsects; i++) {
            //         if (!strcmp(sect->sectname, "__text")) {
            //             mach_vm_address_t text_text = map_at_offset(dev_info->kernel_info.kPhysBase + sect->offset - gMappingBase, sect->size);
            //             if (text_text == -1) {
            //                 ERR("Failed to map in kernel __TEXT__text (wtf?)");
            //                 break;
            //             }
            //             // C8 10 C2 F2 iOS10 only
            //             uint8_t kernproc_oracle[] = {0xC8, 0x10, 0xC2, 0xF2}; // iOS 10 oracle
            //             //uint8_t kernproc_oracle[] = {0xC8, 0x10, 0x82, 0x52}; // MOV W8, #0x1086
            //             uint32_t* insn = reverse_memmem((void*)text_text, sect->size, kernproc_oracle, sizeof(kernproc_oracle) / sizeof(*kernproc_oracle));
            //             if (!insn) {
            //                 ERR("Failed to find instruction (wtf?)");
            //                 break;
            //             }
            //             LOG("kernproc oracle at %#lx, insn %#x", ((char*)insn - (char*)text_text), *insn);
            //             int found = 0;
            //             for (int i = 0; i < 20; i++) {
            //                 if (insn_is_ldr_imm_64(insn - i)) {
            //                     //                                if (!found) {
            //                     //                                    found++;
            //                     //                                    continue;
            //                     //                                }
            //                     // TODO: iOS 12 has multiple LDR, keep last(first?)
            //                     uintptr_t bingo_insn = (uintptr_t)(insn - i);
            //                     uint64_t pc_ref = find_pc_rel_value_64((uint8_t*) text_text, sect->size, insn - i + 1, insn_ldr_imm_rn_64(insn - i));
            //                     //                                LOG("Found kernproc ldr at %#lx, instruction %#x, ref addr is %#llx", ((char*)(bingo_insn) - (char*)text_text), *(uint32_t*)bingo_insn, pc_ref);
            //                     uint64_t kernproc_ptr = physread64(dev_info->kernel_info.kPhysBase + sect->offset + pc_ref); // kernproc ptr
            //                     dev_info->kernel_info.kernproc = kernproc_ptr;
            //                     LOG("kernproc @ %#llx", kernproc_ptr);
            //                     break;
            //                 }
            //             }
            //             break;
            //         }
            //         sect++;
            //     }
            // }
            //            if (!strcmp(seg->segname, "__LINKEDIT")) {
            //                uint64_t off = 0x5a8000;
            //                off += dev_info->kPhysbase;
            //                LOG("fake vk %#llx linkedit vmaddr %llx off %#llx", fake_vkbase, seg->vmaddr, off);
            //                off -= gMappingBase;
            //                mach_vm_address_t ua = map_at_offset(off, seg->vmsize);
            //                //LOG("UA %#llx", *(uint64_t*)ua);
            //                //break;
            //                hexdump((void*)ua, seg->vmsize);
            //            }
            //        }
            // linkedit is bzero'd unless keepsyms=1 so this is useless
            //        if (lc->cmd == LC_SYMTAB)
            //        {
            //            struct symtab_command* sc = (struct symtab_command*)(kHeader + sizeof(struct mach_header_64) + lc_offset);
            //            LOG("symtab in the house! Number of symbols: %d, symt offset %#x", sc->nsyms, sc->symoff);
            //            struct nlist_64* sym_table = malloc(sc->nsyms * sizeof(struct nlist_64));
            //            char* strtab = malloc(sc->strsize);
            //
            //            int ret = physreadbuf(dev_info->kPhysbase + sc->symoff, sym_table, sc->nsyms * sizeof(struct nlist_64));
            //            LOG("ret 1 %d ", ret);
            //            hexdump(sym_table, 0x4000);
            //            ret = physreadbuf(dev_info->kPhysbase + sc->stroff, strtab, sc->strsize);
            //            LOG("ret 2 %d ", ret);
            //
            //            for(int i = 0; i < sc->nsyms; i++) {
            //                char* symbol_str = strtab + sym_table->n_un.n_strx;
            //                LOG("%x", sym_table->n_un.n_strx);
            //                if(!strcmp("_kernproc", symbol_str)) {
            //                    LOG("Found %s: %#llx", symbol_str, sym_table->n_value);
            //                    break;
            //                }
            //                sym_table++;
            //            }
        }
        lc_offset += lc->cmdsize;
    }
    return RET_SUCCESS;
}

bool isKernelHeader(mach_vm_address_t mapping_addr) {
    struct mach_header_64* mh = (struct mach_header_64*)mapping_addr;
    if (mh->magic == MH_MAGIC_64 && mh->filetype == MH_EXECUTE && mh->flags == (MH_PIE | MH_NOUNDEFS)){
        return true;
    }
    return false;
}
