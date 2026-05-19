/**************************************************************************
 * C S 429 system emulator
 *
 * hw_elts.c - Module for emulating hardware elements.
 *
 * Copyright (c) 2022, 2023, 2024, 2025.
 * Authors: S. Chatterjee, Z. Leeper., P. Jamadagni, W. Borden.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/

#include "hw_elts.h"
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "mem.h"
#include <assert.h>
#include <stdint.h>

extern machine_t guest;

fa_t rca[64];

/*
 * Read from instruction memory. Students, do not modify.
 */
comb_logic_t imem(uint64_t imem_addr, uint32_t *imem_rval, bool *imem_err) {
    // imem_addr must be in "instruction memory" and a multiple of 4
    *imem_err = (!addr_in_imem(imem_addr) || (imem_addr & 0x3U));
    *imem_rval = (uint32_t) mem_read_I(imem_addr);
}

/*
 * Sets up the inputs to the ripple carry adder.
 * STUDENT TO-DO:
 */
comb_logic_t init_rca(uint64_t val_a, uint64_t val_b, bool c_in) {
    // Initialize 64 full-adder stages with input operands and initial carry-in.
    for (uint64_t i = 0; i < 64; i++) {
        rca[i].bit_a = (val_a >> i) & 0x1;
        rca[i].bit_b = (val_b >> i) & 0x1;
        rca[i].c_in = false;
        rca[i].c_out = false;
        rca[i].s = false;
    }
    rca[0].c_in = c_in;
    return;
}

/*
 * Performs the ripple carry add.
 * STUDENT TO-DO:
 */
comb_logic_t ripple_carry_add(uint64_t *sum) {
    *sum = 0;

    bool a0 = rca[0].bit_a;
    bool b0 = rca[0].bit_b;
    bool cin0 = rca[0].c_in;
    rca[0].s = (a0 ^ b0) ^ cin0;
    rca[0].c_out = (a0 & b0) | (a0 & cin0) | (b0 & cin0);
    *sum |= ((uint64_t) rca[0].s) << 0;

    for (uint64_t i = 1; i < 64; i++) {
        rca[i].c_in = rca[i - 1].c_out;
        bool a = rca[i].bit_a;
        bool b = rca[i].bit_b;
        bool cin = rca[i].c_in;
        rca[i].s = (a ^ b) ^ cin;
        rca[i].c_out = (a & b) | (a & cin) | (b & cin);
        *sum |= ((uint64_t) rca[i].s) << i;
    }
    return;
}

/*
 * Read from register file.
 * STUDENT TO-DO:
 * Read from src1 and src2 registers. Take extra care for SP/XZR.
 */
comb_logic_t regfile_read(uint8_t src1, uint8_t src2, uint64_t *val_a,
                          uint64_t *val_b) {
    /* your implementation */
    //src 1
    if (src1 == 31) {
        *val_a = guest.proc->SP;
    } else if (src1 > 31) {
        *val_a = 0;
    } else {
        *val_a = guest.proc->GPR[src1];
    }
    //src 2
    if (src2 == 31) {
        *val_b = guest.proc->SP;
    } else if (src2 > 31) {
        *val_b = 0;
    } else {
        *val_b = guest.proc->GPR[src2];
    }
    return;
}

/*
 * Write to register file.
 * STUDENT TO-DO:
 * Write to dst register if enabled. Take extra care for SP/XZR.
 */
comb_logic_t regfile_write(uint8_t dst, uint64_t val_w, bool w_enable) {
    if (!w_enable) {
        return;
    }
        if (dst == 31) {
            guest.proc->SP = val_w;
        } else if (dst > 31) {
            return;
        } else {
            guest.proc->GPR[dst] = val_w;
        }
    return;
}

/*
 * Check whether a condition is satisfied given the NCZV status flags.
 * STUDENT TO-DO:
 */
static bool cond_holds(cond_t cond, uint8_t flags) {
    bool N = (flags >> 3) & 1;
    bool Z = (flags >> 2) & 1;
    bool C = (flags >> 1) & 1;
    bool V = (flags >> 0) & 1;

    switch (cond) {
        case C_EQ: return Z;
        case C_NE: return !Z;

        case C_CS: return C;
        case C_CC: return !C;

        case C_MI: return N;
        case C_PL: return !N;

        case C_VS: return V;
        case C_VC: return !V;

        case C_HI: return C && !Z;
        case C_LS: return !C || Z;

        case C_GE: return N == V;
        case C_LT: return N != V;

        case C_GT: return !Z && (N == V);
        case C_LE: return Z || (N != V);

        case C_AL: return true;
        case C_NV: return true;
        case C_ERROR: return false;

        default: return false;
    }
}

/*
 * Perform the appropriate ALU operation, setting NZCV flags if needed.
 * STUDENT TO-DO:
 */
