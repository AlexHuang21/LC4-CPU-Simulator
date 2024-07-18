/*
 * LC4.c: Defines simulator functions for executing instructions
 */

#include "LC4.h"
#include <stdio.h>

//macros
#define INSN_OP(I) ((I) >> 12)
#define INSN_11_9(I) (((I) >> 9) & 0x7)
#define INSN_IMM9(I) ((short)(I) & 0x1FF) // extracts 9-bit immediate value (bits [8:0])
#define INSN_8_6(I) (((I) >> 6) & 0x7) // extracts bits 8:6 for rs
#define INSN_8_0(I) (((I) >> 0) & 0x1FF) // extracts bits 8:0 for br imm
#define INSN_2_0(I) ((I) & 0x7) // extracts bits 2:0 for rt
#define INSN_IMM5(I) ((short)(I) & 0x1F) // extracts 5 bit immediate value [4:0]
#define INSN_IMM7(I) ((short)(I) & 0x7F) // extracts 7 bit immediate value [6:0]

int error = 0;

/*
 * Reset the machine state as Pennsim would do
 */
void Reset(MachineState* CPU) {
    //initialize pc to default starting pc, as well as psr's 16th bit to 1
    CPU->PC = 0x8200;
    CPU->PSR |= (1 << 15);
    CPU->R[0] = 0;
    CPU->R[1] = 0;
    CPU->R[2] = 0;
    CPU->R[3] = 0;
    CPU->R[4] = 0;
    CPU->R[5] = 0;
    CPU->R[6] = 0;
    CPU->R[7] = 0;
    //reset signals
    ClearSignals(CPU);
}


/*
 * Clear all of the control signals (set to 0)
 */
void ClearSignals(MachineState* CPU) {
    //reset all control signals
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 0;
    CPU->DATA_WE = 0;
    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    CPU->rdMux_CTL = 0;
    CPU->regInputVal = 0;
    CPU->NZPVal = 0;
    CPU->dmemAddr = 0;
    CPU->dmemValue = 0;
}


/*
 out the current state of the CPU to the file output.
 */
void WriteOut(MachineState* CPU, FILE* output) {
    //print current pc in hex
    fprintf(output, "%04X ", CPU->PC);
    //convert the instruction into binary by looping through it in memory
    unsigned short instruction = CPU->memory[CPU->PC];
    char binaryInstruction[17];
    for (int i = 15; i >= 0; i--) {
        binaryInstruction[15-i] = (instruction & (1 << i)) ? '1' : '0';
    }
    //null terminate the string
    binaryInstruction[16] = '\0';
    //print instruction
    fprintf(output, "%s ", binaryInstruction);
    
    //print regFile_WE
    fprintf(output, "%01X ", CPU->regFile_WE);
    
    //if regFile_WE is high, print rdMux_CTL and regInputVal
    if (CPU->regFile_WE) {
        fprintf(output, "%01X ", CPU->rdMux_CTL);
        fprintf(output, "%04X ", CPU->regInputVal);
    } else {
        fprintf(output, "0 0000 ");
    }

    //print NZP_WE
    fprintf(output, "%01X ", CPU->NZP_WE);
    
    //if NZP_WE is high, print NZP val
    if (CPU->NZP_WE) {
        fprintf(output, "%01X ", CPU->NZPVal);
    } else {
        fprintf(output, "0 ");
    }

    //print DATA_WE
    fprintf(output, "%01X ", CPU->DATA_WE);
    
    //if DATA_WE is high, print dmem address and dmem value
    if (CPU->DATA_WE) {
        fprintf(output, "%04X ", CPU->dmemAddr);
        fprintf(output, "%04X", CPU->dmemValue);
    } else {
        fprintf(output, "0000 0000");
    }
    fprintf(output, "\n");
}

/*
 * Parses rest of const operation and updates state of machine.
 */
void constOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    CPU->regFile_WE = 1;
    //get destination register to put in rdMux_CTL
    unsigned char rd = INSN_11_9(instruction);
    CPU->rdMux_CTL = rd;
    //get immediate value to put in regInputVal
    short immediate_val = INSN_IMM9(instruction);
    //make sure we handle sext by extending 16 bits if top bit is set
    if (immediate_val & 0x100) {
        immediate_val |= 0xFE00; 
    }
    //update registers and control signals
    CPU->regInputVal = immediate_val;
    CPU->R[rd] = immediate_val;
    //use immediate value to update NZP bits
    SetNZP(CPU, immediate_val);
    //write to NZP but not any data
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->dmemAddr = 0;
    CPU->dmemValue = 0;
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Parses rest of rti operation and updates state of machine.
 */
void rtiOp(MachineState* CPU, FILE* output) {
    //bit 15 of PSR should be 0 for RTI to be legal
    //clear WE signals
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 0;
    CPU->DATA_WE = 0;

    //write everything then update PC to R7
    WriteOut(CPU, output);
    CPU->PC = CPU->R[7];
}

/*
 * Parses rest of trap operation and updates state of machine.
 */
void trapOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //save return address before jumping into trap
    CPU->R[7] = CPU->PC + 1;
    //extract lowest 8 bits for trap bits
    unsigned char trap = instruction & 0xFF;
    //set signals and NZP bits
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = 7;
    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    SetNZP(CPU, CPU->R[7]);
    //update PSR using trap vector table's address
    CPU->PSR |= 0x8000;
    //store R7 in regInputVal
    CPU->regInputVal = CPU->R[7];
    //writeout and update PC to trap vector table's address
    WriteOut(CPU, output);
    CPU->PC = 0x8000 | trap;
}

/*
 * Parses rest of hiconst operation and updates state of machine.
 */
void hiconstOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //make sure hiconst is valid
    unsigned char check = (instruction >> 8) & 0x1;
    if (check != 1) {
        error = 1;
        return;
    }
    //save dest reg and imm8
    unsigned char rd = INSN_11_9(instruction);
    unsigned char imm = instruction & 0xFF;
    //update highest 8 bits of rd
    CPU->R[rd] = (CPU->R[rd] & 0x0FF) | (imm << 8);
    //set signals and NZP bits
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = rd;
    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    SetNZP(CPU, CPU->R[rd]);
    //store rd in regInputVal
    CPU->regInputVal = CPU->R[rd];
    //writeout and update pc
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Parses rest of str operation and updates state of machine.
 */
void strOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //extact rs and rt and imm6
    unsigned char rs = INSN_8_6(instruction);
    unsigned char rt = INSN_11_9(instruction);
    short imm = (short)(instruction & 0x3F);
    //sext imm
    if (imm & 0x20) {
        imm |= 0xFFC0;
    }
    unsigned short memAddress = CPU->R[rs] + imm;
    int isUserMode = (CPU->PSR >> 15) == 0;
    int isProtectedAddr = (memAddress < 0xFFFF && memAddress > 0xA000) && isUserMode;
    int isInvalidAddr = (memAddress < 0x1FFF || (memAddress > 0x8000 && memAddress < 0x9FFF));
    if (isProtectedAddr || isInvalidAddr) {
        error = 1;
        return;
    }
    if (rs == rt) {
      error = 1;
      return;
    }
    //Update memoryAddress
    CPU->memory[CPU->R[rs] + imm] = CPU->R[rt];
    //set signals and data
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 0;
    CPU->DATA_WE = 1;
    CPU->rdMux_CTL = 0;
    CPU->rsMux_CTL = rs;
    CPU->rtMux_CTL = rt;
    
    CPU->dmemAddr = CPU->R[rs] + imm;
    CPU->dmemValue = CPU->R[rt];
    //writeout and update pc
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Parses rest of ldr operation and updates state of machine.
 */
void ldrOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //extact rd and rs and imm6
    unsigned char rs = INSN_8_6(instruction);
    unsigned char rd = INSN_11_9(instruction);
    short imm = (short)(instruction & 0x3F);
    //sext imm
    if (imm & 0x20) {
        imm |= 0xFFC0;
    }
    unsigned short memAddress = CPU->R[rs] + imm;
    int isUserMode = (CPU->PSR >> 15) == 0;
    int isProtectedAddr = (memAddress < 0xFFFF && memAddress > 0xA000) && isUserMode;
    int isInvalidAddr = (memAddress < 0x1FFF || (memAddress > 0x8000 && memAddress < 0x9FFF));
    if (isProtectedAddr || isInvalidAddr) {
        error = 1;
        return;
    }
    if (rs == rd) {
      error = 1;
      return;
    }

    //Update memoryAddress and regInputVal to same val
    CPU->R[rd] = CPU->memory[CPU->R[rs] + imm];
    CPU->regInputVal = CPU->R[rd];
    //set signals and NZP
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = rd;
    CPU->rsMux_CTL = rs;
    CPU->rtMux_CTL = 0;
    SetNZP(CPU, CPU->R[rd]);
    //writeout and update pc
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * This function should execute one LC4 datapath cycle.
 */
int UpdateMachineState(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //extract opcode using macros
    unsigned short opcode = INSN_OP(instruction);
    unsigned short ceiling = 0xFFFF;

    if (CPU->PC == 0x80FF) {
        return 1;
    }
    if (CPU->PC > 0xA000 && CPU->PC < ceiling) {
        return 1;
    }
    if (CPU->PC > 0x2000 && CPU->PC < 0x7FFF) {
        return 1;
    }

    //go through different possible opcodes and make necessary updates
    switch (opcode) {
        case 9: {
            constOp(CPU, output);
            break;
        }
        case 1: {
            ArithmeticOp(CPU, output);
            break;
        }
        case 8: {
            rtiOp(CPU, output);
            break;
        }
        case 0: {
            BranchOp(CPU, output);
            break;
        }
        case 15: {
            trapOp(CPU, output);
            break;
        }
        case 13: {
            hiconstOp(CPU, output);
            break;
        }
        case 7: {
            strOp(CPU, output);
            break;
        }
        case 6: {
            ldrOp(CPU, output);
            break;
        }
        case 2: {
            ComparativeOp(CPU, output);
            break;
        }
        case 5: {
            LogicalOp(CPU, output);
            break;
        }
        case 12: {
            JumpOp(CPU, output);
            break;
        }
        case 4: {
            JSROp(CPU, output);
            break;
        }
        case 10: {
            ShiftModOp(CPU, output);
            break;
        }
        default: {
            return 1;
        }
    }
    if (error) {
        return 1;
    }
    return 0;
}



//////////////// PARSING HELPER FUNCTIONS ///////////////////////////



/*
 * Parses rest of branch operation and updates state of machine.
 */
void BranchOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    short imm = instruction & 0x1FF; // Immediate 9-bit value (bits [8:0])
    unsigned char condition = (instruction >> 9) & 0x7; // Branch condition (bits [11:9])
    unsigned char NZP = CPU->PSR & 0x7; // NZP condition codes in the PSR

    // Sign extend the immediate value if necessary
    if ((imm >> 8) & 0x1) {
        imm |= 0xFE00;
    }

    // Set control signals
    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    CPU->rdMux_CTL = 0;
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 0;
    CPU->DATA_WE = 0;

    // Determine the branch action based on the condition
    int shouldBranch = 0;
    switch (condition) {
        case 0: // No operation
            break;
        case 1: // BRP (Branch if positive)
        case 2: // BRZ (Branch if zero)
        case 3: // BRZP (Branch if zero or positive)
        case 4: // BRN (Branch if negative)
        case 5: // BRNP (Branch if negative or positive)
        case 6: // BRNZ (Branch if negative or zero)
        case 7: // BRNZP (Branch always)
            shouldBranch = (NZP & condition) != 0;
            break;
        default:
            break;
    }

    // Write out the current state
    WriteOut(CPU, output);

    // Update the PC based on the branch condition
    CPU->PC += shouldBranch ? (imm + 1) : 1;
}



/*
 * Parses rest of arithmetic operation and prints out.
 */
void ArithmeticOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    unsigned short opcode = INSN_OP(instruction);
    //decode rd, rs, and rt from instruction
    unsigned char rd = INSN_11_9(instruction);
    unsigned char rs = INSN_8_6(instruction);
    unsigned char rt;
    int ans;
    short imm5;
    //determine type of operation and update ans
    if ((instruction & 0x0020) == 0) {
        //cannot be add imm5 so we can use rt
        rt = INSN_2_0(instruction);
        CPU->rtMux_CTL = rt;
        //check bits 5:3 to determine operation
        switch ((instruction >> 3) & 0x7) {
            //add
            case 0: {
                ans = (short)CPU->R[rs] + (short)CPU->R[rt];
                break;
            }
            //mult
            case 1: {
                ans = (short)CPU->R[rs] * (short)CPU->R[rt];
                break;
            }
            //subtract
            case 2: {
                ans = (short)CPU->R[rs] - (short)CPU->R[rt];
                break;
            }
            //divide
            case 3: {
                if (CPU->R[rt] == 0) {
                    error = 1;
                    return;
                }
                ans = (short)CPU->R[rs] / (short)CPU->R[rt];
                break;
            }
            default:
                error = 1;
                return;
        }
    } else {
        imm5 = INSN_IMM5(instruction);
        //add sext if needed
        if (imm5 & 0x10) {
            imm5 |= 0xFFE0;
        }
        ans = (short)CPU->R[rs] + imm5;
        //imm5 doesn't use rt
        CPU->rtMux_CTL = 0;
    }
    //update signals and set NZP
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = rd;
    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    CPU->regInputVal = ans;
    SetNZP(CPU, ans);
    CPU->R[rd] = ans;
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Parses rest of comparative operation and prints out.
 */
void ComparativeOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    unsigned char rs;
    unsigned char rt;
    short imm;
    short ans;

    //check bits 8:7 to determine operation
    switch ((instruction >> 7) & 0x3) {
        case 0: {
            //CMP
            rs = INSN_11_9(instruction);
            rt = INSN_2_0(instruction);
            ans = (short)CPU->R[rs] - (short)CPU->R[rt];
            break;
        }
        case 1: {
            //CMPU
            rs = INSN_11_9(instruction);
            rt = INSN_2_0(instruction);
            ans = CPU->R[rs] - CPU->R[rt];
            break;
        }
        case 2: {
            //CMPI
            rs = INSN_11_9(instruction);
            imm = INSN_IMM7(instruction);
            //sext imm if needed
            if (imm & 0x40) {
                imm |= 0xFF80;
            }
            ans = (short) CPU->R[rs] - imm;
            break;
        }
        case 3: {
            //CMPIU
            rs = INSN_11_9(instruction);
            unsigned short uimm = instruction & 0x7F;
            ans = (unsigned short)CPU->R[rs] - uimm;
            break;
        }
    }
    //update signals and set NZP
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = 0;
    CPU->rsMux_CTL = rs;
    CPU->rtMux_CTL = 0;
    SetNZP(CPU, ans);
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Parses rest of logical operation and prints out.
 */
void LogicalOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    unsigned char rd = (instruction >> 9) & 0x7; // Destination register (bits [11:9])
    unsigned char rs = (instruction >> 6) & 0x7; // Source register (bits [8:6])
    unsigned char rt;
    short imm;

    switch ((instruction >> 3) & 0x7) { // Opcode (bits [5:3])
        case 0: // AND
            rt = (instruction >> 0) & 0x7; // Target register (bits [2:0])
            CPU->R[rd] = CPU->R[rs] & CPU->R[rt];
            break;
        case 1: // NOT
            CPU->R[rd] = ~CPU->R[rs];
            break;
        case 2: // OR
            rt = (instruction >> 0) & 0x7; // Target register (bits [2:0])
            CPU->R[rd] = CPU->R[rs] | CPU->R[rt];
            break;
        case 3: // XOR
            rt = (instruction >> 0) & 0x7; // Target register (bits [2:0])
            CPU->R[rd] = CPU->R[rs] ^ CPU->R[rt];
            break;
        default: // AND immediate
            imm = (short)(instruction & 0x1F); // Immediate 5-bit value (bits [4:0])
            if ((imm >> 4) & 0x1) {
                imm |= 0xFFE0;
            }
            CPU->R[rd] = CPU->R[rs] & imm;
            break;
    }

    // Update control signals
    CPU->rsMux_CTL = rs;
    CPU->rtMux_CTL = (((instruction >> 3) & 0x7) == 4) ? 0 : rt;
    CPU->rdMux_CTL = rd;
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->regInputVal = CPU->R[rd];

    // Set NZP bits and write out the current state
    SetNZP(CPU, CPU->R[rd]);
    WriteOut(CPU, output);

    // Increment PC
    CPU->PC++;
}

