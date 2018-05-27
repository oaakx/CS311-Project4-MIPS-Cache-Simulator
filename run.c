/*
 * I have made changes to **cs311.c**. I updated CURRENT_STATE initialization method
 * to correspond to my changes I made to CPU_State_Struct of **util.c**.
 * */

/***************************************************************/
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   CS311 KAIST                                               */
/*   run.c                                                     */
/*                                                             */
/***************************************************************/

#include <stdio.h>

#include "util.h"
#include "run.h"

/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
instruction* get_inst_info(uint32_t pc) { 
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

CPU_State NEXT_STATE;
int bne_beq_flush;
uint32_t bne_beq_npc;
int j_jr_jal_flush;
uint32_t j_jr_jal_npc;
int lw_stall;
uint32_t lw_npc;

int miss_stall = 0;

int debug_on = FALSE;

/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/
void process_instruction(){
    bne_beq_flush = FALSE;
    j_jr_jal_flush = FALSE;
    lw_stall = FALSE;
    lw_stall = FALSE;

    doIF();
    doWB(); // should happen before ID
    doID();
    doEX();
    doMEM();

    if (bne_beq_flush) {
        NEXT_STATE.IF_ID_NOOP = TRUE;
        NEXT_STATE.ID_EX_NOOP = TRUE;
        NEXT_STATE.EX_MEM_NOOP = TRUE;
        CURRENT_STATE.PC = bne_beq_npc;
    }

    update_PIPE();

    if (j_jr_jal_flush) {
        if (debug_on)
            printf("j_jr_jal_flush: TRUE\n");
        NEXT_STATE.IF_ID_NOOP = TRUE;
        CURRENT_STATE.PC = j_jr_jal_npc;
    }

    /* stall IF and ID for load-use data hazard */
    instruction *exins =  &INST_INFO[CURRENT_STATE.ID_EX_II];
    if (!CURRENT_STATE.ID_EX_NOOP && OPCODE(exins) == 0x23) { // lw
        // TODO: this code has bug !!! fix later

        if (NEXT_STATE.ID_EX_RS == CURRENT_STATE.ID_EX_DEST ||
            NEXT_STATE.ID_EX_RT == CURRENT_STATE.ID_EX_DEST) {
            lw_stall = TRUE;
        }
    }

    // update PC
    if (!bne_beq_flush && !j_jr_jal_flush && !lw_stall && miss_stall == 0 &&
            (CURRENT_STATE.PC - MEM_TEXT_START)/4 < NUM_INST) {
        CURRENT_STATE.PC += 4;
    }

    // update latches
    update_latches();

    if (miss_stall > 0) {
        miss_stall--; // TODO: maybe need to move it after update_latches??
    }


    // if next fetch will fail and all other stages have noops then halt
    if ((CURRENT_STATE.PC - MEM_TEXT_START)/4 >= NUM_INST &&
            CURRENT_STATE.IF_ID_NOOP &&
            CURRENT_STATE.ID_EX_NOOP &&
            CURRENT_STATE.EX_MEM_NOOP &&
            CURRENT_STATE.MEM_WB_NOOP)
        RUN_BIT = FALSE;
}

void doIF() {
    uint32_t ii = CURRENT_STATE.PC - MEM_TEXT_START;
    ii >>= 2;

    if (ii >= NUM_INST) {
        NEXT_STATE.IF_ID_NOOP = TRUE;
        return;
    }

    instruction *curins = &INST_INFO[ii];
    uint32_t npc = CURRENT_STATE.PC + 4;

    if (debug_on) {
        printf(" IF: ");
        print_inst_name(curins);
        printf("\n");
    }

    NEXT_STATE.IF_ID_NOOP = FALSE;
    NEXT_STATE.IF_ID_II = ii;
    NEXT_STATE.IF_ID_NPC = npc;
}

void doID() {
    int noop = CURRENT_STATE.IF_ID_NOOP;
    uint32_t ii = CURRENT_STATE.IF_ID_II;
    instruction *curins = &INST_INFO[ii];
    uint32_t npc = CURRENT_STATE.IF_ID_NPC;

    if (debug_on) {
        printf(" ID: ");
        print_inst_name(curins);
        printf("\n");
    }

    if (noop) {
        NEXT_STATE.ID_EX_NOOP = noop;
        return;
    }

    // to avoid forwarding for operations that dont write back, set dest to (invalid register id) 99
    unsigned char dest = 99;

    if (OPCODE(curins) == 0x00) { // R-type
        if (FUNC(curins) != 0x08) // not jr
            dest = RD(curins);
        else { // jr
            j_jr_jal_flush = TRUE;
            j_jr_jal_npc = CURRENT_STATE.REGS[ RS(curins) ]; // TODO: (RS register) forward here from MEM, EX
        }
    } else if (OPCODE(curins) == 0x02 || OPCODE(curins) == 0x03) { // J-type
        if (OPCODE(curins) == 0x03) // jal
            dest = 31; // $ra

        // j and jal
        if (debug_on)
            printf("j_jr_jal_flush TRUE (realtime update)\n");
        j_jr_jal_flush = TRUE;
        j_jr_jal_npc = (npc & 0xf0000000) | (TARGET(curins) << 2);
    } else { // I-type
        if (OPCODE(curins) != 0x2B && // not sw
                OPCODE(curins) != 0x04 && // not beq
                OPCODE(curins) != 0x05) // not bne
            dest = RT(curins);
    }

    NEXT_STATE.ID_EX_NOOP = noop;
    NEXT_STATE.ID_EX_II = ii;
    NEXT_STATE.ID_EX_NPC = npc;

    // TODO: This is buggy! If instruction doesn't have rt register, then random value will be read. Can cause segfault.
    NEXT_STATE.ID_EX_REG1 = CURRENT_STATE.REGS[ RS(curins) ];
    NEXT_STATE.ID_EX_REG2 = CURRENT_STATE.REGS[ RT(curins) ];

    NEXT_STATE.ID_EX_IMM = IMM(curins);
    NEXT_STATE.ID_EX_RS = RS(curins);
    NEXT_STATE.ID_EX_RT = RT(curins);
    NEXT_STATE.ID_EX_DEST = dest;
}

void doEX() {
    int noop = CURRENT_STATE.ID_EX_NOOP;
    uint32_t ii = CURRENT_STATE.ID_EX_II;
    instruction *curins = &INST_INFO[ii];
    uint32_t npc = CURRENT_STATE.ID_EX_NPC;
    uint32_t reg1 = CURRENT_STATE.ID_EX_REG1;
    uint32_t reg2 = CURRENT_STATE.ID_EX_REG2;
    short imm = CURRENT_STATE.ID_EX_IMM;
    unsigned char dest = CURRENT_STATE.ID_EX_DEST;

    // refetch reg vals (here is why: EX stalled but WB not. then forwarding would work
    // at first until WB is complete. after that neither forwarding from WB nor reg vals
    // from ID would be correct. only refetching works.)
    // again this code is buggy, reg2 part might cause segfault.
    reg1 = CURRENT_STATE.REGS[ RS(curins) ];
    reg2 = CURRENT_STATE.REGS[ RT(curins) ];

    if (debug_on) {
        printf(" EX: ");
        print_inst_name(curins);
        printf("\n");
    }

    if (noop) {
        NEXT_STATE.EX_MEM_NOOP = noop;
        return;
    }

    /* Forwarding */
    if (!CURRENT_STATE.EX_MEM_NOOP && CURRENT_STATE.EX_MEM_DEST == RS(curins)) {
        reg1 = CURRENT_STATE.EX_MEM_ALU_OUT;
    } else if (!CURRENT_STATE.MEM_WB_NOOP && CURRENT_STATE.MEM_WB_DEST == RS(curins)) {
        reg1 = CURRENT_STATE.MEM_WB_VAL;
    }

    if (!CURRENT_STATE.EX_MEM_NOOP && CURRENT_STATE.EX_MEM_DEST == RT(curins)) {
        reg2 = CURRENT_STATE.EX_MEM_ALU_OUT;
    } else if (!CURRENT_STATE.MEM_WB_NOOP && CURRENT_STATE.MEM_WB_DEST == RT(curins)) {
        reg2 = CURRENT_STATE.MEM_WB_VAL;
    }

    // printf(" | reg1 : 0x%x\n", reg1);
    // printf(" | ii : %d\n", ii);
    // printf(" |\n");

    /* ALU calculation */
    uint32_t alu_out = 0;

    if (OPCODE(curins) == 0x00) { // R-type
        switch (FUNC(curins)) {
            case 0x21: // addu
                alu_out = reg1 + reg2;
                break;
            case 0x24: // and
                alu_out = reg1 & reg2;
                break;
            case 0x08: // jr
                // doesn't need ALU
                break;
            case 0x27: // nor
                alu_out = ~(reg1 | reg2);
                break;
            case 0x25: // or
                alu_out = reg1 | reg2;
                break;
            case 0x2B: // sltu
                alu_out = (uint32_t) (reg1 < reg2);
                break;
            case 0x00: // sll
                alu_out = reg2 << SHAMT(curins);
                break;
            case 0x02: // srl
                alu_out = reg2 >> SHAMT(curins);
                break;
            case 0x23: // subu
                alu_out = reg1 - reg2;
                break;
            default:
                // TODO: this shouldn't happen!!!
                break;
        }
    } else if (OPCODE(curins) == 0x02 || OPCODE(curins) == 0x03) { // J-type
        uint32_t target = TARGET(curins);

        if (OPCODE(curins) == 0x02) { // j
            // doesn't need ALU
            //
            // jump executes at ID(?)
            // npc = (npc & 0xf0000000) | (target << 2);
        } else if (OPCODE(curins) == 0x03) { // jal
            // doesn't need ALU
            //
            // npc = npc + 8;
            // npc = (npc & 0xf0000000) | (target << 2);
            alu_out = npc + 4;
        }
    } else { // I-type
        switch (OPCODE(curins)) {
            case 0x09: // addiu
                alu_out = reg1 + imm;
                break;
            case 0x0C: // andi
                alu_out = reg1 & imm;
                break;
            case 0x04: // beq
                alu_out = (uint32_t)(reg1 == reg2);
                break;
            case 0x05: // bne
                alu_out = (uint32_t)(reg1 != reg2);
                break;
            case 0x0F: // lui
                alu_out = (uint32_t)imm << 16;
                break;
            case 0x23: // lw
                alu_out = reg1 + imm;
                break;
            case 0x0D: // ori
                alu_out = reg1 | imm;
                break;
            case 0x0B: // sltiu
                alu_out = (uint32_t)(reg1 < imm);
//                printf("reg1:%u  imm:%d  alu_out(reg1<imm):%u\n", reg1, imm, alu_out);
                break;
            case 0x2B: // sw
                alu_out = reg1 + imm;
                break;
            default:
                // ???
                break;
        }
    }

    NEXT_STATE.EX_MEM_NOOP = noop;
    NEXT_STATE.EX_MEM_II = ii;
    NEXT_STATE.EX_MEM_NPC = npc;
    NEXT_STATE.EX_MEM_ALU_OUT = alu_out;
    NEXT_STATE.EX_MEM_WRITE_VAL = reg2;
    NEXT_STATE.EX_MEM_DEST = dest;
}

void doMEM() {
    int noop = CURRENT_STATE.EX_MEM_NOOP;
    uint32_t ii = CURRENT_STATE.EX_MEM_II;
    instruction *curins = &INST_INFO[ii];
    uint32_t npc = CURRENT_STATE.EX_MEM_NPC;
    uint32_t alu_out = CURRENT_STATE.EX_MEM_ALU_OUT;
    unsigned char dest = CURRENT_STATE.EX_MEM_DEST;
    uint32_t writeval = CURRENT_STATE.EX_MEM_WRITE_VAL;

    // if (CYCLE_COUNT == 52-1+1) {
    //     print_inst_name(curins);
    //     printf(" | rt(curins) = %d\n", RT(curins));
    //     printf(" | writeval = 0x%x\n", writeval);
    // }

    if (debug_on) {
        printf("MEM: ");
        print_inst_name(curins);
        printf("\n");
    }

    if (noop) {
        NEXT_STATE.MEM_WB_NOOP = noop;
        return;
    }

    /* Forwarding */
    if (!CURRENT_STATE.MEM_WB_NOOP && CURRENT_STATE.MEM_WB_DEST == RT(curins)) {
        writeval = CURRENT_STATE.MEM_WB_VAL;
    }

    uint32_t wbval = alu_out; // alu_out OR mem_out

    if (OPCODE(curins) == 0x04 || OPCODE(curins) == 0x05) { // bne or beq
        if (alu_out) {
            bne_beq_flush = TRUE;
            bne_beq_npc = npc + (IMM(curins) << 2);
        }
    }
    else if (OPCODE(curins) == 0x23) { // lw
        // wbval = mem_read_32(alu_out);

        // printf(" | alu_out : %x\n", alu_out);
        // printf(" | ii : %d\n", ii);

        int penalty;
        if ((penalty = read_cache(alu_out, &wbval)) > 0) {
            miss_stall = penalty;
        }

        // printf(" | wbval : %d\n", wbval);
    }
    else if (OPCODE(curins) == 0x2B) { // sw
        // mem_write_32(alu_out, writeval);
        // if (CYCLE_COUNT == 83 - 1) {
        //     printf(" | ii = %d (17?)\n", ii);
        //     printf(" | alu_out = 0x%x\n", alu_out);
        // }

        int penalty;
        if ((penalty = write_cache(alu_out, writeval)) > 0) {
            miss_stall = penalty;
        }
    }

    NEXT_STATE.MEM_WB_NOOP = noop;
    NEXT_STATE.MEM_WB_II = ii;
    NEXT_STATE.MEM_WB_NPC = npc;
    NEXT_STATE.MEM_WB_VAL = wbval;
    NEXT_STATE.MEM_WB_DEST = dest;
}

void doWB() {
    int noop = CURRENT_STATE.MEM_WB_NOOP;
    uint32_t ii = CURRENT_STATE.MEM_WB_II;
    instruction *curins = &INST_INFO[ii];
    uint32_t npc = CURRENT_STATE.MEM_WB_NPC;
    uint32_t wbval = CURRENT_STATE.MEM_WB_VAL;
    unsigned char dest = CURRENT_STATE.MEM_WB_DEST;

    if (debug_on) {
        printf(" WB: ");
        print_inst_name(curins);
        printf("\n");
    }

    if (noop) {
        return;
    }

    INSTRUCTION_COUNT++;
    if (!(OPCODE(curins) == 0x00 && FUNC(curins) == 0x08) && // jr
            OPCODE(curins) != 0x2B && // sw
            OPCODE(curins) != 0x02 && // j
            OPCODE(curins) != 0x04 && // beq
            OPCODE(curins) != 0x05) { // bne
        CURRENT_STATE.REGS[dest] = wbval;
    }
}

void update_latches() {
    // TODO: add if(miss_stall) {} somewhere here

    if (miss_stall == 0) {
        if (lw_stall) {
            CURRENT_STATE.ID_EX_NOOP = TRUE;
        } else {
            CURRENT_STATE.IF_ID_NOOP        = NEXT_STATE.IF_ID_NOOP;
            CURRENT_STATE.IF_ID_II          = NEXT_STATE.IF_ID_II;
            CURRENT_STATE.IF_ID_NPC         = NEXT_STATE.IF_ID_NPC;

            CURRENT_STATE.ID_EX_NOOP        = NEXT_STATE.ID_EX_NOOP;
            CURRENT_STATE.ID_EX_II          = NEXT_STATE.ID_EX_II;
            CURRENT_STATE.ID_EX_NPC         = NEXT_STATE.ID_EX_NPC;
            CURRENT_STATE.ID_EX_REG1        = NEXT_STATE.ID_EX_REG1;
            CURRENT_STATE.ID_EX_REG2        = NEXT_STATE.ID_EX_REG2;
            CURRENT_STATE.ID_EX_IMM         = NEXT_STATE.ID_EX_IMM;
            CURRENT_STATE.ID_EX_RS          = NEXT_STATE.ID_EX_RS;
            CURRENT_STATE.ID_EX_RT          = NEXT_STATE.ID_EX_RT;
            CURRENT_STATE.ID_EX_DEST        = NEXT_STATE.ID_EX_DEST;
        }

        CURRENT_STATE.EX_MEM_NOOP       = NEXT_STATE.EX_MEM_NOOP;
        CURRENT_STATE.EX_MEM_II         = NEXT_STATE.EX_MEM_II;
        CURRENT_STATE.EX_MEM_NPC        = NEXT_STATE.EX_MEM_NPC;
        CURRENT_STATE.EX_MEM_ALU_OUT    = NEXT_STATE.EX_MEM_ALU_OUT;
        CURRENT_STATE.EX_MEM_WRITE_VAL  = NEXT_STATE.EX_MEM_WRITE_VAL;
        CURRENT_STATE.EX_MEM_DEST       = NEXT_STATE.EX_MEM_DEST;

        CURRENT_STATE.MEM_WB_NOOP       = NEXT_STATE.MEM_WB_NOOP;
        CURRENT_STATE.MEM_WB_II         = NEXT_STATE.MEM_WB_II;
        CURRENT_STATE.MEM_WB_NPC        = NEXT_STATE.MEM_WB_NPC;
        CURRENT_STATE.MEM_WB_VAL        = NEXT_STATE.MEM_WB_VAL;
        CURRENT_STATE.MEM_WB_DEST       = NEXT_STATE.MEM_WB_DEST;
    }
    else {
        CURRENT_STATE.MEM_WB_NOOP = TRUE;
    }
}

void update_PIPE() {
//    CURRENT_STATE.PIPE[IF_STAGE] = ((cpc - MEM_TEXT_START)/4 >= NUM_INST ?0 :cpc);
//    CURRENT_STATE.PIPE[ID_STAGE] = (CURRENT_STATE.IF_ID_NOOP ?0 :MEM_TEXT_START + (CURRENT_STATE.IF_ID_II << 2));
//    CURRENT_STATE.PIPE[EX_STAGE] = (CURRENT_STATE.ID_EX_NOOP ?0 :MEM_TEXT_START + (CURRENT_STATE.ID_EX_II << 2));
//    CURRENT_STATE.PIPE[MEM_STAGE] = (CURRENT_STATE.EX_MEM_NOOP ?0 :MEM_TEXT_START + (CURRENT_STATE.EX_MEM_II << 2));
//    CURRENT_STATE.PIPE[WB_STAGE] = (CURRENT_STATE.MEM_WB_NOOP ?0 :MEM_TEXT_START + (CURRENT_STATE.MEM_WB_II << 2));

    CURRENT_STATE.PIPE[IF_STAGE] = (NEXT_STATE.IF_ID_NOOP ?0 :(NEXT_STATE.IF_ID_II << 2) + MEM_TEXT_START);
    CURRENT_STATE.PIPE[ID_STAGE] = (NEXT_STATE.ID_EX_NOOP ?0 :(NEXT_STATE.ID_EX_II << 2) + MEM_TEXT_START);
    CURRENT_STATE.PIPE[EX_STAGE] = (NEXT_STATE.EX_MEM_NOOP ?0 :(NEXT_STATE.EX_MEM_II << 2) + MEM_TEXT_START);
    CURRENT_STATE.PIPE[MEM_STAGE] = (NEXT_STATE.MEM_WB_NOOP ?0 :(NEXT_STATE.MEM_WB_II << 2) + MEM_TEXT_START);
    CURRENT_STATE.PIPE[WB_STAGE] = (CURRENT_STATE.MEM_WB_NOOP ?0 :(CURRENT_STATE.MEM_WB_II << 2) + MEM_TEXT_START);
}

void print_inst_name(instruction *curins) {
    if (OPCODE(curins) == 0x00) { // R-type
        switch (FUNC(curins)) {
            case 0x21: // addu
                printf("addu");
                break;
            case 0x24: // and
                printf("and");
                break;
            case 0x08: // jr
                printf("jr");
                break;
            case 0x27: // nor
                printf("nor");
                break;
            case 0x25: // or
                printf("or");
                break;
            case 0x2B: // sltu
                printf("sltu");
                break;
            case 0x00: // sll
                printf("sll");
                break;
            case 0x02: // srl
                printf("srl");
                break;
            case 0x23: // subu
                printf("subu");
                break;
            default:
                printf("invalid_r_type");
                break;
        }
    } else if (OPCODE(curins) == 0x02 || OPCODE(curins) == 0x03) { // J-type
        if (OPCODE(curins) == 0x02) { // j
            printf("j");
        } else if (OPCODE(curins) == 0x03) { // jal
            printf("jal");
        }
    } else { // I-type
        switch (OPCODE(curins)) {
            case 0x09: // addiu
                printf("addiu");
                break;
            case 0x0C: // andi
                printf("andi");
                break;
            case 0x04: // beq
                printf("beq");
                break;
            case 0x05: // bne
                printf("bne");
                break;
            case 0x0F: // lui
                printf("lui");
                break;
            case 0x23: // lw
                printf("lw");
                break;
            case 0x0D: // ori
                printf("ori");
                break;
            case 0x0B: // sltiu
                printf("sltiu");
                break;
            case 0x2B: // sw
                printf("sw");
                break;
            default:
                printf("invalid_i_type");
                break;
        }
    }
}