/* 
 * IotaOS - pic.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remaps the master and slave PICs to offsets 32 and 40 */
void pic_remap(void);

/* Enable (masked=0) or disable (masked!=0) one IRQ line (0-15) */
void pic_irq_mask(uint8_t irq, int masked);

#endif /* PIC_H */