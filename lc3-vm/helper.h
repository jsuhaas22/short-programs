#ifndef HELPER
#define HELPER

uint16_t sign_extend(uint16_t n, int bit_cnt);
void update_flags(uint16_t r);
uint16_t swap16(int x);

int read_img(const char *path);
void read_img_file(FILE *file);

#endif // HELPER
