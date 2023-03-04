#include "global.h"
#include "operations.h"
#include <stdio.h>

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

	uint16_t regs[R_COUNT];

	regs[R_COND] = FL_ZRO;
	regs[R_PC] = 0x3000;

	while (1) {
		uint16_t instr = mem_read(regs[R_PC]++);
		uint16_t opcode = instr >> 12;

		switch (opcode) {
			case OP_ADD:
				op_add(instr);
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
