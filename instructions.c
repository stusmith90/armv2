#include "armv2.h"
#include <stdio.h>

#define ALU_TYPE_IMM 0x02000000
#define ALU_SETS_FLAGS 0x00100000
#define ALU_OPCODE_AND 0x0
#define ALU_OPCODE_EOR 0x1
#define ALU_OPCODE_SUB 0x2
#define ALU_OPCODE_RSB 0x3
#define ALU_OPCODE_ADD 0x4
#define ALU_OPCODE_ADC 0x5
#define ALU_OPCODE_SBC 0x6
#define ALU_OPCODE_RSC 0x7
#define ALU_OPCODE_TST 0x8
#define ALU_OPCODE_TEQ 0x9
#define ALU_OPCODE_CMP 0xa
#define ALU_OPCODE_CMN 0xb
#define ALU_OPCODE_ORR 0xc
#define ALU_OPCODE_MOV 0xd
#define ALU_OPCODE_BIC 0xe
#define ALU_OPCODE_MVN 0xf

#define ALU_SHIFT_LSL  0x0
#define ALU_SHIFT_LSR  0x1
#define ALU_SHIFT_ASR  0x2
#define ALU_SHIFT_ROR  0x3

armv2exception_t ALUInstruction                         (armv2_t *cpu,uint32_t instruction)
{
    uint32_t opcode   = (instruction>>21)&0xf;
    uint32_t rn       = (instruction>>16)&0xf;
    uint32_t rd       = (instruction>>12)&0xf;
    uint32_t result   = 0;
    uint32_t source_val;
    uint32_t shift_c = (cpu->regs.actual[PC]&FLAG_C);
    uint32_t arith_v = cpu->regs.actual[PC]&FLAG_V;
    uint64_t result64;
    if(instruction&ALU_TYPE_IMM) {
        source_val = (instruction&0xff) << ((instruction>>7)&0x1e);
    }
    else {
        uint32_t shift_type = (instruction>>5)&0x3;
        uint32_t shift_amount;
        source_val = *cpu->regs.effective[instruction&0xf];
        if(instruction&0x10) {
            //shift amount comes from a register
            shift_amount = (*cpu->regs.effective[(instruction>>8)&0xf])&0xff;
        }
        else {
            //shift amount is in the instruction
            shift_amount = (instruction>>7)&0x1f;
        }
        switch(shift_type) {
        case ALU_SHIFT_LSL:
            if(shift_amount < 32) {
                shift_c = (source_val>>(32-shift_amount))&1;
                source_val <<= shift_amount;
            }
            else if(shift_amount == 32) {
                shift_c = source_val&1;
                source_val = 0;
            }
            else {
                shift_c = source_val = 0;
            }
            
            break;
        case ALU_SHIFT_LSR:
            if(shift_amount == 0) {
                if((instruction&0x10) == 0) {
                    //this means LSR 32
                    shift_c = source_val&0x80000000;
                    source_val = 0;
                }
            }
            else if(shift_amount < 32) {
                shift_c = (source_val>>(shift_amount-1))&1;
                source_val >>= shift_amount;
            }
            else if(shift_amount == 32) {
                shift_c = source_val&0x80000000;
                source_val = 0;
            }
            else {
                shift_c = source_val = 0;
            }
            
            break;
        case ALU_SHIFT_ASR:
            if(shift_amount == 0) {
                if((instruction&0x10) == 0) {
                    //this means asr 32
                    shift_c = (source_val>>31)&1;
                    source_val = shift_c*0xffffffff;
                }
            }
            else if(shift_amount < 32){
                shift_c = (source_val>>(shift_amount-1))&1;
                source_val = (uint32_t)(((int32_t)source_val)>>shift_amount);
            }
            else {
                shift_c = (shift_amount>>31)&1;
                source_val = shift_c*0xffffffff;
            }
            break;
        case ALU_SHIFT_ROR:
            if(shift_amount > 32) {
                //This is not clear to me from the spec. Should this do the same as 32 or as 0? Go with zero for now
                shift_amount &= 0x1f;
            }
            if(shift_amount == 0) {
                if((instruction&0x10) == 0) {
                    //this means something weird. RRX
                    shift_c = source_val&1;
                    source_val = (source_val>>1) | (((cpu->regs.actual[PC]>>29)&1)<<31);
                }
            }
            else if (shift_amount < 32){
                shift_c = (source_val>>(shift_amount-1))&1;
                source_val = (source_val>>shift_amount) | (source_val<<(32-shift_amount));
            }
            else if (shift_amount == 32) {
                shift_c = source_val&0x80000000;
            }
            break;
        }
    }
    uint32_t op2;
    uint32_t op1;
    uint32_t carry = (cpu->regs.actual[PC]>>29)&1;
    uint32_t rn_val = *cpu->regs.effective[rn];
    switch(opcode) {
    case ALU_OPCODE_AND:
    case ALU_OPCODE_TST:
        result = rn_val & source_val;
        break;
    case ALU_OPCODE_EOR:
    case ALU_OPCODE_TEQ:
        result = rn_val ^ source_val;
        break;
    case ALU_OPCODE_SUB:
    case ALU_OPCODE_CMP:
        op2 = ~source_val;
        op1 = rn_val;
        result64 = ((uint64_t)op1) + op2 + 1;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        /*      ADDITION SIGN BITS */
        /*    num1sign num2sign sumsign */
        /*   --------------------------- */
        /*        0 0 0 */
        /* *OVER* 0 0 1 (adding two positives should be positive) */
        /*        0 1 0 */
        /*        0 1 1 */
        /*        1 0 0 */
        /*        1 0 1 */
        /* *OVER* 1 1 0 (adding two negatives should be negative) */
        /*        1 1 1 */

        arith_v = (op1^op2^0x80000000)&(op1^result)&0x80000000;
        break;
    case ALU_OPCODE_RSB:
        op1 = source_val;
        op2 = ~rn_val;
        result64 = ((uint64_t)op1) + op2 + 1;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        arith_v = (op1^op2^0x80000000)&(op1^result)&0x80000000;
        break;
    case ALU_OPCODE_ADD:
    case ALU_OPCODE_CMN:
        op2 = source_val;
        op1 = rn_val;
        result64 = ((uint64_t)op1) + op2;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        arith_v = (op1^op2^0x80000000)&(op1^result)&0x80000000;
        break;
    case ALU_OPCODE_ADC:
        op1 = rn_val;
        op2 = source_val;
        result64 = ((uint64_t)op1) + op2 + carry;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        arith_v = (op1^op2^0x80000000)&(op1^result)&0x80000000;
        break;
    case ALU_OPCODE_SBC:
        op1 = rn_val;
        op2 = ~source_val; 
        result64 = ((uint64_t)op1) + op2 + carry;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        arith_v = (op1^op2^0x80000000)&(op1^result)&0x80000000;
        break;
    case ALU_OPCODE_RSC:
        op1 = ~rn_val;
        op2 = source_val;
        result64 = ((uint64_t)op2) + op1 + carry;
        result = result64&0xffffffff;
        shift_c = result64>>32;
        arith_v = (result^rn_val)&0x80000000;
        break;
    case ALU_OPCODE_ORR:
        result = rn_val | source_val;
        break;
    case ALU_OPCODE_MOV:
        result = source_val;
        break;
    case ALU_OPCODE_BIC:
        result = rn_val & (~source_val);
        break;
    case ALU_OPCODE_MVN:
        result = ~source_val;
        break;
    }
    if(rd == PC) {
        if(instruction&ALU_SETS_FLAGS) {
            //this means we update the whole register, except for prohibited flags in user mode
            if(GETMODE(cpu) == MODE_USR) {
                cpu->regs.actual[PC] = (cpu->regs.actual[PC]&PC_PROTECTED_BITS) | (result&PC_UNPROTECTED_BITS);
            }
            else {
                cpu->regs.actual[PC] = result;
            }
        }
        else {
            //Only update the PC part
            SETPC(cpu,result);
        }
    }
    else {
        if(instruction&ALU_SETS_FLAGS) {
            uint32_t n = (result&FLAG_N);
            uint32_t z = result == 0 ? FLAG_Z : 0;
            uint32_t c = shift_c ? FLAG_C : 0;
            uint32_t v = arith_v ? FLAG_V : 0;
            cpu->regs.actual[PC] = (cpu->regs.actual[PC]&0x0fffffff)|n|z|c|v;
        }
        if((opcode&0xc) != 0x8) {
            *cpu->regs.effective[rd] = result;
        }
    }
    
    LOG("%s r%d %08x %08x\n",__func__,rd,*cpu->regs.effective[rd],cpu->regs.actual[PC]);
    return EXCEPT_NONE;
}
armv2exception_t MultiplyInstruction                    (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t SwapInstruction                        (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t SingleDataTransferInstruction          (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t BranchInstruction                      (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    if((instruction>>24&1)) {
        *cpu->regs.effective[LR] = cpu->pc-4;
    }
    cpu->pc = cpu->pc + 8 + ((instruction&0xffffff)<<2) - 4; 
    //+8 due to the weird prefetch thing, -4 for the hack as we're going to add 4 in the next loop

    return EXCEPT_NONE;
}
armv2exception_t MultiDataTransferInstruction           (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t SoftwareInterruptInstruction           (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t CoprocessorDataTransferInstruction     (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t CoprocessorRegisterTransferInstruction (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
armv2exception_t CoprocessorDataOperationInstruction    (armv2_t *cpu,uint32_t instruction)
{
    LOG("%s\n",__func__);
    return EXCEPT_NONE;
}
