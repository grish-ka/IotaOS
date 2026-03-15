/*
* IotaOS - task.h
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#ifndef TASK_H
#define TASK_H

#include <stdint.h>

void task_init(void);
void create_task(void (*entry)());
uint32_t task_schedule(uint32_t current_esp);

#endif