/**************************************************************************
 * C S 429 system emulator
 *
 * Bubble and stall checking logic.
 * STUDENT TO-DO:
 * Implement logic for hazard handling.
 *
 * handle_hazards is called from proc.c with the appropriate
 * parameters already set, you must implement the logic for it.
 *
 * You may optionally use the other three helper functions to
 * make it easier to follow your logic.
 **************************************************************************/

#include "hazard_control.h"
#include "hw_elts.h"
#include "machine.h"

extern machine_t guest;
extern mem_status_t dmem_status;
extern bool deassert_flags;

/* Use this method to actually bubble/stall a pipeline stage.
 * Call it in handle_hazards(). Do not modify this code. */
void pipe_control_stage(proc_stage_t stage, bool bubble, bool stall) {
    pipe_reg_t *pipe;
    switch (stage) {
        case S_FETCH:
            pipe = F_instr;
            break;
        case S_DECODE:
            pipe = D_instr;
            break;
        case S_EXECUTE:
            pipe = X_instr;
            break;
        case S_MEMORY:
            pipe = M_instr;
            break;
        case S_WBACK:
            pipe = W_instr;
            break;
        default:
            printf("Error: incorrect stage provided to pipe control.\n");
            return;
    }
    if (bubble && stall) {
        printf("Error: cannot bubble and stall at the same time.\n");
        pipe->ctl = P_ERROR;
    }
    // If we were previously in an error state, stay there.
    if (pipe->ctl == P_ERROR)
        return;

    if (bubble) {
        pipe->ctl = P_BUBBLE;
    } else if (stall) {
        pipe->ctl = P_STALL;
    } else {
        pipe->ctl = P_LOAD;
    }
}


bool check_ret_hazard(opcode_t D_opcode) {
    return D_opcode == OP_RET;
}

#ifdef EC
bool check_br_hazard(opcode_t D_opcode) {
    return D_opcode == OP_BR || D_opcode == OP_BLR;
}

bool check_cb_hazard(opcode_t D_opcode, uint64_t D_val_a) {
    return (D_opcode == OP_CBZ && D_val_a == 0) ||
           (D_opcode == OP_CBNZ && D_val_a != 0);
}
#endif

bool check_mispred_branch_hazard(opcode_t X_opcode, bool X_condval) {
    return (X_opcode == OP_B_COND && !X_condval)
#ifdef EC 
    || ((X_opcode == OP_CBZ || X_opcode == OP_CBNZ) && !X_condval)
#endif
        ;
}

bool check_load_use_hazard(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                           opcode_t X_opcode, uint8_t X_dst) {
    return X_opcode == OP_LDUR && X_dst != XZR_NUM &&
           (X_dst == D_src1 || X_dst == D_src2);
}

bool error(stat_t status) {
    return status == STAT_HLT || status == STAT_ADR || status == STAT_INS;
}

comb_logic_t handle_hazards(opcode_t D_opcode, uint8_t D_src1, uint8_t D_src2,
                            uint64_t D_val_a, opcode_t X_opcode, uint8_t X_dst,
                            bool X_condval) {
    /* Students: Change this code */
    // This will need to be updated in week 2, good enough for week 1
#ifdef PIPE
    // Student TODO
    // Prio: mispred branch > load-use > ret
    bool load_use_hazard = check_load_use_hazard(D_opcode, D_src1, D_src2, X_opcode, X_dst);
    bool ret_hazard = check_ret_hazard(D_opcode);
    bool mispred_branch_hazard = check_mispred_branch_hazard(X_opcode, X_condval);
    bool error_WB = error(W_in->status);
    #ifdef EC
        bool mispred_br_hazard = check_br_hazard(D_opcode);
        ret_hazard = ret_hazard || mispred_br_hazard;
    #endif
    bool dmem_in_flight = dmem_status == IN_FLIGHT;
    bool need_stall = error_WB || dmem_in_flight;
    deassert_flags = need_stall;
    pipe_control_stage(S_FETCH, false, need_stall);
    pipe_control_stage(S_DECODE, false, need_stall);
    pipe_control_stage(S_EXECUTE, false, need_stall);
    pipe_control_stage(S_MEMORY, false, need_stall);
    pipe_control_stage(S_WBACK, dmem_in_flight, false);
    if(dmem_in_flight) {
        return;
    } else if (mispred_branch_hazard) {
        pipe_control_stage(S_DECODE, true, false);
        pipe_control_stage(S_EXECUTE, true, false);
    } else if (load_use_hazard) {
        pipe_control_stage(S_FETCH, false, true);
        pipe_control_stage(S_DECODE, false, true);
        pipe_control_stage(S_EXECUTE, true, false);
    } else if (ret_hazard) {
        pipe_control_stage(S_DECODE, true, false);
    }
#else
    bool f_stall = F_out->status == STAT_HLT || F_out->status == STAT_INS;
    pipe_control_stage(S_FETCH, false, f_stall);
    pipe_control_stage(S_DECODE, false, false);
    pipe_control_stage(S_EXECUTE, false, false);
    pipe_control_stage(S_MEMORY, false, false);
    pipe_control_stage(S_WBACK, false, false);
#endif
}
