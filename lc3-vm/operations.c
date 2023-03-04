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
