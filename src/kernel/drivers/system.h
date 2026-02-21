/* 
 * IotaOS - system.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

void panic(const char* message);

void shutdown(void);
void reboot(void);


#endif /* SYSTEM_H */