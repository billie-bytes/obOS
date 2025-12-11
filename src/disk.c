#include "header/driver/disk.h"
#include "header/cpu/portio.h"

static void ATA_busy_wait() {
    while (in(0x1F7) & ATA_STATUS_BSY);
}

static void ATA_DRQ_wait() {
    while (!(in(0x1F7) & ATA_STATUS_RDY));
}

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    // Conversion: 1 FS Block (1024 B) = 2 ATA Sectors (512 B)
    uint32_t sector_lba   = logical_block_address * 2;
    uint8_t  sector_count = block_count * 2;

    ATA_busy_wait();
    out(0x1F6, 0xE0 | ((sector_lba >> 24) & 0xF));
    out(0x1F2, sector_count);
    out(0x1F3, (uint8_t) sector_lba);
    out(0x1F4, (uint8_t) (sector_lba >> 8));
    out(0x1F5, (uint8_t) (sector_lba >> 16));
    out(0x1F7, 0x20); // Read Sectors command

    uint16_t *target = (uint16_t*) ptr;

    // Loop per 512-byte Sector
    for (uint32_t i = 0; i < sector_count; i++) {
        ATA_busy_wait();
        ATA_DRQ_wait();
        // Read 256 words (512 bytes)
        for (uint32_t j = 0; j < 256; j++)
            target[j] = in16(0x1F0);
        target += 256;
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    // Conversion: 1 FS Block (1024 B) = 2 ATA Sectors (512 B)
    uint32_t sector_lba   = logical_block_address * 2;
    uint8_t  sector_count = block_count * 2;

    ATA_busy_wait();
    out(0x1F6, 0xE0 | ((sector_lba >> 24) & 0xF));
    out(0x1F2, sector_count);
    out(0x1F3, (uint8_t) sector_lba);
    out(0x1F4, (uint8_t) (sector_lba >> 8));
    out(0x1F5, (uint8_t) (sector_lba >> 16));
    out(0x1F7, 0x30); // Write Sectors command

    uint16_t *target = (uint16_t*) ptr;

    // Loop per 512-byte Sector
    for (uint32_t i = 0; i < sector_count; i++) {
        ATA_busy_wait();
        ATA_DRQ_wait();

        for (uint32_t j = 0; j < 256; j++)
            out16(0x1F0, target[j]);
        target += 256;
    }
}