#ifndef HELPER
#define HELPER

uint16_t sign_extend(uint16_t n, int bit_cnt);
void update_flags(uint16_t r);
uint16_t swap16(int x);

int read_img(const char *path);
void read_img_file(FILE *file);

void mem_write(uint16_t address, uint16_t val);
int mem_read(uint16_t address);

/* unix specific setting of terminal and I/O to appropriate values */
void disable_ip_buffering();
void restore_ip_buffering();
uint16_t check_key();
void handle_interrupt_sig(int signal);

#endif // HELPER
