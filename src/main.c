#include "../include/gb.h"
#include "../include/rom.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: gb <rom.gb>\n");
    return 1;
  }

  GB gb = {0};

  if (!rom_load(argv[1], &gb.rom, &gb.rom_size)) {
    return 1;
  }

  rom_print_header(gb.rom);

  if (!gb_init(&gb)) {
    fprintf(stderr, "gb_init failed\n");
    rom_free(gb.rom);
    return 1;
  }

  gb_run(&gb);
  gb_quit(&gb);
  rom_free(gb.rom);

  return 0;
}
