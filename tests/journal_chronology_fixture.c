#include "../kernel/src/recovery_store.c"
static uint8_t media[64][512];
static uint32_t reads;
static int fail_writes;
static struct recovery_memory memory;
void ata_init(void) {}
int ata_available(void) { return 1; }
uint32_t ata_capacity_sectors(void) { return 64; }
int ata_read_sector(uint32_t lba, uint8_t *out)
{ if (lba >= 64) return 0; reads++; memcpy(out, media[lba],512); return 1; }
int ata_write_sector(uint32_t lba, const uint8_t *in)
{ if (lba >= 64 || fail_writes) return 0; memcpy(media[lba],in,512); return 1; }
const struct recovery_memory *recovery_memory_get(void) { return &memory; }
void error_log(enum err_severity s,const char*c,const char*o,const char*m,const char*h)
{ (void)s;(void)c;(void)o;(void)m;(void)h; }
static void complete(int index, uint8_t boot)
{
    uint8_t *e = media[2 + index/32] + (index%32)*16;
    kmemset(e,0,16); e[0]=0xa7;e[1]=1;e[2]=boot;e[3]=8;e[5]=1;e[7]=1;
    e[15]=checksum_over(e,15);
}
static void setup(int count)
{
    kmemset(media,0,sizeof(media)); fail_writes=0;
    for(int i=0;i<count;i++) complete(i,i+1);
    recovery_store_init(); reads=0;
}
int test_chronology_crosses_physical_sector_boundaries(void)
{
    setup(65);
    return recovery_journal_boot_progression_verified(16,66) && reads==3;
}
int test_chronology_is_rechecked_not_sorted(void)
{
    setup(32);
    for(int i=0;i<32;i++) complete(i,32-i);
    if(recovery_journal_boot_progression_verified(16,33)) return 0;
    setup(32); complete(24,24);
    if(recovery_journal_boot_progression_verified(16,33)) return 0;
    setup(32); kmemset(media[2]+23*16,0,16);
    return !recovery_journal_boot_progression_verified(16,33) &&
           !recovery_journal_boot_progression_verified(255,0);
}
int test_failed_clear_never_unlocks_persistence(void)
{
    setup(2); fail_writes=1; recovery_store_clear();
    if(!state_locked || !journal_locked || valid) return 0;
    fail_writes=0; recovery_store_clear();
    return !state_locked && !journal_locked && journal_scan_valid && journal_order_valid && jcount==0;
}
