/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Execute.c - Execute stage of instruction processing pipeline.
 **************************************************************************/

#include "hw_elts.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

extern machine_t guest;

extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

extern uint8_t X_nzcvval;
extern bool X_set_flags;

/*
 * Execute stage logic.
 * STUDENT TO-DO:
 * Implement the execute stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 *
 * You will need the following helper functions:
 * copy_m_ctl_signals, copy_w_ctl_signals, and alu.
 */
comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {
    // 1. Pass through basic metadata
    out->op = in->op;
    out->print_op = in->print_op;
    out->status = in->status;
    out->dst = in->dst;
    out->val_b = in->val_b; // Needed for STUR in Memory stage
    out->multipurpose_val.seq_succ_PC = in->multipurpose_val.seq_succ_PC;

    // 2. Select ALU Operands (Mux logic)
    // vala_sel: 0 for val_a, 1 for multipurpose_val (used for ADRP/BL)
    uint64_t val_a = in->X_sigs.vala_sel ? in->multipurpose_val.seq_succ_PC : in->val_a;
    // valb_sel: 0 for immediate, 1 for register
    uint64_t val_b = in->X_sigs.valb_sel ? in->val_b : (uint64_t)in->val_imm;

    // 3. Call ALU with the correct signature from your hw_elts.c
    /* Signature: alu(alu_vala, alu_valb, alu_valhw, nzcv, ALUop, set_flags, cond, 
                     *val_e, *cond_val, *nzcv_dst) */
    
    alu(val_a,           // alu_vala
        val_b,           // alu_valb
        in->val_hw,      // alu_valhw
        guest.proc->NZCV, // Current architectural flags
        in->ALU_op,      // ALUop
        in->X_sigs.set_flags, // set_flags signal
        in->cond,        // cond field for B_COND
        &out->val_ex,    // Output: ALU result
        &out->cond_holds,// Output: Result of condition check
        &X_nzcvval       // Output: Updated global flags
    );

    // X_set_flags is likely used for simulation tracing
    X_set_flags = in->X_sigs.set_flags;

    // 4. Copy Control Signals to next stage
    copy_m_ctl_sigs(&(out->M_sigs), &(in->M_sigs));
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));

    // 5. BL Special Case
    if (in->op == OP_BL) {
        // Link register gets the sequential PC
        out->val_ex = in->multipurpose_val.seq_succ_PC;
    }
    #ifdef EC
    if (in->op == OP_BLR) {
        // Link register gets the sequential PC
        out->val_ex = in->multipurpose_val.seq_succ_PC;
    }
    #endif

    return;
}