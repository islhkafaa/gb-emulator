#ifndef GB_ROM_H
#define GB_ROM_H

#include "types.h"
#include <stddef.h>

#define ROM_HEADER_TITLE_START 0x0134
#define ROM_HEADER_TITLE_LEN 16
#define ROM_HEADER_TYPE 0x0147
#define ROM_HEADER_ROM_SIZE 0x0148
#define ROM_HEADER_RAM_SIZE 0x0149
#define ROM_HEADER_CHECKSUM 0x014D
#define ROM_NINTENDO_LOGO_START 0x0104
#define ROM_NINTENDO_LOGO_LEN 48
#define ROM_MIN_SIZE 0x8000

typedef struct {
  u8 title[ROM_HEADER_TITLE_LEN + 1];
  u8 type;
  u8 rom_size_code;
  u8 ram_size_code;
  u8 header_checksum;
} ROMHeader;

int rom_load(const char *path, u8 **out_data, size_t *out_size);
void rom_free(u8 *data);
void rom_print_header(const u8 *rom);

#endif
