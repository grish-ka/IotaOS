/*
 * IotaOS - ata.h
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Primary ATA / IDE Hard Disk Controller Driver (PIO Mode).
 * Supports persistent sector read and write for /dev/hda.
 */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>

#define ATA_SECTOR_SIZE 512

typedef struct {
    bool present;
    char model[41];
    char serial[21];
    uint32_t sectors;      /* Total LBA28 sectors */
    uint32_t size_mb;      /* Total disk size in Megabytes */
} ata_device_info_t;

extern ata_device_info_t ata_primary_master;

void ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);
int ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer);

#endif /* ATA_H */
