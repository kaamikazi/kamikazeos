/* ata.c — ATA PIO LBA28 driver for the primary master disk. See ata.h. */
#include "ata.h"
#include "errors.h"
#include "io.h"

/* The primary ATA channel's I/O ports. */
#define REG_DATA      0x1F0   /* 16-bit data window                       */
#define REG_ERROR     0x1F1
#define REG_SECCOUNT  0x1F2   /* how many sectors this command covers     */
#define REG_LBA_LO    0x1F3   /* LBA bits 0-7                             */
#define REG_LBA_MID   0x1F4   /* LBA bits 8-15                            */
#define REG_LBA_HI    0x1F5   /* LBA bits 16-23                           */
#define REG_DRIVE     0x1F6   /* drive select + LBA bits 24-27            */
#define REG_STATUS    0x1F7   /* read: status                             */
#define REG_COMMAND   0x1F7   /* write: command                           */
#define REG_ALTSTATUS 0x3F6   /* status without side effects              */

/* Status register bits. */
#define ST_ERR  0x01          /* an error occurred — details in REG_ERROR */
#define ST_DRQ  0x08          /* data ready: CPU may use the data port    */
#define ST_BSY  0x80          /* busy: ignore every other bit while set   */

#define ST_DF   0x20

/* Commands. */
#define CMD_READ     0x20
#define CMD_WRITE    0x30
#define CMD_FLUSH    0xE7     /* push the drive's write cache to media    */
#define CMD_IDENTIFY 0xEC

/* Bounded-wait budget. PIO on QEMU answers in microseconds; a stuck or
   absent device exhausts this and we fail cleanly instead of hanging. */
#define WAIT_BUDGET  100000

static int present;
static uint32_t capacity_sectors;
static volatile uint32_t io_depth;

/* ~400ns settle delay after selecting a drive: the spec says give the
   drive four status-register read cycles before trusting the bits. */
static void io_settle(void)
{
    inb(REG_ALTSTATUS);
    inb(REG_ALTSTATUS);
    inb(REG_ALTSTATUS);
    inb(REG_ALTSTATUS);
}

/* Wait until BSY clears. 1 = ok, 0 = timeout. */
static int wait_not_busy(void)
{
    for (int i = 0; i < WAIT_BUDGET; i++)
        if ((inb(REG_STATUS) & ST_BSY) == 0)
            return 1;
    return 0;
}

/*
 * Patient variant for CACHE FLUSH completion. Found the hard way: QEMU
 * implements 0xE7 as a real host-file fsync, and on a loaded Windows
 * host that can take many milliseconds — longer than the fast poll
 * budget. Timing out mid-flush made roughly one journal write in ten
 * "fail" (the data was even on disk; only our wait gave up early).
 * Each io_wait() is ~1µs, so this budget is seconds, not microseconds.
 */
static int wait_not_busy_patient(void)
{
    for (int i = 0; i < WAIT_BUDGET * 20; i++) {
        if ((inb(REG_STATUS) & ST_BSY) == 0)
            return 1;
        io_wait();
    }
    return 0;
}

/* Wait until the drive asks for data (DRQ) — or reports an error. */
static int wait_drq(void)
{
    for (int i = 0; i < WAIT_BUDGET; i++) {
        uint8_t st = inb(REG_STATUS);
        if (st & (ST_ERR | ST_DF))
            return 0;
        if ((st & ST_BSY) == 0 && (st & ST_DRQ))
            return 1;
    }
    return 0;
}

static int completion_ok(void)
{
    if (!wait_not_busy())
        return 0;
    return (inb(REG_STATUS) & (ST_ERR | ST_DF)) == 0;
}

static int completion_ok_patient(void)
{
    if (!wait_not_busy_patient())
        return 0;
    return (inb(REG_STATUS) & (ST_ERR | ST_DF)) == 0;
}

