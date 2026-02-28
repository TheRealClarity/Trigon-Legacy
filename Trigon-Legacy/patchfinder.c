//
//  patchfinder.c
//  Created by Vladimir Putin on 14.12.16.
//  Fixes by Alex Hude and Max Bazaliy
//  Some parts of code from Luca Todesco and Pangu
//  Some parts by kok3shi and Clarity

#include "patchfinder.h"

static uint32_t* find_next_insn_matching_64(uint64_t region, uint8_t* kdata, size_t ksize, uint32_t* current_instruction, int (*match_func)(uint32_t*))
{
    while((uintptr_t)current_instruction < (uintptr_t)kdata + ksize - 4) {
        current_instruction++;
        
        if(match_func(current_instruction)) {
            return current_instruction;
        }
    }
    
    return NULL;
}

static uint32_t* find_prev_insn_matching_64(uint8_t* kdata, uint32_t* current_instruction, int (*match_func)(uint32_t*))
{
    //just do it
    while((uintptr_t)current_instruction > (uintptr_t)kdata) {
        current_instruction--;
        
        if(match_func(current_instruction)) {
            return current_instruction;
        }
    }
    
    return NULL;
}

static int insn_is_cmp_64(uint32_t* i)
{
    /* 0x2100001F */
    if ((*i & 0x2100001F) == 0x2100001F)
        return 1;
    else
        return 0;
}

static int insn_is_cbnz_w32(uint32_t* i)
{
    return (*i >> 24 == 0x35);
}

static int insn_is_orr_w32(uint32_t* i)
{
    return (*i >> 24 == 0x32);
}

static int insn_is_ret(uint32_t* i)
{
    if (*i == 0xd65f03c0)
        return 1;
    
    return 0;
}

__unused static uint32_t bit_range_64(uint32_t x, int start, int end)
{
    x = (x << (31 - start)) >> (31 - start);
    x = (x >> end);
    return x;
}

__unused static uint64_t real_signextend_64(uint64_t imm, uint8_t bit)
{
    if ((imm >> bit) & 1) {
        return (-1LL << (bit + 1)) + imm;
    } else
        return imm;
}

__unused static uint64_t signextend_64(uint64_t imm, uint8_t bit)
{
    return real_signextend_64(imm, bit - 1);
    /*
     if ((imm >> bit) & 1)
     return (uint64_t)(-1) - (~((uint64_t)1 << bit)) + imm;
     else
     return imm;
     */
}

__unused static int insn_is_mov_reg64(uint32_t* i)
{
    return (*i & 0x7FE003E0) == 0x2A0003E0;
}

__unused static int insn_mov_reg_rt64(uint32_t* i)
{
    return (*i >> 16) & 0x1F;
}

__unused static int insn_mov_reg_rd64(uint32_t* i)
{
    return *i & 0x1F;
}

__unused static int insn_is_movz_64(uint32_t* i)
{
    return (*i & 0x7F800000) == 0x52800000;
}

__unused static int insn_movz_rd_64(uint32_t* i)
{
    return *i & 0x1F;
}

__unused static int insn_is_mov_imm_64(uint32_t* i)
{
    if ((*i & 0x7f800000) == 0x52800000)
        return 1;
    
    return 0;
}

static int insn_is_movz_x0_0(uint32_t *i)
{
    if(*i == 0xd2800000){
        return 1;
    }
    return 0;
}


__unused static int insn_mov_imm_rd_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static uint32_t insn_mov_imm_imm_64(uint32_t* i)
{
    return bit_range_64(*i, 20, 5);
}

__unused static uint32_t insn_movz_imm_64(uint32_t* i)
{
    return bit_range_64(*i, 20, 5);
}

__unused static int insn_is_ldr_literal_64(uint32_t* i)
{
    // C6.2.84 LDR (literal) LDR Xt
    if ((*i & 0xff000000) == 0x58000000)
        return 1;
    
    // C6.2.84 LDR (literal) LDR Wt
    if ((*i & 0xff000000) == 0x18000000)
        return 1;
    
    // C6.2.95 LDR (literal) LDRSW Xt
    if ((*i & 0xff000000) == 0x98000000)
        return 1;
    
    return 0;
}

