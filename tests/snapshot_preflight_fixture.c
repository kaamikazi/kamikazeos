/* Actual preflight; mock only disk reads and the separate chronology proof. */
#include "../kernel/src/boot_snapshot.c"
static uint8_t media[32][512];
static int proof;
int ata_read_sector(uint32_t lba, uint8_t *out)
{ if (lba >= 32) return 0; memcpy(out, media[lba], 512); return 1; }
int recovery_journal_boot_progression_verified(uint8_t prior, uint8_t next)
{ return proof && next > prior; }
static void history(void)
{
    kmemset(media, 0, sizeof(media)); proof = 1;
    for (int boot = 1; boot <= 16; boot++) {
        uint8_t *s = media[16 + boot % 16];
        memcpy(s, "KSNP", 4); s[4] = 1; s[5] = boot;
        uint8_t sum = 0; for (int i = 0; i < 17; i++) sum += s[i];
        s[17] = 0u - sum;
    }
}
int test_swapped_and_duplicate_history_is_held(void)
{
    history(); uint8_t tmp[512]; memcpy(tmp, media[16], 512);
    memcpy(media[16], media[17], 512); memcpy(media[17], tmp, 512);
    if (snapshot_preflight(17)) return 0;
    history(); memcpy(media[20], media[16], 512);
    return !snapshot_preflight(17);
}
int test_ring_age_and_boot_boundaries(void)
{
    history();
    if (!snapshot_preflight(17) || !snapshot_preflight(18) || !snapshot_preflight(32)) return 0;
    if (snapshot_preflight(16) || snapshot_preflight(0)) return 0;
    proof = 0;
    return !snapshot_preflight(34);
}