static void identify_disk(void)
{
    present = 0;
    capacity_sectors = 0;

    /* A "floating bus" reads 0xFF: no controller at all. */
    if (inb(REG_STATUS) == 0xFF) {
        error_log(SEV_WARNING, "STORE-WARN-001", "ata",
                  "No ATA controller detected",
                  "recovery persistence falls back to CMOS only");
        return;
    }

    /* IDENTIFY the primary master: select it, zero the address registers,
       send the command, and see whether anything answers. */
    outb(REG_DRIVE, 0xA0);          /* 0xA0 = master, CHS addressing bits */
    io_settle();
    outb(REG_SECCOUNT, 0);
    outb(REG_LBA_LO, 0);
    outb(REG_LBA_MID, 0);
    outb(REG_LBA_HI, 0);
    outb(REG_COMMAND, CMD_IDENTIFY);

    if (inb(REG_STATUS) == 0) {     /* status 0 = no device on the wire */
        error_log(SEV_WARNING, "STORE-WARN-001", "ata",
                  "No ATA disk attached",
                  "recovery persistence falls back to CMOS only");
        return;
    }
    if (!wait_not_busy() || !wait_drq()) {
        error_log(SEV_WARNING, "STORE-WARN-002", "ata",
                  "ATA disk did not answer IDENTIFY",
                  "recovery persistence falls back to CMOS only");
        return;
    }

    uint16_t word49 = 0;
    uint16_t word60 = 0;
    uint16_t word61 = 0;
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(REG_DATA);
        if (i == 49) word49 = word;
        if (i == 60) word60 = word;
        if (i == 61) word61 = word;
    }

    capacity_sectors = (uint32_t)word60 | ((uint32_t)word61 << 16);
    if ((word49 & (1u << 9)) == 0 || capacity_sectors == 0) {
        capacity_sectors = 0;
        error_log(SEV_WARNING, "STORE-WARN-007", "ata",
                  "ATA disk lacks a valid LBA28 capacity",
                  "durable storage disabled rather than guessing bounds");
        return;
    }

    present = 1;
    error_log(SEV_INFO, "STORE-000", "ata",
              "ATA disk detected (PIO mode)", "durable recovery store enabled");
}

void ata_init(void)
{
    io_depth++;identify_disk();io_depth--;
}
int ata_io_in_progress(void) { return io_depth!=0; }

int ata_available(void)
{
    return present;
}

uint32_t ata_capacity_sectors(void)
{
    return present ? capacity_sectors : 0;
}

static int request_valid(uint32_t lba, const void *buffer)
{
    return present && buffer != 0 && lba < capacity_sectors &&
           lba <= 0x0FFFFFFFu;
}

/* Program the address registers for a one-sector LBA28 command. */
static void select_lba(uint32_t lba)
{
    outb(REG_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));  /* 0xE0=LBA+master */
    io_settle();
    outb(REG_SECCOUNT, 1);
    outb(REG_LBA_LO,  (uint8_t)(lba));
    outb(REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(REG_LBA_HI,  (uint8_t)(lba >> 16));
}

static int read_sector_once(uint32_t lba, uint8_t *buffer)
{
    if (!wait_not_busy())
        return 0;

    select_lba(lba);
    outb(REG_COMMAND, CMD_READ);
    if (!wait_drq())
        return 0;

    /* The drive hands us 256 little-endian words through the data port. */
    for (int i = 0; i < 256; i++) {
        uint16_t w = inw(REG_DATA);
        buffer[i * 2]     = (uint8_t)(w & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(w >> 8);
    }
    return completion_ok();
}

static int write_sector_once(uint32_t lba, const uint8_t *buffer)
{
    if (!wait_not_busy())
        return 0;

    select_lba(lba);
    outb(REG_COMMAND, CMD_WRITE);
    if (!wait_drq())
        return 0;

    for (int i = 0; i < 256; i++) {
        uint16_t w = (uint16_t)(buffer[i * 2] | (buffer[i * 2 + 1] << 8));
        outw(REG_DATA, w);
    }

    /* Make it DURABLE: flush the drive's write cache, then wait. Recovery
       state that only reached a volatile cache would defeat the point.
       Flush = host fsync in QEMU: wait PATIENTLY (see above). */
    if (!completion_ok_patient())
        return 0;
    outb(REG_COMMAND, CMD_FLUSH);
    return completion_ok_patient();
}

/*
 * Public read/write: one RETRY on failure. Found the hard way: polled PIO
 * against QEMU's IDE intermittently misses a status transition (a write
 * every few dozen ops saw a stuck/odd status and aborted). One settle +
 * retry clears every observed case; persistent failures still return 0
 * and the caller logs them loudly.
 */
int ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    if (!request_valid(lba, buffer))
        return 0;
    io_depth++;
    int ok=read_sector_once(lba,buffer);
    if(!ok) {io_settle();ok=read_sector_once(lba,buffer);}
    io_depth--;return ok;
}

int ata_write_sector(uint32_t lba, const uint8_t *buffer)
{
    if (!request_valid(lba, buffer))
        return 0;
    io_depth++;
    int ok=write_sector_once(lba,buffer);
    if(!ok) {io_settle();ok=write_sector_once(lba,buffer);}
    io_depth--;return ok;
}
