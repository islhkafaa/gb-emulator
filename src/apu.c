#include "../include/apu.h"
#include "../include/gb.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static const float DUTY_TABLE[4][8] = {{0, 0, 0, 0, 0, 0, 0, 1},
                                       {1, 0, 0, 0, 0, 0, 0, 1},
                                       {1, 0, 0, 0, 0, 1, 1, 1},
                                       {0, 1, 1, 1, 1, 1, 1, 0}};

void apu_init(GB *gb) {
  memset(&gb->apu, 0, sizeof(APU));
  gb->apu.nr52 = 0x00;

  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 44100;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;

  gb->apu.device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (gb->apu.device == 0) {
    fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
  } else {
    SDL_PauseAudioDevice(gb->apu.device, 0);
  }
}

void apu_quit(GB *gb) {
  if (gb->apu.is_recording) {
    apu_toggle_recording(gb);
  }
  if (gb->apu.device != 0) {
    SDL_CloseAudioDevice(gb->apu.device);
    gb->apu.device = 0;
  }
}

void apu_toggle_recording(GB *gb) {
  APU *apu = &gb->apu;
  if (!apu->is_recording) {
    apu->wav_file = fopen("gb_audio_capture.wav", "wb");
    if (!apu->wav_file) {
      fprintf(stderr, "Failed to open gb_audio_capture.wav for recording\n");
      return;
    }
    apu->wav_data_size = 0;

    u8 header[44] = {'R',  'I',  'F',  'F',  0,    0,   0,    0,    'W',
                     'A',  'V',  'E',  'f',  'm',  't', ' ',  16,   0,
                     0,    0,    3,    0,    2,    0,   0x44, 0xAC, 0x00,
                     0x00, 0x10, 0xB1, 0x05, 0x00, 8,   0,    32,   0,
                     'd',  'a',  't',  'a',  0,    0,   0,    0};
    fwrite(header, 1, 44, apu->wav_file);
    apu->is_recording = TRUE;
    printf("Started audio recording to gb_audio_capture.wav\n");
  } else {
    if (apu->wav_file) {
      u32 chunk_size = 36 + apu->wav_data_size;
      fseek(apu->wav_file, 4, SEEK_SET);
      fwrite(&chunk_size, 4, 1, apu->wav_file);

      fseek(apu->wav_file, 40, SEEK_SET);
      fwrite(&apu->wav_data_size, 4, 1, apu->wav_file);

      fclose(apu->wav_file);
      apu->wav_file = NULL;
    }
    apu->is_recording = FALSE;
    printf("Stopped audio recording. File size: %u bytes\n",
           apu->wav_data_size + 44);
  }
}

