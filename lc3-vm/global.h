#ifndef GLOBAL
#define GLOBAL

#include <stdint.h>

enum {
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC,
	R_COND,
	R_COUNT,
};

enum {
	FL_POS = 1 << 0,
	FL_ZRO = 1 << 1,
	FL_NEG = 1 << 2,
};

enum {
	OP_BR = 0, /* branch */
	OP_ADD, /* add */
	OP_LD, /* load */
	OP_ST, /* store */
	OP_JSR, /* jump register */
	OP_AND, /* and */
	OP_LDR, /* load register */
	OP_STR, /* store register */
	OP_RTI, /* reserved */
	OP_NOT, /* bitwise not */
	OP_LDI, /* load indirect */
	OP_STI, /* store indirect */
	OP_JMP, /* jump */
	OP_RES, /* reserved */
	OP_LEA, /* load effective address */
	OP_TRAP /* execute a trap routine */
};

extern uint16_t regs[R_COUNT];

enum {
	TRAP_GETC = 0x20, /* get a character without echoing */
	TRAP_OUT = 0x21, /* output a character to console */
	TRAP_PUTS = 0x22, /* output a word string to a console */
	TRAP_IN = 0x23, /* get a character while echoing */
	TRAP_PUTSP = 0x24, /* output a byte string to the console */
	TRAP_HALT = 0x25, /* stop the program */
};

#define MEMORY_SIZE (1 << 16)
extern uint16_t memory[MEMORY_SIZE];

enum {
	MR_KBSR = 0xFE00,
	MR_KBDR = 0xFE02,
};

#endif // GLOBAL
