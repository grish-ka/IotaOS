/*
 * IotaOS - task.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>

void task_init(void);
/* NEW: Takes the app's isolated Page Directory as an argument */
int create_task(void (*entry)(), uint32_t* page_directory);
uint32_t task_schedule(uint32_t current_esp);

#endif