void apu_step(GB *gb, int m_cycles) {
  int cycles = m_cycles * 4;
  APU *apu = &gb->apu;

  if (apu->nr52 & 0x80) {
    apu->ch1.timer -= cycles;
    if (apu->ch1.timer <= 0) {
      apu->ch1.timer += (2048 - apu->ch1.period) * 4;
      apu->ch1.duty_pos = (apu->ch1.duty_pos + 1) & 0x07;
    }

    apu->ch2.timer -= cycles;
    if (apu->ch2.timer <= 0) {
      apu->ch2.timer += (2048 - apu->ch2.period) * 4;
      apu->ch2.duty_pos = (apu->ch2.duty_pos + 1) & 0x07;
    }

    apu->ch3.timer -= cycles;
    if (apu->ch3.timer <= 0) {
      apu->ch3.timer += (2048 - apu->ch3.period) * 2;
      apu->ch3.sample_pos = (apu->ch3.sample_pos + 1) % 32;
    }

    apu->ch4.timer -= cycles;
    if (apu->ch4.timer <= 0) {
      int divisor =
          apu->ch4.divisor_code == 0 ? 8 : (apu->ch4.divisor_code << 4);
      apu->ch4.timer += divisor << apu->ch4.clock_shift;
      int xor_bit = (apu->ch4.lfsr & 1) ^ ((apu->ch4.lfsr >> 1) & 1);
      apu->ch4.lfsr = (apu->ch4.lfsr >> 1) | (xor_bit << 14);
      if (apu->ch4.width_mode) {
        apu->ch4.lfsr = (apu->ch4.lfsr & ~(1 << 6)) | (xor_bit << 6);
      }
    }

    apu->frame_sequencer += cycles;
    if (apu->frame_sequencer >= 8192) {
      apu->frame_sequencer -= 8192;
      apu->cycles++;

      if ((apu->cycles & 1) == 0) {
        if (apu->ch1.length_enabled && apu->ch1.length_timer > 0) {
          if (--apu->ch1.length_timer == 0)
            apu->ch1.enabled = 0;
        }
        if (apu->ch2.length_enabled && apu->ch2.length_timer > 0) {
          if (--apu->ch2.length_timer == 0)
            apu->ch2.enabled = 0;
        }
        if (apu->ch3.length_enabled && apu->ch3.length_timer > 0) {
          if (--apu->ch3.length_timer == 0)
            apu->ch3.enabled = 0;
        }
        if (apu->ch4.length_enabled && apu->ch4.length_timer > 0) {
          if (--apu->ch4.length_timer == 0)
            apu->ch4.enabled = 0;
        }
      }
      if ((apu->cycles & 7) == 7) {
        if (apu->ch1.envelope_period > 0) {
          if (--apu->ch1.envelope_timer <= 0) {
            apu->ch1.envelope_timer = apu->ch1.envelope_period;
            if (apu->ch1.envelope_add && apu->ch1.volume < 15)
              apu->ch1.volume++;
            else if (!apu->ch1.envelope_add && apu->ch1.volume > 0)
              apu->ch1.volume--;
          }
        }
        if (apu->ch2.envelope_period > 0) {
          if (--apu->ch2.envelope_timer <= 0) {
            apu->ch2.envelope_timer = apu->ch2.envelope_period;
            if (apu->ch2.envelope_add && apu->ch2.volume < 15)
              apu->ch2.volume++;
            else if (!apu->ch2.envelope_add && apu->ch2.volume > 0)
              apu->ch2.volume--;
          }
        }
        if (apu->ch4.envelope_period > 0) {
          if (--apu->ch4.envelope_timer <= 0) {
            apu->ch4.envelope_timer = apu->ch4.envelope_period;
            if (apu->ch4.envelope_add && apu->ch4.volume < 15)
              apu->ch4.volume++;
            else if (!apu->ch4.envelope_add && apu->ch4.volume > 0)
              apu->ch4.volume--;
          }
        }
      }
      if ((apu->cycles & 3) == 2 && apu->ch1.sweep_enabled) {
        if (--apu->ch1.sweep_timer <= 0) {
          apu->ch1.sweep_timer =
              apu->ch1.sweep_period ? apu->ch1.sweep_period : 8;
          if (apu->ch1.sweep_period > 0) {
            int delta = apu->ch1.shadow_period >> apu->ch1.sweep_shift;
            int new_period = apu->ch1.shadow_period +
                             (apu->ch1.sweep_negate ? -delta : delta);
            if (new_period > 2047) {
              apu->ch1.enabled = 0;
            } else {
              apu->ch1.shadow_period = new_period;
              apu->ch1.period = new_period;
            }
          }
        }
      }
    }
  }

  float sample_interval = 4194304.0f / 44100.0f;
  apu->sample_timer += (float)cycles;

  while (apu->sample_timer >= sample_interval) {
    apu->sample_timer -= sample_interval;

    float out_l = 0.0f;
    float out_r = 0.0f;

    if (apu->nr52 & 0x80) {
      if (apu->ch1.enabled && apu->ch1.dac_enabled && (apu->nr51 & 0x11)) {
        float s = DUTY_TABLE[apu->ch1.duty][apu->ch1.duty_pos] ? 1.0f : -1.0f;
        float v = (float)apu->ch1.volume / 15.0f;
        float amt = s * v * 0.25f;
        if (apu->nr51 & 0x10)
          out_l += amt;
        if (apu->nr51 & 0x01)
          out_r += amt;
      }

      if (apu->ch2.enabled && apu->ch2.dac_enabled && (apu->nr51 & 0x22)) {
        float s = DUTY_TABLE[apu->ch2.duty][apu->ch2.duty_pos] ? 1.0f : -1.0f;
        float v = (float)apu->ch2.volume / 15.0f;
        float amt = s * v * 0.25f;
        if (apu->nr51 & 0x20)
          out_l += amt;
        if (apu->nr51 & 0x02)
          out_r += amt;
      }

      if (apu->ch3.enabled && apu->ch3.dac_enabled && (apu->nr51 & 0x44)) {
        u8 b = apu->ch3.wave_ram[apu->ch3.sample_pos / 2];
        u8 sample = (apu->ch3.sample_pos & 1) ? (b & 0x0F) : (b >> 4);
        static const int VOL_SHIFT[4] = {4, 0, 1, 2};
        sample >>= VOL_SHIFT[apu->ch3.volume_shift & 0x03];
        float amt = ((float)sample / 15.0f - 0.5f) * 0.25f;
        if (apu->nr51 & 0x40)
          out_l += amt;
        if (apu->nr51 & 0x04)
          out_r += amt;
      }

      if (apu->ch4.enabled && apu->ch4.dac_enabled && (apu->nr51 & 0x88)) {
        float s = (apu->ch4.lfsr & 1) ? -1.0f : 1.0f;
        float v = (float)apu->ch4.volume / 15.0f;
        float amt = s * v * 0.25f;
        if (apu->nr51 & 0x80)
          out_l += amt;
        if (apu->nr51 & 0x08)
          out_r += amt;
      }
    }

    float master_l = ((apu->nr50 >> 4) & 0x07) / 7.0f;
    float master_r = (apu->nr50 & 0x07) / 7.0f;

    apu->sample_buffer[apu->sample_count++] = out_l * master_l * 0.25f;
    apu->sample_buffer[apu->sample_count++] = out_r * master_r * 0.25f;

    if (apu->sample_count >= 1024) {
      if (apu->device) {
        SDL_QueueAudio(apu->device, apu->sample_buffer, 1024 * sizeof(float));
      }
      if (apu->is_recording && apu->wav_file) {
        fwrite(apu->sample_buffer, sizeof(float), 1024, apu->wav_file);
        apu->wav_data_size += sizeof(float) * 1024;
      }
      apu->sample_count = 0;
    }
  }
}

