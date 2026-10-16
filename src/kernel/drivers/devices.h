/*
 * IotaOS - devices.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 */

#ifndef DEVICES_H
#define DEVICES_H

#include "fs/vfs.h"

/* Populate /dev; returns the console node */
vfs_node_t* devices_init(void);

#endif