__unused static int insn_nop_64(uint32_t *i)
{
    return (*i == 0xD503201F);
}

__unused static int insn_add_reg_rm_64(uint32_t* i)
{
    return ((*i >> 16) & 0x1f);
}

__unused static int insn_ldr_literal_rt_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static uint64_t insn_ldr_literal_imm_64(uint32_t* i)
{
    uint64_t imm = (*i & 0xffffe0) >> 3;
    return signextend_64(imm, 21);
}

__unused static uint64_t insn_adr_imm_64(uint32_t* i)
{
    uint64_t immhi = bit_range_64(*i, 23, 5);
    uint64_t immlo = bit_range_64(*i, 30, 29);
    uint64_t imm = (immhi << 2) + (immlo);
    return signextend_64(imm, 19+2);
}

__unused static uint64_t insn_adrp_imm_64(uint32_t* i)
{
    uint64_t immhi = bit_range_64(*i, 23, 5);
    uint64_t immlo = bit_range_64(*i, 30, 29);
    uint64_t imm = (immhi << 14) + (immlo << 12);
    return signextend_64(imm, 19+2+12);
}

__unused static int insn_is_adrp_64(uint32_t* i)
{
    if ((*i & 0x9f000000) == 0x90000000) {
        return 1;
    }
    
    return 0;
}

__unused static int insn_adrp_rd_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_is_mov_bitmask(uint32_t* i)
{
    return (*i & 0x7F8003E0) == 0x320003E0;
}