u8 apu_read(GB *gb, u16 addr) {
  APU *apu = &gb->apu;
  if (addr == 0xFF10)
    return (apu->ch1.sweep_period << 4) | (apu->ch1.sweep_negate << 3) |
           apu->ch1.sweep_shift;
  if (addr == 0xFF11)
    return (apu->ch1.duty << 6) | (64 - apu->ch1.length_timer);
  if (addr == 0xFF12)
    return (apu->ch1.initial_volume << 4) | (apu->ch1.envelope_add << 3) |
           apu->ch1.envelope_period;
  if (addr == 0xFF16)
    return (apu->ch2.duty << 6) | (64 - apu->ch2.length_timer);
  if (addr == 0xFF17)
    return (apu->ch2.initial_volume << 4) | (apu->ch2.envelope_add << 3) |
           apu->ch2.envelope_period;
  if (addr == 0xFF1C)
    return (apu->ch3.volume_shift << 5);
  if (addr == 0xFF21)
    return (apu->ch4.initial_volume << 4) | (apu->ch4.envelope_add << 3) |
           apu->ch4.envelope_period;
  if (addr == 0xFF24)
    return apu->nr50;
  if (addr == 0xFF25)
    return apu->nr51;
  if (addr == 0xFF26) {
    u8 ret = apu->nr52 & 0x80;
    if (apu->ch1.enabled)
      ret |= 0x01;
    if (apu->ch2.enabled)
      ret |= 0x02;
    if (apu->ch3.enabled)
      ret |= 0x04;
    if (apu->ch4.enabled)
      ret |= 0x08;
    return ret | 0x70;
  }
  if (addr >= 0xFF30 && addr <= 0xFF3F) {
    return apu->ch3.wave_ram[addr - 0xFF30];
  }
  return 0xFF;
}

