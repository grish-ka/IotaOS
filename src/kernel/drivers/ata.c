/*
 * IotaOS - ata.c
 * Copyright (c) 2026 grish-ka
 * Licensed under the MIT License.
 *
 * Primary ATA / IDE Hard Disk Controller Driver (PIO Mode).
 * Supports persistent sector read and write for /dev/hda.
 */

#include "drivers/ata.h"
#include "drivers/io.h"
#include "drivers/stdio.h"
#include "drivers/string.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SEC_COUNT    0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRV_HEAD     0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_CMD          0x1F7
#define ATA_PRIMARY_CONTROL      0x3F6

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_IDENTIFY         0xEC
#define ATA_CMD_FLUSH            0xE7

#define ATA_SR_BSY               0x80    /* Busy */
#define ATA_SR_DRDY              0x40    /* Drive ready */
#define ATA_SR_DF                0x20    /* Drive write fault */
#define ATA_SR_DSC               0x10    /* Drive seek complete */
#define ATA_SR_DRQ               0x08    /* Data request ready */
#define ATA_SR_CORR              0x04    /* Corrected data */
#define ATA_SR_IDX               0x02    /* Index */
#define ATA_SR_ERR               0x01    /* Error */

ata_device_info_t ata_primary_master = {0};

static void ata_io_wait(void) {
    /* Reading the alternate status port 4 times provides an ~400ns delay */
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
    inb(ATA_PRIMARY_CONTROL);
}

static int ata_poll_busy(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY)) return 0;
        ata_io_wait();
    }
    return -1;
}

static int ata_poll_ready(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
        ata_io_wait();
    }
    return -1;
}

static void clean_string(char* out, const uint16_t* src, int word_count) {
    int idx = 0;
    for (int i = 0; i < word_count; i++) {
        uint16_t w = src[i];
        out[idx++] = (char)(w >> 8);
        out[idx++] = (char)(w & 0xFF);
    }
    out[idx] = '\0';
    /* Trim trailing spaces */
    for (int i = idx - 1; i >= 0; i--) {
        if (out[i] == ' ') out[i] = '\0';
        else break;
    }
}

void ata_init(void) {
    memset(&ata_primary_master, 0, sizeof(ata_primary_master));

    /* 1. Select master drive */
    outb(ATA_PRIMARY_DRV_HEAD, 0xA0);
    ata_io_wait();

    /* 2. Clear sector count and LBA registers */
    outb(ATA_PRIMARY_SEC_COUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);

    /* 3. Send IDENTIFY command */
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);
    ata_io_wait();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0 || status == 0xFF) {
        /* No ATA drive on primary channel */
        return;
    }

    if (ata_poll_busy() != 0) {
        return;
    }

    /* Check for ATAPI or SATA devices */
    uint8_t cl = inb(ATA_PRIMARY_LBA_MID);
    uint8_t ch = inb(ATA_PRIMARY_LBA_HI);
    if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96)) {
        /* ATAPI or SATA device; ignore for ATA disk */
        return;
    }

    if (ata_poll_ready() != 0) {
        return;
    }

    /* 4. Read 256 words (512 bytes) of IDENTIFY data */
    uint16_t id_buf[256];
    insw(ATA_PRIMARY_DATA, id_buf, 256);

    clean_string(ata_primary_master.model, &id_buf[27], 20);
    clean_string(ata_primary_master.serial, &id_buf[10], 10);

    /* Total sectors in 28-bit LBA (words 60 and 61) */
    uint32_t sectors = (uint32_t)id_buf[60] | ((uint32_t)id_buf[61] << 16);
    ata_primary_master.sectors = sectors;
    ata_primary_master.size_mb = (sectors * 512) / (1024 * 1024);
    ata_primary_master.present = true;

    printf("ATA HDD: Primary Master detected - %s (%u MB, %u sectors)\n",
           ata_primary_master.model[0] ? ata_primary_master.model : "Generic Hard Disk",
           ata_primary_master.size_mb,
           ata_primary_master.sectors);
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (!ata_primary_master.present || !buffer || count == 0) return -1;
    if (lba + count > ata_primary_master.sectors) return -1;

    if (ata_poll_busy() != 0) return -1;

    /* Select master drive in LBA mode */
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_wait();

    outb(ATA_PRIMARY_SEC_COUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ_PIO);

    uint8_t* ptr = (uint8_t*)buffer;
    for (uint8_t s = 0; s < count; s++) {
        if (ata_poll_ready() != 0) return -1;
        insw(ATA_PRIMARY_DATA, ptr, 256);
        ptr += ATA_SECTOR_SIZE;
    }

    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer) {
    if (!ata_primary_master.present || !buffer || count == 0) return -1;
    if (lba + count > ata_primary_master.sectors) return -1;

    if (ata_poll_busy() != 0) return -1;

    /* Select master drive in LBA mode */
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_wait();

    outb(ATA_PRIMARY_SEC_COUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE_PIO);

    const uint8_t* ptr = (const uint8_t*)buffer;
    for (uint8_t s = 0; s < count; s++) {
        if (ata_poll_ready() != 0) return -1;
        outsw(ATA_PRIMARY_DATA, ptr, 256);
        ptr += ATA_SECTOR_SIZE;
    }

    /* Flush drive cache to ensure data is written to platter/flash */
    outb(ATA_PRIMARY_CMD, ATA_CMD_FLUSH);
    ata_poll_busy();

    return 0;
}