__unused static int insn_mov_bitmask_rd(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_is_add_imm_64(uint32_t* i)
{
    if ((*i & 0x7f000000) == 0x11000000)
        return 1;
    
    return 0;
}

__unused static int insn_add_imm_rd_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_add_imm_rn_64(uint32_t* i)
{
    return ((*i >> 5) & 0x1f);
}

__unused static uint64_t insn_add_imm_imm_64(uint32_t* i)
{
    uint64_t imm = bit_range_64(*i, 21, 10);
    if (((*i >> 22) & 3) == 1)
        imm = imm << 12;
    return imm;
}

__unused static int insn_add_reg_rd_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_add_reg_rn_64(uint32_t* i)
{
    return ((*i >> 5) & 0x1f);
}

__unused static int insn_is_add_reg_64(uint32_t* i)
{
    if ((*i & 0x7fe00c00) == 0x0b200000)
        return 1;
    
    return 0;
}

__unused static int insn_is_adr_64(uint32_t* i)
{
    if ((*i & 0x9f000000) == 0x10000000)
        return 1;
    
    return 0;
}

__unused static int insn_adr_rd_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_is_bl_64(uint32_t* i)
{
    if ((*i & 0xfc000000) == 0x94000000)
        return 1;
    else
        return 0;
}

__unused static int insn_is_strb(uint32_t* i)
{
    // TODO: more encodings
    return (*i >> 24 == 0x39);
}

__unused static int insn_rt_strb(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_rn_strb(uint32_t* i)
{
    return ((*i >> 5) & 0x1f);
}

__unused static int insn_strb_imm12(uint32_t* i)
{
    return ((*i >> 10) & 0xfff);
}

__unused static int insn_is_br_64(uint32_t *i)
{
    if ((*i & 0xfffffc1f) == 0xd61f0000)
        return 1;
    else
        return 0;
}

__unused static int insn_br_reg_xn_64(uint32_t *i)
{
    if ((*i & 0xfffffc1f) != 0xd61f0000)
        return 0;
    return (*i >> 5) & 0x1f;
}

__unused static uint64_t insn_bl_imm32_64(uint32_t* i)
{
    uint64_t imm = (*i & 0x3ffffff) << 2;
    //PFExtLog("imm = %llx\n", imm);
    // sign extend
    uint64_t res = real_signextend_64(imm, 27);
    
    //PFExtLog("real_signextend_64 = %llx\n", res);
    
    return res;
}

__unused static uint64_t insn_mov_bitmask_imm_64(uint32_t* i)
{
    // Extract the N, imms, and immr fields.
    uint32_t N = (*i >> 22) & 1;
    uint32_t immr = bit_range_64(*i, 21, 16);
    uint32_t imms = bit_range_64(*i, 15, 10);
    uint32_t j;
    
    int len = 31 - __builtin_clz((N << 6) | (~imms & 0x3f));
    
    uint32_t size = (1 << len);
    uint32_t R = immr & (size - 1);
    uint32_t S = imms & (size - 1);
    
    uint64_t pattern = (1ULL << (S + 1)) - 1;
    for (j = 0; j < R; ++j)
        pattern = ((pattern & 1) << (size-1)) | (pattern >> 1); // ror
    
    return pattern;
}

__unused int insn_is_funcbegin_64(uint32_t* i)
{
    if (*i == 0xa9bf7bfd)
        return 1;
    if (*i == 0xa9bc5ff8)
        return 1;
    if (*i == 0xa9bd57f6)
        return 1;
    if (*i == 0xa9ba6ffc)
        return 1;
    if (*i == 0xa9bb67fa)
        return 1;
    if (*i == 0xa9be4ff4)
        return 1;
    return 0;
}

__unused static int insn_is_tbz(uint32_t* i)
{
    return ((*i >> 24) & 0x7f) == 0x36;
}

__unused static int insn_is_tbnz(uint32_t* i)
{
    return ((*i >> 24) & 0x7f) == 0x37;
}

__unused static int insn_is_tbnz_w32(uint32_t* i)
{
    return (*i >> 24 == 0x37);
}

__unused static int insn_is_cbz_w32(uint32_t* i)
{
    return (*i >> 24 == 0x34);
}

__unused static int insn_is_cbz_x64(uint32_t* i)
{
    return (*i >> 24 == 0xb4);
}

__unused static int insn_is_cbz_64(uint32_t* i)
{
    return ((*i >> 24) & 0x7f) == 0x34;
}

__unused static int insn_is_mrs_from_TPIDR_EL1(uint32_t* i)
{
    // op0 op1  CRn  CRm op2
    //  11 000 1101 0000 100
    //
    return ((*i & 0xFFFFFFF0) == 0xD538D080);
}

// search back for memory with step 4 bytes
__unused static uint32_t * memmem_back_64(uint32_t *ptr1, uint64_t max_count, const uint8_t *ptr2, size_t num)
{
    for ( uint64_t i = 0; i < max_count >> 2; ++i ) {
        if ( !memcmp(ptr1, ptr2, num) )
            return ptr1;
        --ptr1;
    }
    return 0;
}

__unused static int insn_ldr_imm_rt_64(uint32_t* i)
{
    return (*i & 0x1f);
}

__unused static int insn_is_b_conditional_64(uint32_t* i)
{
    if ((*i & 0xff000010) == 0x54000000)
        return 1;
    else
        return 0;
}

__unused static int insn_is_b_unconditional_64(uint32_t* i)
{
    if ((*i & 0xfc000000) == 0x14000000)
        return 1;
    else
        return 0;
}

__unused int insn_ldr_imm_rn_64(uint32_t* i)
{
    return ((*i >> 5) & 0x1f);
}

__unused int insn_is_ldr_imm_64(uint32_t* i)
{
    // C6.2.83 LDR (immediate) Post-index
    if ((*i & 0xbfe00c00) == 0xb8400400)
        return 1;
    // C6.2.83 LDR (immediate) Pre-index
    if ((*i & 0xbfe00c00) == 0xb8400c00)
        return 1;
    // C6.2.83 LDR (immediate) Unsigned offset
    if ((*i & 0xbfc00000) == 0xb9400000)
        return 1;
    //------------------------------------//
    
    // C6.2.86 LDRB (immediate) Post-index
    if ((*i & 0xbfe00c00) == 0x38400400)
        return 1;
    // C6.2.86 LDRB (immediate) Pre-index
    if ((*i & 0xbfe00c00) == 0x38400c00)
        return 1;
    // C6.2.86 LDRB (immediate) Unsigned offset
    if ((*i & 0xbfc00000) == 0x39400000)
        return 1;
    //------------------------------------//
    
    // C6.2.90 LDRSB (immediate) Post-index
    if ((*i * 0xbfa00c00) == 0x38800400)
        return 1;
    // C6.2.90 LDRSB (immediate) Pre-index
    if ((*i * 0xbfa00c00) == 0x38800c00)
        return 1;
    // C6.2.90 LDRSB (immediate) Unsigned offset
    if ((*i * 0xbf800000) == 0x39800000)
        return 1;
    //------------------------------------//
    
    // C6.2.88 LDRH (immediate) Post-index
    if ((*i * 0xbfe00c00) == 0x78400c00)
        return 1;
    // C6.2.88 LDRH (immediate) Pre-index
    if ((*i * 0xbfe00c00) == 0x78400c00)
        return 1;
    // C6.2.88 LDRH (immediate) Unsigned offset
    if ((*i * 0xbfc00000) == 0x79400000)
        return 1;
    //------------------------------------//
    
    // C6.2.92 LDRSH (immediate) Post-index
    if ((*i * 0xbfa00c00) == 0x78800c00)
        return 1;
    // C6.2.92 LDRSH (immediate) Pre-index
    if ((*i * 0xbfa00c00) == 0x78800c00)
        return 1;
    // C6.2.92 LDRSH (immediate) Unsigned offset
    if ((*i * 0xbf800000) == 0x79800000)
        return 1;
    //------------------------------------//
    
    
    // C6.2.94 LDRSW (immediate) Post-index
    if ((*i * 0xbfe00c00) == 0xb8800400)
        return 1;
    // C6.2.94 LDRSW (immediate) Pre-index
    if ((*i * 0xbfe00c00) == 0xb8800c00)
        return 1;
    // C6.2.94 LDRSW (immediate) Unsigned offset
    if ((*i * 0xbfc00000) == 0xb9800000)
        return 1;
    
    return 0;
}

// TODO: other encodings
__unused static uint64_t insn_ldr_imm_imm_64(uint32_t* i)
{
    uint64_t imm;
    // C6.2.83 LDR (immediate) Post-index
    if ((*i & 0xbfe00c00) == 0xb8400400)
    {
        imm = bit_range_64(*i, 20, 12);
        return signextend_64(imm, 9);
    }
    
    // C6.2.83 LDR (immediate) Pre-index
    if ((*i & 0xbfe00c00) == 0xb8400c00)
    {
        imm = bit_range_64(*i, 20, 12);
        return signextend_64(imm, 9);
    }
    
    // C6.2.83 LDR (immediate) Unsigned offset
    if ((*i & 0xbfc00000) == 0xb9400000)
    {
        imm = bit_range_64(*i, 21, 10);
        if ((*i >> 30) & 1) // LDR X
            return imm * 8;
        else
            return imm * 4;
    }
    
    //PFLog("Warning! Unsupported encoding or not LDR instruction is passed!\n");
    
    return 0;
}

// calculate value (if possible) of register before specific instruction
uint64_t find_pc_rel_value_64(uint8_t* kdata, size_t ksize, uint32_t* last_insn, int reg)
{
    int found = 0;
    uint32_t* current_instruction = last_insn;
    while((uintptr_t)current_instruction > (uintptr_t)kdata)
    {
        current_instruction--;
        
        if(insn_is_mov_imm_64(current_instruction) && insn_mov_imm_rd_64(current_instruction) == reg)
        {
            found = 1;
            break;
        }
        
        if(insn_is_ldr_literal_64(current_instruction) && insn_ldr_literal_rt_64(current_instruction) == reg)
        {
            found = 1;
            break;
        }
        
        if (insn_is_adrp_64(current_instruction) && insn_adrp_rd_64(current_instruction) == reg)
        {
            found = 1;
            break;
        }
        
        if (insn_is_adr_64(current_instruction) && insn_adr_rd_64(current_instruction) == reg)
        {
            found = 1;
            break;
        }
    }
    if(!found)
        return 0;
    uint64_t value = 0;
    while((uintptr_t)current_instruction < (uintptr_t)last_insn)
    {
        if(insn_is_mov_imm_64(current_instruction) && insn_mov_imm_rd_64(current_instruction) == reg)
        {
            value = insn_mov_imm_imm_64(current_instruction);
            //PFExtLog("%s:%d mov (immediate): value is reset to %#llx\n", __func__, __LINE__, value);
        }
        else if(insn_is_ldr_literal_64(current_instruction) && insn_ldr_literal_rt_64(current_instruction) == reg)
        {
            value = *(uint64_t*)((uintptr_t)current_instruction + insn_ldr_literal_imm_64(current_instruction));
            //PFExtLog("%s:%d ldr (literal): value is reset to %#llx\n", __func__, __LINE__, value);
        }
        else if (insn_is_ldr_imm_64(current_instruction) && insn_ldr_imm_rn_64(current_instruction) == reg)
        {
            value += insn_ldr_imm_imm_64(current_instruction);
            //PFExtLog("%s:%d ldr (immediate): value = %#llx\n", __func__, __LINE__, value);
        }
        if (insn_is_adrp_64(current_instruction) && insn_adrp_rd_64(current_instruction) == reg)
        {
            value = ((((uintptr_t)current_instruction - (uintptr_t)kdata) >> 12) << 12) + insn_adrp_imm_64(current_instruction);
            //PFExtLog("%s:%d adrp: value is reset to %#llx\n", __func__, __LINE__, value);
        }
        else if (insn_is_adr_64(current_instruction) && insn_adr_rd_64(current_instruction) == reg)
        {
            value = (uintptr_t)current_instruction - (uintptr_t)kdata + insn_adr_imm_64(current_instruction);
            //PFExtLog("%s:%d adr: value is reset to %#llx\n", __func__, __LINE__, value);
        }
        else if(insn_is_add_reg_64(current_instruction) && insn_add_reg_rd_64(current_instruction) == reg)
        {
            if (insn_add_reg_rm_64(current_instruction) != 15 || insn_add_reg_rn_64(current_instruction) != reg)
            {
                //PFExtLog("%s:%d add (register): unknown source register, value is reset to 0\n", __func__, __LINE__);
                return 0;
            }
            
            value += ((uintptr_t)current_instruction - (uintptr_t)kdata) + 4;
            //PFExtLog("%s:%d add: PC register, value = %#llx\n", __func__, __LINE__, value);
        }
        else if (insn_is_add_imm_64(current_instruction) && insn_add_imm_rd_64(current_instruction) == reg)
        {
            if (insn_add_imm_rn_64(current_instruction) != reg)
            {
                //PFExtLog("%s:%d add (immediate): unknown source register, value is reset to 0\n", __func__, __LINE__);
                return 0;
            }
            value += insn_add_imm_imm_64(current_instruction);
            //PFExtLog("%s:%d add (immediate): value = %#llx\n", __func__, __LINE__, value);
        }
        
        current_instruction++;
    }
    //PFExtLog("%s:%d FINAL value = %#llx\n", __func__, __LINE__, value);
    
    return value;
}

static uint32_t* find_literal_ref_64(uint64_t region, uint8_t* kdata, size_t ksize, uint32_t* insn, uint64_t address)
{
    uint32_t* current_instruction = insn;
    uint64_t registers[32];
    memset(registers, 0, sizeof(registers));
    
    while((uintptr_t)current_instruction < (uintptr_t)(kdata + ksize))
    {
        if (insn_is_mov_imm_64(current_instruction))
        {
            int reg = insn_mov_imm_rd_64(current_instruction);
            uint64_t value = insn_mov_imm_imm_64(current_instruction);
            registers[reg] = value;
            //PFExtLog("%s:%d mov (immediate): reg[%d] is reset to %#llx\n", __func__, __LINE__, reg, value);
        }
        else if (insn_is_ldr_literal_64(current_instruction))
        {
            uintptr_t literal_address  = (uintptr_t)current_instruction + (uintptr_t)insn_ldr_literal_imm_64(current_instruction);
            if(literal_address >= (uintptr_t)kdata && (literal_address + 4) <= ((uintptr_t)kdata + ksize))
            {
                int reg = insn_ldr_literal_rt_64(current_instruction);
                uint64_t value =  *(uint64_t*)(literal_address);
                registers[reg] = value;
                //PFExtLog("%s:%d ldr (literal): reg[%d] is reset to %#llx\n", __func__, __LINE__, reg, value);
            }
        }
        else if (insn_is_adrp_64(current_instruction))
        {
            int reg = insn_adrp_rd_64(current_instruction);
            uint64_t value = ((((uintptr_t)current_instruction - (uintptr_t)kdata) >> 12) << 12) + insn_adrp_imm_64(current_instruction);
            registers[reg] = value;
            //PFExtLog("%s:%d adrp: reg[%d] is reset to %#llx\n", __func__, __LINE__, reg, value);
        }
        else if (insn_is_adr_64(current_instruction))
        {
            uint64_t value = (uintptr_t)current_instruction - (uintptr_t)kdata + insn_adr_imm_64(current_instruction);
            if (value == address)
            {
                //PFExtLog("%s:%d FINAL pointer is %#llx\n", __func__, __LINE__, (uint64_t)current_instruction - (uint64_t)kdata);
                return current_instruction;
            }
        }
        else if(insn_is_add_reg_64(current_instruction))
        {
            int reg = insn_add_reg_rd_64(current_instruction);
            if(insn_add_reg_rm_64(current_instruction) == 15 && insn_add_reg_rn_64(current_instruction) == reg)
            {
                uint64_t value = ((uintptr_t)current_instruction - (uintptr_t)kdata) + 4;
                registers[reg] += value;
                //PFExtLog("%s:%d adrp: reg[%d] += %#llx\n", __func__, __LINE__, reg, value);
                if(registers[reg] == address)
                {
                    //PFExtLog("%s:%d FINAL pointer is %#llx\n", __func__, __LINE__, (uint64_t)current_instruction - (uint64_t)kdata);
                    return current_instruction;
                }
            }
        }
        else if (insn_is_add_imm_64(current_instruction))
        {
            int reg = insn_add_imm_rd_64(current_instruction);
            if (insn_add_imm_rn_64(current_instruction) == reg)
            {
                uint64_t value = insn_add_imm_imm_64(current_instruction);
                registers[reg] += value;
                //PFExtLog("%s:%d adrp: reg[%d] += %#llx\n", __func__, __LINE__, reg, value);
                if (registers[reg] == address)
                {
                    //PFExtLog("%s:%d FINAL pointer is %#llx\n", __func__, __LINE__, (uint64_t)current_instruction - (uint64_t)kdata);
                    return current_instruction;
                }
            }
            
        }
        
        current_instruction++;
    }
    
    //PFExtLog("%s:%d FINAL pointer is NULL\n", __func__, __LINE__);
    return NULL;
}

// search next instruction, decrementing mode
static uint32_t* find_last_insn_matching_64(uint64_t region, uint8_t* kdata, size_t ksize, uint32_t* current_instruction, int (*match_func)(uint32_t*))
{
    while((uintptr_t)current_instruction > (uintptr_t)kdata) {
        current_instruction--;
        
        if(match_func(current_instruction)) {
            return current_instruction;
        }
    }
    
    return NULL;
}

static uint64_t find_next_insn_bl_64(uint64_t region, uint8_t* kdata, size_t ksize, uint64_t sb)
{
    uint32_t* fn_start = (uint32_t*)(kdata+(sb-region));
    if(!fn_start) return 0;
    
    uint32_t *insn = find_next_insn_matching_64(region, kdata, ksize, fn_start, insn_is_bl_64);
    if(!insn) return 0;
    
    return (uint64_t)insn - (uintptr_t)kdata + 4;
}

static uint64_t find_next_next_insn_bl_64(uint64_t region, uint8_t* kdata, size_t ksize, uint64_t sb)
{
    uint32_t* fn_start = (uint32_t*)(kdata+(sb-region));
    if(!fn_start) return 0;
    
    uint32_t *insn = find_next_insn_matching_64(region, kdata, ksize, fn_start, insn_is_bl_64);
    if(!insn) return 0;
    
    insn = find_next_insn_matching_64(region, kdata, ksize, insn, insn_is_bl_64);
    if(!insn) return 0;
    
    return (uint64_t)insn - (uintptr_t)kdata + 4;
}
