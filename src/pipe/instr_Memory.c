/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Memory.c - Memory stage of instruction processing pipeline.
 **************************************************************************/

#include "hw_elts.h"
#include "instr.h"
#include "instr_pipeline.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

extern uint64_t M_PC;

extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

/*
 * Memory stage logic.
 * STUDENT TO-DO:
 * Implement the memory stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 *
 * You will need the following helper functions:
 * copy_w_ctl_signals and dmem.
 */
comb_logic_t memory_instr(m_instr_impl_t *in, w_instr_impl_t *out) {
    bool dmem_err = false;
    uint64_t read_val = 0;

    M_PC = in->multipurpose_val.seq_succ_PC - 4;

    // 1. Pass through basic metadata
    out->op = in->op;
    out->print_op = in->print_op;
    out->dst = in->dst;
    out->val_ex = in->val_ex; // The ALU result
    out->status = in->status;

    // 2. Memory Access
    // dmem(address, write_data, read_enable, write_enable, &read_value, &error)
    dmem(in->val_ex, in->val_b, in->M_sigs.dmem_read, in->M_sigs.dmem_write, &read_val, &dmem_err);

    // 3. Update Status on Memory Error
    if (dmem_err) {
        out->status = STAT_ADR;
    }
    
    // Store the value read from memory (will be 0 if not a LDUR)
    out->val_mem = read_val;

    // 4. Copy Control Signals to the Writeback stage
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));

    // 5. Special Case: HLT/Bubble Protection
    // If the instruction already has an error or is a HLT, ensure we don't 
    // allow side effects in the next stage (though dmem usually handles this).
    if (out->status != STAT_AOK) {
        out->W_sigs.w_enable = false;
    }

    return;
}