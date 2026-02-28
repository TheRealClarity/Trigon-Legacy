//
//  patchfinder.h
//  Trigon-Legacy
//

#ifndef patchfinder_h
#define patchfinder_h

#include <stdio.h>
#include <stdint.h>
#include <memory.h>

int insn_is_ldr_imm_64(uint32_t*);
uint64_t find_pc_rel_value_64(uint8_t*, size_t, uint32_t*, int);
int insn_ldr_imm_rn_64(uint32_t*);

#endif /* patchfinder_h */
