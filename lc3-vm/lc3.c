#include "global.h"
#include "operations.h"
#include <stdio.h>
#include <stdlib.h>

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

	signal(SIGINT, handle_interrupt_sig);
	disable_ip_buffering();

	uint16_t regs[R_COUNT];

	regs[R_COND] = FL_ZRO;
	regs[R_PC] = 0x3000;

	int running = 1;
	while (running) {
		uint16_t instr = mem_read(regs[R_PC]++);
		uint16_t opcode = instr >> 12;

		switch (opcode) {
			case OP_ADD:
				op_add(instr);
				break;
			case OP_LDI:
				op_ldi(instr);
				break;
			case OP_AND:
				op_and(instr);
				break;
			case OP_BR:
				op_br(instr);
				break;
			case OP_JMP:
				op_jmp(instr);
				break;
			case OP_JSR:
				op_jsr(instr);
				break;
			case OP_LD:
				op_ld(instr);
				break;
			case OP_LDR:
				op_ldr(instr);
				break;
			case OP_NOT:
				op_not(instr);
				break;
			case OP_ST:
				op_st(instr);
				break;
			case OP_STR:
				op_str(instr);
				break;	
			case OP_STI:
				op_sti(instr);
				break;
			case OP_TRAP:
				switch(instr & 0xFF) {
					case TRAP_GETC:
						trap_getc();
						break;
					case TRAP_OUT:
						trap_out();
						break;
					case TRAP_IN:
						trap_in();
						break;
					case TRAP_PUTS:
						trap_puts();
						break;
					case TRAP_PUTSP:
						trap_putsp();
						break;
					case TRAP_HALT:
						trap_halt();
						running = 0;
						break;
				}
			case OP_RES:
			case OP_RTI:
			default:
				printf("Bad instruction\n");
				abort();
				break;
		}
	}
	restore_ip_buffering();
}
