#include "../include/gb.h"
#include "../include/rom.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: gb <rom.gb>\n");
    return 1;
  }

  GB *gb = calloc(1, sizeof(GB));
  if (!gb) {
    return 1;
  }

  if (!rom_load(argv[1], &gb->rom, &gb->rom_size)) {
    free(gb);
    return 1;
  }

  rom_print_header(gb->rom);

  printf("DEBUG: calling gb_init\n");
  if (!gb_init(gb, argv[1])) {
    fprintf(stderr, "gb_init failed\n");
    rom_free(gb->rom);
    free(gb);
    return 1;
  }

  gb_run(gb);
  gb_quit(gb);
  rom_free(gb->rom);
  free(gb);

  return 0;
}
