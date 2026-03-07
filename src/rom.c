#include "../include/rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const u8 NINTENDO_LOGO[ROM_NINTENDO_LOGO_LEN] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E};

int rom_load(const char *path, u8 **out_data, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "rom_load: cannot open '%s'\n", path);
    return 0;
  }

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);

  if (sz < ROM_MIN_SIZE) {
    fprintf(stderr, "rom_load: file too small (%ld bytes, need %d)\n", sz,
            ROM_MIN_SIZE);
    fclose(f);
    return 0;
  }

  u8 *data = malloc((size_t)sz);
  if (!data) {
    fprintf(stderr, "rom_load: out of memory\n");
    fclose(f);
    return 0;
  }

  if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
    fprintf(stderr, "rom_load: read error\n");
    free(data);
    fclose(f);
    return 0;
  }
  fclose(f);

  if (memcmp(data + ROM_NINTENDO_LOGO_START, NINTENDO_LOGO,
             ROM_NINTENDO_LOGO_LEN) != 0) {
    fprintf(stderr, "rom_load: Nintendo logo mismatch (non-standard ROM)\n");
  }

  *out_data = data;
  *out_size = (size_t)sz;
  return 1;
}

void rom_free(u8 *data) { free(data); }

void rom_print_header(const u8 *rom) {
  char title[ROM_HEADER_TITLE_LEN + 1];
  memcpy(title, rom + ROM_HEADER_TITLE_START, ROM_HEADER_TITLE_LEN);
  title[ROM_HEADER_TITLE_LEN] = '\0';

  u8 checksum = 0;
  for (int i = 0x0134; i <= 0x014C; i++) {
    checksum = checksum - rom[i] - 1;
  }
  u8 stored = rom[ROM_HEADER_CHECKSUM];

  static const char *type_names[] = {"ROM ONLY", "MBC1", "MBC1+RAM",
                                     "MBC1+RAM+BATTERY"};
  const char *type_str =
      (rom[ROM_HEADER_TYPE] < 4) ? type_names[rom[ROM_HEADER_TYPE]] : "UNKNOWN";

  printf("Title    : %.16s\n", title);
  printf("Type     : 0x%02X (%s)\n", rom[ROM_HEADER_TYPE], type_str);
  printf("ROM size : code 0x%02X (%d KiB)\n", rom[ROM_HEADER_ROM_SIZE],
         32 << rom[ROM_HEADER_ROM_SIZE]);
  printf("RAM size : code 0x%02X\n", rom[ROM_HEADER_RAM_SIZE]);
  printf("Checksum : stored=0x%02X computed=0x%02X [%s]\n", stored, checksum,
         (stored == checksum) ? "OK" : "FAIL");
}