/*
 * Parses rest of jump operation and prints out.
 */
void JumpOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    short imm = instruction & 0x7FF;
    if ((imm >> 10) & 0x1) {
        imm |= 0xF800;
    }
    CPU->rsMux_CTL = 0;
    CPU->regFile_WE = 0;
    CPU->NZP_WE = 0;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    unsigned short opcode = (instruction >> 11) & 0x1;
    if (opcode == 1) {
        WriteOut(CPU, output);
        CPU->PC = (CPU->PC & 0x8000) | (imm << 4);
    } else if (opcode == 0) {
        WriteOut(CPU, output);
        CPU->PC = INSN_8_6(instruction);
    }
}

/*
 * Parses rest of JSR operation and prints out.
 */
void JSROp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    //make sure we writeout then update the PC
    unsigned short tempPC;
    unsigned short opcode = (instruction >> 11) & 0x1;
    short imm = instruction & 0x7FF;
    if ((imm >> 10) & 0x1) {
        imm |= 0xF800;
    }
    //save return address
    CPU->R[7] = CPU->PC + 1;

    CPU->regInputVal = CPU->R[7];
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->rdMux_CTL = 7;
    CPU->rtMux_CTL = 0;
    CPU->rsMux_CTL = 0;

    SetNZP(CPU, CPU->R[7]);

    WriteOut(CPU, output);
    if (opcode == 1) {
        tempPC = (CPU->PC & 0x8000) | (imm << 4);
    } else if (opcode == 0) {
        tempPC = (instruction >> 6) & 0x7;
    }
    CPU->PC = tempPC;
}

/*
 * Parses rest of shift/mod operations and prints out.
 */
void ShiftModOp(MachineState* CPU, FILE* output) {
    unsigned short instruction = CPU->memory[CPU->PC];
    unsigned short imm = instruction & 0xF;
    unsigned char rd = (instruction >> 9) & 0x7;
    unsigned char rs = (instruction >> 6) & 0x7;

    switch ((instruction >> 4) & 0x3) {
        case 0: {
            //SSL
            CPU->R[rd] = CPU->R[rs] << imm;
            break;
        }
        case 1: {
            //SRA
            CPU->R[rd] = (short)CPU->R[rs] << imm;
            break;
        }
        case 2: {
            //SRL
            CPU->R[rd] = CPU->R[rs] >> imm;
            break;
        }
        case 3: {
            //mod
            unsigned char rt = instruction & 0x7;
            CPU->R[rd] = CPU->R[rs] % CPU->R[rt];
            break;
        }
    }

    CPU->rsMux_CTL = 0;
    CPU->rtMux_CTL = 0;
    CPU->rdMux_CTL = 0;
    CPU->regFile_WE = 1;
    CPU->NZP_WE = 1;
    CPU->DATA_WE = 0;
    CPU->regInputVal = CPU->R[rd];

    SetNZP(CPU, CPU->R[rd]);
    WriteOut(CPU, output);
    CPU->PC ++;
}

/*
 * Set the NZP bits in the PSR.
 */
void SetNZP(MachineState* CPU, short result) {
    //use PSR to reset NZP bits
    CPU->PSR &= 0xFFF8;
    // Reset the NZPVal
    CPU->NZPVal = 0;
    //set bits and update NZP val
    if (result < 0) {
        //N bit
        CPU->PSR |= (1 << 2);
        CPU->NZPVal = 4;
    } else if (result == 0) {
        //Z bit
        CPU->PSR |= (1 << 1);
        CPU->NZPVal = 2;
    } else {
        //P bit
        CPU->PSR |= 1;
        CPU->NZPVal = 1;
    }
}