comb_logic_t alu(uint64_t alu_vala, uint64_t alu_valb, uint8_t alu_valhw,
                 uint8_t nzcv, alu_op_t ALUop, bool set_flags, cond_t cond,
                 uint64_t *val_e, bool *cond_val, uint8_t *nzcv_dst) {
    
    uint64_t res = 0;
    // Current flags (for cond_holds and potential pass-through)
    uint8_t n = (nzcv >> 3) & 1;
    uint8_t z = (nzcv >> 2) & 1;
    uint8_t c = (nzcv >> 1) & 1;
    uint8_t v = (nzcv >> 0) & 1;

    // To track if the specific operation updates C/V to 0 or calculates them
    bool logical_op = false;

    switch (ALUop) {
#ifdef EC
        case CSEL_OP:
            res = cond_holds(cond, nzcv) ? alu_vala : alu_valb;
            break;
        case CSINV_OP:
            res = cond_holds(cond, nzcv) ? alu_vala : ~alu_valb;
            break;
        case CSINC_OP:
            res = cond_holds(cond, nzcv) ? alu_vala : alu_valb + 1;
            break;
        case CSNEG_OP:
            res = cond_holds(cond, nzcv) ? alu_vala : -alu_valb;
            break;
        case CBZ_OP:
        case CBNZ_OP:
            res = alu_vala;
            break;
#endif
        case PLUS_OP:
            res = alu_vala + alu_valb;
            if (set_flags) {
                c = (res < alu_vala); 
                v = ((alu_vala >> 63) == (alu_valb >> 63)) && ((res >> 63) != (alu_vala >> 63));
            }
            break;

        case MINUS_OP:
            res = alu_vala - alu_valb;
            if (set_flags) {
                c = (alu_vala >= alu_valb); // C = !Borrow
                v = ((alu_vala >> 63) != (alu_valb >> 63)) && ((res >> 63) != (alu_vala >> 63));
            }
            break;

        case INV_OP:   res = alu_vala | (~alu_valb); logical_op = true; break;
        case OR_OP:    res = alu_vala | alu_valb;    logical_op = true; break;
        case EOR_OP:   res = alu_vala ^ alu_valb;    logical_op = true; break;
        case AND_OP:   res = alu_vala & alu_valb;    logical_op = true; break;
        
        case MOV_OP:   
            res = alu_vala | (alu_valb << alu_valhw); 
            break;

        case MOVK_OP: 
            // Mask out the 16-bit chunk at valhw, then OR in the new 16 bits
            res = (alu_vala & (~(0xFFFFUL << alu_valhw))) | (alu_valb << alu_valhw);
            break;

        case LSL_OP:   res = alu_vala << (alu_valb & 0x3FUL); break;
        case LSR_OP:   res = alu_vala >> (alu_valb & 0x3FUL); break;
        case ASR_OP:   
            // Cast to signed for arithmetic shift
            res = (uint64_t)((int64_t)alu_vala >> (alu_valb & 0x3FUL)); 
            break;

        case PASS_A_OP: res = alu_vala; break;

        default: res = 0; break;
    }

    // Common Flag Logic for N and Z
    if (set_flags) {
        n = (res >> 63) & 1;
        z = (res == 0);
        if (logical_op) {
            c = 0; // ARM logical instructions (ANDS) clear C and V
            v = 0;
        }
    }

    *val_e = res;
    
    
    if (set_flags) {
        *nzcv_dst = (n << 3) | (z << 2) | (c << 1) | v;
    } else {
        *nzcv_dst = nzcv;
    }
    bool done = false;
    #ifdef EC
    if (ALUop == CBZ_OP) {
        *cond_val = (res == 0);
    } else if (ALUop == CBNZ_OP) {
        *cond_val = (res != 0);
    } else {
        *cond_val = cond_holds(cond, *nzcv_dst);
    }
    done = true;
    #endif
    if (!done) {
        *cond_val = cond_holds(cond, *nzcv_dst);
    }

    return;
}

/*
 * Read from data memory, Students do not modify.
 */
comb_logic_t dmem(uint64_t dmem_addr, uint64_t dmem_wval, bool dmem_read,
                  bool dmem_write, uint64_t *dmem_rval, bool *dmem_err) {
    if (!dmem_read && !dmem_write) {
        return;
    }
    // dmem_addr must be in "data memory" and a multiple of 8
    *dmem_err = (!addr_in_dmem(dmem_addr) || (dmem_addr & 0x7U));
    if (is_special_addr(dmem_addr))
        *dmem_err = false;
    if (dmem_read)
        *dmem_rval = (uint64_t) mem_read_L(dmem_addr);
    if (dmem_write)
        mem_write_L(dmem_addr, dmem_wval);
}