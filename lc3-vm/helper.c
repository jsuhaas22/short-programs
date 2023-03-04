#include "helper.h"

uint16_t sign_extend(uint16_t n, int bit_cnt)
{
	return (n >> (bit_cnt - 1)) & 1 ? n | (0xFFFF << bit_cnt) : n;
}

void update_flag(uint16_t r)
{
	regs[R_COND] = regs[r] == 0 ? FL_ZERO : regs[r] < 0 ? FL_NEG : FL_POS;
}
