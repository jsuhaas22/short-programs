#include <stdio.h>

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

uint16_t regs[R_COUNT];

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

int main(int argc, char **args)
{
	if (argc < 2) {
		printf("Specify input file name.\n");
		exit(1);
	}

	for (int i = 0; i < argc; ++i) {
		if (!read_file(args[i])) {
			printf("Could not read file %s\n. Please ensure it exists.\n", args[i]);
			exit(1);
		} else {
			printf("Successfully loaded file %s\n.", args[i]);
		}
	}

	regs[R_COND] = FL_ZRO;
	regs[R_PC] = 0x3000;

	while (1) {
		uint16_t instr = mem_read(regs[R_PC]++);
		uint16_t opcode = instr >> 12;

		switch (opcode) {
			case OP_ADD:
				break;
			case OP_AND:
				break;
			case OP_NOT:
				break;
			default:
				printf("Bad instruction\n");
				break;
		}
	}
}
