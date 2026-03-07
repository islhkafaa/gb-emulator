#include "../include/gb.h"
#include <stdio.h>
#include <string.h>

#define STATE_MAGIC 0x47425353
#define STATE_VERSION 2

typedef struct {
  u32 magic;
  u32 version;
} StateHeader;

typedef struct {
  CPU cpu;
  PPU ppu;
  Memory mem;
  APU apu;
  Joypad joypad;
  int div_counter;
  int timer_counter;
  int tima_overflow_delay;
} SaveData;

static void state_path(const GB *gb, char *out, size_t len) {
  snprintf(out, len, "%s.state", gb->rom_path);
}

void gb_savestate(GB *gb) {
  char path[1088];
  state_path(gb, path, sizeof(path));

  FILE *f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "savestate: cannot open %s\n", path);
    return;
  }

  StateHeader hdr = {STATE_MAGIC, STATE_VERSION};
  fwrite(&hdr, sizeof(hdr), 1, f);

  SaveData sd;
  sd.cpu = gb->cpu;
  sd.ppu = gb->ppu;
  sd.mem = gb->mem;
  sd.apu = gb->apu;
  sd.joypad = gb->joypad;
  sd.div_counter = gb->div_counter;
  sd.timer_counter = gb->timer_counter;
  sd.tima_overflow_delay = gb->tima_overflow_delay;
  sd.mem.gb_ptr = NULL;

  SDL_AudioDeviceID dev = sd.apu.device;
  sd.apu.device = 0;
  (void)dev;

  fwrite(&sd, sizeof(sd), 1, f);
  fclose(f);
}

void gb_loadstate(GB *gb) {
  char path[1088];
  state_path(gb, path, sizeof(path));

  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "savestate: no state at %s\n", path);
    return;
  }

  StateHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != STATE_MAGIC ||
      hdr.version != STATE_VERSION) {
    fprintf(stderr, "savestate: invalid or incompatible state file\n");
    fclose(f);
    return;
  }

  SaveData sd;
  if (fread(&sd, sizeof(sd), 1, f) != 1) {
    fprintf(stderr, "savestate: truncated state file\n");
    fclose(f);
    return;
  }
  fclose(f);

  SDL_AudioDeviceID dev = gb->apu.device;
  void *gb_ptr = gb->mem.gb_ptr;

  gb->cpu = sd.cpu;
  gb->ppu = sd.ppu;
  gb->mem = sd.mem;
  gb->apu = sd.apu;
  gb->joypad = sd.joypad;
  gb->div_counter = sd.div_counter;
  gb->timer_counter = sd.timer_counter;
  gb->tima_overflow_delay = sd.tima_overflow_delay;

  gb->apu.device = dev;
  gb->mem.gb_ptr = gb_ptr;
}