void apu_write(GB *gb, u16 addr, u8 val) {
  APU *apu = &gb->apu;
  if (addr == 0xFF26) {
    apu->nr52 = val & 0x80;
    if (!(apu->nr52)) {
      memset(&apu->ch1, 0, sizeof(SquareChannel));
      memset(&apu->ch2, 0, sizeof(SquareChannel));
      memset(&apu->ch3, 0, sizeof(WaveChannel));
      memset(&apu->ch4, 0, sizeof(NoiseChannel));
      apu->nr50 = 0;
      apu->nr51 = 0;
    }
    return;
  }

  if (addr >= 0xFF30 && addr <= 0xFF3F) {
    apu->ch3.wave_ram[addr - 0xFF30] = val;
    return;
  }

  if (!(apu->nr52 & 0x80))
    return;

  if (addr == 0xFF24) {
    apu->nr50 = val;
    return;
  }
  if (addr == 0xFF25) {
    apu->nr51 = val;
    return;
  }

  if (addr == 0xFF10) {
    apu->ch1.sweep_period = (val >> 4) & 0x07;
    apu->ch1.sweep_negate = (val & 0x08) != 0;
    apu->ch1.sweep_shift = val & 0x07;
  } else if (addr == 0xFF11) {
    apu->ch1.duty = val >> 6;
    apu->ch1.length_timer = 64 - (val & 0x3F);
  } else if (addr == 0xFF12) {
    apu->ch1.initial_volume = val >> 4;
    apu->ch1.envelope_add = (val & 0x08) != 0;
    apu->ch1.envelope_period = val & 0x07;
    apu->ch1.dac_enabled = (val & 0xF8) != 0;
    if (!apu->ch1.dac_enabled)
      apu->ch1.enabled = 0;
  } else if (addr == 0xFF13) {
    apu->ch1.period = (apu->ch1.period & 0x0700) | val;
  } else if (addr == 0xFF14) {
    apu->ch1.period = (apu->ch1.period & 0x00FF) | ((val & 0x07) << 8);
    bool_t was_len_enabled = apu->ch1.length_enabled;
    apu->ch1.length_enabled = (val & 0x40) != 0;
    if (!was_len_enabled && apu->ch1.length_enabled && (apu->cycles & 1) != 0) {
      if (apu->ch1.length_timer > 0 && --apu->ch1.length_timer == 0)
        apu->ch1.enabled = 0;
    }
    if (val & 0x80) {
      if (apu->ch1.dac_enabled)
        apu->ch1.enabled = 1;
      if (apu->ch1.length_timer == 0)
        apu->ch1.length_timer = 64;
      apu->ch1.timer = (2048 - apu->ch1.period) * 4;
      apu->ch1.duty_pos = 0;
      apu->ch1.volume = apu->ch1.initial_volume;
      apu->ch1.envelope_timer = apu->ch1.envelope_period;
      apu->ch1.shadow_period = apu->ch1.period;
      apu->ch1.sweep_timer = apu->ch1.sweep_period ? apu->ch1.sweep_period : 8;
      apu->ch1.sweep_enabled =
          (apu->ch1.sweep_period > 0 || apu->ch1.sweep_shift > 0);
      if (apu->ch1.sweep_shift > 0) {
        int delta = apu->ch1.shadow_period >> apu->ch1.sweep_shift;
        int new_p =
            apu->ch1.shadow_period + (apu->ch1.sweep_negate ? -delta : delta);
        if (new_p > 2047)
          apu->ch1.enabled = 0;
      }
    }
  }

  else if (addr == 0xFF16) {
    apu->ch2.duty = val >> 6;
    apu->ch2.length_timer = 64 - (val & 0x3F);
  } else if (addr == 0xFF17) {
    apu->ch2.initial_volume = val >> 4;
    apu->ch2.envelope_add = (val & 0x08) != 0;
    apu->ch2.envelope_period = val & 0x07;
    apu->ch2.dac_enabled = (val & 0xF8) != 0;
    if (!apu->ch2.dac_enabled)
      apu->ch2.enabled = 0;
  } else if (addr == 0xFF18) {
    apu->ch2.period = (apu->ch2.period & 0x0700) | val;
  } else if (addr == 0xFF19) {
    apu->ch2.period = (apu->ch2.period & 0x00FF) | ((val & 0x07) << 8);
    bool_t was_len_enabled = apu->ch2.length_enabled;
    apu->ch2.length_enabled = (val & 0x40) != 0;
    if (!was_len_enabled && apu->ch2.length_enabled && (apu->cycles & 1) != 0) {
      if (apu->ch2.length_timer > 0 && --apu->ch2.length_timer == 0)
        apu->ch2.enabled = 0;
    }
    if (val & 0x80) {
      if (apu->ch2.dac_enabled)
        apu->ch2.enabled = 1;
      if (apu->ch2.length_timer == 0)
        apu->ch2.length_timer = 64;
      apu->ch2.timer = (2048 - apu->ch2.period) * 4;
      apu->ch2.duty_pos = 0;
      apu->ch2.volume = apu->ch2.initial_volume;
      apu->ch2.envelope_timer = apu->ch2.envelope_period;
    }
  }

  else if (addr == 0xFF1A) {
    apu->ch3.dac_enabled = (val & 0x80) != 0;
    if (!apu->ch3.dac_enabled)
      apu->ch3.enabled = 0;
  } else if (addr == 0xFF1B) {
    apu->ch3.length_timer = 256 - val;
  } else if (addr == 0xFF1C) {
    apu->ch3.volume_shift = (val >> 5) & 0x03;
  } else if (addr == 0xFF1D) {
    apu->ch3.period = (apu->ch3.period & 0x0700) | val;
  } else if (addr == 0xFF1E) {
    apu->ch3.period = (apu->ch3.period & 0x00FF) | ((val & 0x07) << 8);
    bool_t was_len_enabled = apu->ch3.length_enabled;
    apu->ch3.length_enabled = (val & 0x40) != 0;
    if (!was_len_enabled && apu->ch3.length_enabled && (apu->cycles & 1) != 0) {
      if (apu->ch3.length_timer > 0 && --apu->ch3.length_timer == 0)
        apu->ch3.enabled = 0;
    }
    if (val & 0x80) {
      if (apu->ch3.dac_enabled)
        apu->ch3.enabled = 1;
      if (apu->ch3.length_timer == 0)
        apu->ch3.length_timer = 256;
      apu->ch3.timer = (2048 - apu->ch3.period) * 2;
      apu->ch3.sample_pos = 0;
    }
  }

  else if (addr == 0xFF20) {
    apu->ch4.length_timer = 64 - (val & 0x3F);
  } else if (addr == 0xFF21) {
    apu->ch4.initial_volume = val >> 4;
    apu->ch4.envelope_add = (val & 0x08) != 0;
    apu->ch4.envelope_period = val & 0x07;
    apu->ch4.dac_enabled = (val & 0xF8) != 0;
    if (!apu->ch4.dac_enabled)
      apu->ch4.enabled = 0;
  } else if (addr == 0xFF22) {
    apu->ch4.clock_shift = val >> 4;
    apu->ch4.width_mode = (val & 0x08) != 0;
    apu->ch4.divisor_code = val & 0x07;
  } else if (addr == 0xFF23) {
    bool_t was_len_enabled = apu->ch4.length_enabled;
    apu->ch4.length_enabled = (val & 0x40) != 0;
    if (!was_len_enabled && apu->ch4.length_enabled && (apu->cycles & 1) != 0) {
      if (apu->ch4.length_timer > 0 && --apu->ch4.length_timer == 0)
        apu->ch4.enabled = 0;
    }
    if (val & 0x80) {
      if (apu->ch4.dac_enabled)
        apu->ch4.enabled = 1;
      if (apu->ch4.length_timer == 0)
        apu->ch4.length_timer = 64;
      apu->ch4.volume = apu->ch4.initial_volume;
      apu->ch4.envelope_timer = apu->ch4.envelope_period;
      apu->ch4.lfsr = 0x7FFF;
      int divisor =
          apu->ch4.divisor_code == 0 ? 8 : (apu->ch4.divisor_code << 4);
      apu->ch4.timer = divisor << apu->ch4.clock_shift;
    }
  }
}
