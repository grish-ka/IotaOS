#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remaps the master and slave PICs to offsets 32 and 40 */
void pic_remap(void);

#endif /* PIC_H */