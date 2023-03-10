#include "helper.h"
#include "global.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

uint16_t sign_extend(uint16_t n, int bit_cnt)
{
	return (n >> (bit_cnt - 1)) & 1 ? n | (0xFFFF << bit_cnt) : n;
}

void update_flags(uint16_t r)
{
	regs[R_COND] = regs[r] == 0 ? FL_ZRO : regs[r] < 0 ? FL_NEG : FL_POS;
}

uint16_t swap16(int x)
{
	return (x >> 8) | (x << 8);
}

int read_img(const char *path)
{
	FILE *file = fopen(path, "r");
	if (!file)
		return 0;
	read_img_file(file);
	fclose(file);
	return 1;
}

void read_img_file(FILE *file)
{
	uint16_t origin;
	origin = fread(&origin, sizeof(origin), 1, file);
	origin = swap16(origin);

	uint16_t max_read = MEMORY_SIZE - origin;
	uint16_t *p = memory + origin;
	size_t read = fread(p, sizeof(uint16_t), max_read, file);

	while (read--) {
		*p = swap16(*p);
		p++;
	}
}

void mem_write(uint16_t address, uint16_t val)
{
	memory[address] = val;
}

int mem_read(uint16_t address)
{
	if (address == MR_KBSR) {
		if (check_key()) {
			memory[MR_KBSR] = (1 << 15);
			memory[MR_KBDR] = getchar();
		} else {
			memory[MR_KBSR] = 0;
		}
	}
	return memory[address];
}

struct termios orig_tio;
void disable_ip_buffering()
{
	tcgetattr(STDIN_FILENO, &orig_tio);
	struct termios new_tio = orig_tio;
	new_tio.c_lflag = ~ICANON & ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_ip_buffering()
{
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
}

uint16_t check_key()
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

void handle_interrupt_sig(int signal)
{
	restore_ip_buffering();
	printf("\n");
	exit(-2);
}
