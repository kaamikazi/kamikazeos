/* Actual shared validator, without I/O or independently reimplemented policy. */
#include "../kernel/src/recovery_store.c"

static void make_valid(uint8_t *e)
{
    kmemset(e, 0, 16);
    e[0] = 0xa7; e[1] = 1; e[2] = 240; e[3] = 1;
    e[4] = 1; e[5] = 1; e[7] = 1;
    for (int i = 8; i < 14; i++) e[i] = 'X';
    e[15] = checksum_over(e, 15);
}

int test_documented_journal_domains(void)
{
    static const uint8_t mutations[][2] = {
        {5, 2}, {5, 255}, {6, 3}, {6, 255}, {7, 2}, {7, 255},
        {4, 5}, {4, 254}, {14, 1}, {14, 255}, {8, 10}, {8, 127}, {9, 0}
    };
    uint8_t e[16];
    for (uint32_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
        make_valid(e); e[mutations[i][0]] = mutations[i][1];
        e[15] = checksum_over(e, 15);
        if (journal_entry_valid(e)) return 0;
    }
    return 1;
}

int test_valid_journal_boundaries(void)
{
    uint8_t e[16];
    for (int event = 1; event <= 15; event++)
        for (int result = 0; result <= 1; result++)
            for (int risk = 0; risk <= 2; risk++)
                for (int rev = 0; rev <= 1; rev++) {
                    make_valid(e); e[2] = 255; e[3] = event;
                    e[4] = event >= 9 ? 254 : 0;
                    e[5] = result; e[6] = risk; e[7] = rev;
                    e[15] = checksum_over(e, 15);
                    if (!journal_entry_valid(e)) return 0;
                }
    make_valid(e); e[8] = 0; e[9] = 0; e[10] = 0; e[11] = 0; e[12] = 0; e[13] = 0;
    e[15] = checksum_over(e, 15);
    return journal_entry_valid(e);
}
