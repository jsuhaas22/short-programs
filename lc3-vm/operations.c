#include "operations.h"
#include "helper.h"
#include "global.h"

int regs[R_COUNT];

void op_add(uint16_t instr)
{
	uint16_t dst_r = (instr >> 9) & 0x07;
	uint16_t src_r = (instr >> 6) & 0x07;
	if ((instr >> 5) & 0x01) {
		uint16_t imm = sign_extend(instr & 0x1F, 5);
		regs[dst_r] = regs[src_r] + imm;
	} else {
		uint16_t src2_r = instr & 0x07;
		regs[dst_r] = regs[src_r] + regs[src2_r];
	}
	update_flags(dst_r);
}

void op_ldi(uint16_t instr)
{
	uint16_t dst_r = (instr >> 9) & 0x07;
	uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
	regs[dst_r] = mem_read(mem_read(regs[R_PC] + pc_offset));
	update_flag(dst_r);
}

void op_and(uint16_t instr)
{
	uint16_t dst_r = (instr >> 9) & 0x07;
	uint16_t src_r = (instr >> 6) & 0x07;
	if ((instr >> 5) & 0x01) {
		uint16_t imm = sign_extend(instr & 0x1F, 5);
		regs[dst_r] = regs[src_r] & imm;
	} else {
		uint16_t src2_r = instr & 0x07;
		regs[dst_r] = regs[src_r] & src2_r;
	}
	update_flags(dst_r);
}

void op_br(uint16_t instr)
{
	uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
	uint16_t cond = (instr >> 9) & 0x07;
	if (cond & regs[R_COND])
		regs[PC] += pc_offset;
}

void op_jmp(uint16_t instr)
{
	regs[PC] = (instr >> 6) & 0x07;
}

void op_jsr(uint16_t instr)
{
	regs[R_R7] = regs[R_PC];
	/* JSR or JSRR */
	regs[R_PC] = ((instr >> 6) & 0x01) ? regs[R_PC] + sign_extend(instr >> 0x7FF, 11) : (instr >> 6) & 0x07;
}

void op_ld(uint16_t instr)
{
	uint16_t dst_r = (instr >> 9) & 0x07;
	uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
	regs[dst_r] = mem_read(regs[R_PC] + pc_offset);
	update_flag(dst_r);
}

void op_ldr(uint16_t instr)
{
	uint16_t dst_r = (instr >> 9) & 0x07;
	uint16_t base_r = (instr >> 6) & 0x07;
	uint16_t pc_offset = sign_extend(instr & 0x3F, 5);
	regs[dst_r] = mem_read(regs[base_r] + pc_offset);
	update_flag(dst_r);
}
