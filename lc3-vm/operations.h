#ifndef OPERATIONS
#define OPERATIONS

#include <stdint.h>

void op_add(uint16_t instr);
void op_ldi(uint16_t instr);
void op_and(uint16_t instr);
void op_br(uint16_t instr);
void op_jmp(uint16_t instr);
void op_jsr(uint16_t instr);
void op_ld(uint16_t instr);
void op_ldr(uint16_t instr);
void op_not(uint16_t instr);
void op_st(uint16_t instr);
void op_str(uint16_t instr);
void op_sti(uint16_t instr);
void op_lea(uint16_t instr);

void trap_puts();
void trap_getc();
void trap_out();
void trap_in();
void trap_putsp();
void trap_halt();

#endif // OPERATIONS
