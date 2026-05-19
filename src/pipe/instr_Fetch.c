/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
 **************************************************************************/

#include "hw_elts.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

extern machine_t guest;
extern uint64_t F_PC;

/*
 * Select PC logic.
 */
static comb_logic_t
select_PC(uint64_t pred_PC,                  
          opcode_t D_opcode, uint64_t val_a, 
          uint64_t D_seq_succ,               
          opcode_t M_opcode, bool M_cond_val, 
          uint64_t seq_succ,                  
          uint64_t *current_PC) {

    if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR) {
        *current_PC = 0;
    } else if (M_opcode == OP_B_COND && !M_cond_val) {
        *current_PC = seq_succ;
    }
    #ifdef EC
     else if ((M_opcode == OP_CBZ || M_opcode == OP_CBNZ) && !M_cond_val) {
        *current_PC = seq_succ;
    }
    #endif 
    else if (D_opcode == OP_RET && val_a != RET_FROM_MAIN_ADDR) {
        *current_PC = val_a;
    }
#ifdef EC
    else if (D_opcode == OP_BR || D_opcode == OP_BLR) {
        *current_PC = val_a;
    }
#endif
    else {
        *current_PC = pred_PC;
    }
    return;
}

/*
 * Predict PC logic.
 */
static comb_logic_t predict_PC(uint64_t current_PC, uint32_t insnbits,
                               opcode_t op, uint64_t *predicted_PC,
                               uint64_t *seq_succ) {
    /* * IMPORTANT: We must set these values BEFORE returning.
     * This ensures that when current_PC is 0, predicted_PC becomes 4.
     */
    *seq_succ = current_PC + 4;
    
    if (op == OP_B || op == OP_BL) {
        int64_t imm26 = bitfield_s64(insnbits, 0, 26) << 2;
        *predicted_PC = current_PC + imm26;
    }
    else if (op == OP_B_COND) {
        int64_t imm19 = bitfield_s64(insnbits, 5, 19) << 2;
        *predicted_PC = current_PC + imm19;
    }
    #ifdef EC
    else if (op == OP_CBZ || op == OP_CBNZ) {
        int64_t imm19 = bitfield_s64(insnbits, 5, 19) << 2;
        *predicted_PC = current_PC + imm19;
    }
    #endif 
    else {
        *predicted_PC = current_PC + 4;
    }

    if (!current_PC) {
        return; 
    }
        
    return;
}

static void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
    if (*op == OP_UBFM) {
        uint32_t imms = bitfield_u32(insnbits, 10, 6);
        *op = (imms == 63) ? OP_LSR_RI : OP_LSL_RI;
    }
    if (*op == OP_UBFMV) {
        uint32_t imms = bitfield_u32(insnbits, 10, 6);
        *op = (imms == 9) ? OP_LSR_RR : OP_LSL_RR;
    }
    
    uint32_t rd = bitfield_u32(insnbits, 0, 5);
    if (rd == 31) {
        if (*op == OP_SUBS_RR) *op = OP_CMP_RR;
        if (*op == OP_ADDS_RR) *op = OP_CMN_RR;
        if (*op == OP_ANDS_RR) *op = OP_TST_RR;
    }

    
#ifdef EC
// The different conditional select instructions have a shared encoding, so we need to check the condition code to determine the correct opcode.
    if (*op == OP_CSEL) {
        uint32_t op_bit = bitfield_u32(insnbits, 10, 1);
        if (op_bit) {
            *op = OP_CSINC;
        }
    }

    if (*op == OP_CSNEG) {
        uint32_t op_bit = bitfield_u32(insnbits, 10, 1);
        if (!op_bit) {
            *op = OP_CSINV;
        }
    }
#endif
}

/*
 * Fetch stage logic.
 */
comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
    bool imem_err = false;
    uint64_t current_PC = 0;
    uint64_t predicted_PC = F_PC;
    uint64_t seq_succ = 0;

    select_PC(in->pred_PC,
              X_out->op, X_out->val_a,
              X_out->multipurpose_val.seq_succ_PC,
              M_out->op, M_out->cond_holds,
              M_out->multipurpose_val.seq_succ_PC,
              &current_PC);                

    if (!current_PC || F_in->status == STAT_HLT) {
        out->insnbits = 0xD4400000U;
        out->op = OP_HLT;
        out->print_op = OP_HLT;
        out->format = ftable[out->op];
        imem_err = false;
    } else {
        imem(current_PC, &(out->insnbits), &imem_err);
        out->op = itable[bitfield_u32(out->insnbits, 21, 11)];
        if (out->op == OP_ERROR) {
            out->print_op = OP_ERROR;
            out->format = FORMAT_ERROR;
        } else {
            fix_instr_aliases(out->insnbits, &(out->op));
            out->print_op = out->op;
            out->format = ftable[out->op];
        }

        predict_PC(current_PC, out->insnbits, 
                    out->op, &predicted_PC,
                    &seq_succ);

        if (out->op == OP_ADRP) {
            out->multipurpose_val.adrp_val = current_PC & ~0xFFFUL;
        } else {
            out->multipurpose_val.seq_succ_PC = seq_succ;
        }

        F_PC = predicted_PC;
    }

    if (imem_err || out->op == OP_ERROR) {
        in->status = STAT_INS;
        F_in->status = in->status;
    } else if (out->op == OP_HLT) {
        in->status = STAT_HLT;
        F_in->status = in->status;
    } else {
        in->status = STAT_AOK;
    }
    out->status = in->status;

    return;
}