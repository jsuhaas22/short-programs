#ifndef GLOBAL
#define GLOBAL

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
	FL_POS = 1 << 0;
	FL_ZRO = 1 << 1;
	FL_NEG = 1 << 2;
};

enum {
	OP_BR = 0,
	OP_ADD,
	OP_LD,
	OP_ST,
	OP_JSR,
	OP_AND,
	OP_LDR,
	OP_STR,
	OP_RTI,
	OP_NOT,
	OP_LDI,
	OP_STI,
	OP_JMP,
	OP_RES,
	OP_LEA,
	OP_TRAP,
};

extern uint16_t regs[R_COUNT];

enum {
	TRAP_GETC = 0x20,
	TRAP_OUT,
	TRAP_IN,
	TRAP_PUTSP,
	TRAP_HALT,
};

#define MEMORY_SIZE (1 << 16)
extern uint16_t memory[MEMORY_SIZE];

enum {
	MR_KBSR = 0xFE00,
	MR_KBDR = 0xFE02,
};

#endif // GLOBAL
