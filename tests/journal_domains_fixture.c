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
    /* Producer-valid boundaries, not an unsafe cross-product of event facts. */
    const uint8_t tuples[][5] = {
        {1,1,1,0,1}, {1,4,1,1,1}, {2,2,1,0,1}, {3,3,0,0,1}, {3,1,1,0,1},
        {4,4,1,1,1}, {5,255,0,2,0}, {6,0,0,2,0}, {7,4,1,1,1}, {8,0,1,0,1},
        {9,0,1,0,1}, {10,254,1,1,1}, {11,255,1,0,1}, {12,1,1,1,1},
        {13,1,0,0,1}, {14,1,0,0,1}, {14,255,1,1,1}, {15,1,0,2,0}, {15,0,0,0,1}
    };
    for (uint32_t i = 0; i < sizeof(tuples)/sizeof(tuples[0]); i++) {
        make_valid(e); e[2] = 255;
        for (int j = 0; j < 5; j++) e[3+j] = tuples[i][j];
        e[15] = checksum_over(e, 15);
        if (!journal_entry_valid(e)) return 0;
    }
    make_valid(e); e[8] = 0; e[9] = 0; e[10] = 0; e[11] = 0; e[12] = 0; e[13] = 0;
    e[15] = checksum_over(e, 15);
    return journal_entry_valid(e);
}

int test_event_specific_semantics(void)
{
    const uint8_t tuples[][5] = {
        {1,1,0,0,1}, {8,1,1,0,1}, {8,0,1,2,0}, {6,0,1,0,1},
        {7,4,1,0,1}, {1,1,1,1,1}, {5,1,0,0,1}, {15,1,1,0,1}, {12,1,1,2,1}
    };
    uint8_t e[16];
    for (uint32_t i = 0; i < sizeof(tuples)/sizeof(tuples[0]); i++) {
        make_valid(e);
        for (int j = 0; j < 5; j++) e[3+j] = tuples[i][j];
        e[15] = checksum_over(e, 15);
        if (journal_entry_valid(e)) return 0;
    }
    return 1;
}
