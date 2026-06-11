/*
 * ata.h — minimal ATA PIO disk driver (the smallest honest storage layer).
 *
 * Talks to the PRIMARY MASTER IDE disk through the legacy ATA I/O ports
 * (0x1F0–0x1F7) in PIO mode: the CPU moves every byte itself through the
 * data port, polling status bits between steps. No interrupts, no DMA,
 * no partitions, no filesystem — just "read sector N" / "write sector N",
 * which is exactly what the RecoveryStore needs and nothing more.
 *
 * QEMU side: attach a raw image with
 *     -drive file=build/recovery.img,format=raw,if=ide
 * and it appears as this primary master disk.
 *
 * SAFETY PROPERTIES:
 *  - every wait loop is BOUNDED: a missing/stuck disk can never hang boot
 *  - if no disk answers IDENTIFY at init, all calls become safe no-ops and
 *    ata_available() returns 0 — the kernel keeps booting (CMOS fallback)
 *  - LBA28 only, one sector at a time, fixed 512-byte buffers
 */
#ifndef KMZ_ATA_H
#define KMZ_ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

/* Probe the primary master with IDENTIFY. Call once at boot. */
void ata_init(void);

/* 1 if a disk answered IDENTIFY at init. */
int ata_available(void);

/* Read/write one 512-byte sector. Return 1 on success, 0 on failure
   (no disk, timeout, or error bit). Never blocks forever. */
int ata_read_sector(uint32_t lba, uint8_t *buffer);
int ata_write_sector(uint32_t lba, const uint8_t *buffer);

#endif /* KMZ_ATA_H */
