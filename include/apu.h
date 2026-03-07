#ifndef GB_APU_H
#define GB_APU_H

#include "types.h"
#include <SDL2/SDL.h>

struct GB;

typedef struct {
  u8 enabled;
  u8 dac_enabled;
  u8 duty;
  int timer;
  int period;
  int volume;
  int initial_volume;
  u8 envelope_add;
  int envelope_period;
  int envelope_timer;
  int length_timer;
  int length_enabled;
  int duty_pos;
  int sweep_period;
  int sweep_shift;
  u8 sweep_negate;
  int sweep_timer;
  u8 sweep_enabled;
  int shadow_period;
} SquareChannel;

typedef struct {
  u8 enabled;
  u8 dac_enabled;
  int timer;
  int period;
  int length_timer;
  int length_enabled;
  u8 volume_shift;
  u8 wave_ram[16];
  int sample_pos;
} WaveChannel;

typedef struct {
  u8 enabled;
  u8 dac_enabled;
  int timer;
  int length_timer;
  int length_enabled;
  int volume;
  int initial_volume;
  u8 envelope_add;
  int envelope_period;
  int envelope_timer;
  u16 lfsr;
  u8 clock_shift;
  u8 width_mode;
  u8 divisor_code;
} NoiseChannel;

typedef struct {
  SquareChannel ch1;
  SquareChannel ch2;
  WaveChannel ch3;
  NoiseChannel ch4;

  SDL_AudioDeviceID device;
  int cycles;
  int frame_sequencer;

  u8 nr50;
  u8 nr51;
  u8 nr52;

  float sample_buffer[4096];
  int sample_count;
  float sample_timer;
} APU;

void apu_init(struct GB *gb);
void apu_step(struct GB *gb, int cycles);
void apu_quit(struct GB *gb);

u8 apu_read(struct GB *gb, u16 addr);
void apu_write(struct GB *gb, u16 addr, u8 val);

#endif
