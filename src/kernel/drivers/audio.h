/*
 * IotaOS - audio.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

struct vfs_node;

/* Probe the SB16, allocate the DMA ring and register /dev/dsp, /dev/audio
   and /dev/mixer under dev (the /dev directory node). Silent no-op on
   machines without the card; the PC speaker works regardless. */
void audio_init(struct vfs_node* dev);

/* IRQ5 (DSP) handler, called from irq_handler with the vector 37 */
void audio_irq(void);

/* PIT channel-2 PC speaker (port 0x61 gate) */
void speaker_beep(uint32_t freq_hz);
void speaker_off(void);

/* Terminal bell: '\a' → 200 ms of tone, auto-off driven by audio_tick()
   (call from the timer handler). Safe from IRQ context. */
void audio_bell(void);
void audio_tick(void);

#endif /* AUDIO_H */
