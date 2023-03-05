#ifndef HELPER
#define HELPER

uint16_t sign_extend(uint16_t n, int bit_cnt);
void update_flags(uint16_t r);
uint16_t swap16(int x);

int read_img(const char *path);
void read_img_file(FILE *file);

void mem_write(uint16_t address, uint16_t val);
int mem_read(uint16_t address);

#endif // HELPER
