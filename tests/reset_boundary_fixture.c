/* Reset-boundary regressions against the actual authority, slot, state and
   lifecycle C (same harness as slot_transition_fixture.c). Only ATA outcomes,
   manifest trust and diagnostics are controlled; no authority function is
   mocked. Proposals are built through the public mutable-record APIs. */
#include "joint_authority_fixture.c"

/* The documented factory table, written here independently of the kernel's
   own builder: identities in fixed order, A active and verified, B "-", no
   rollback, nothing pending, every unused version-array byte zero. */
static const struct { enum module_id id; const char *base; } factory[SLOT_MODULE_COUNT] = {
    { MOD_GHOSTATTACH, "0.3" }, { MOD_RECOVERY_ENGINE, "0.5" },
    { MOD_AI_RULES, "0.4" },    { MOD_RC_SCHEMA, "0.8" },
};

static int bytes_equal(const void *a, const void *b, uint32_t n)
{
    const uint8_t *x=a, *y=b;
    for (uint32_t i=0;i<n;i++) if (x[i]!=y[i]) return 0;
    return 1;
}

static void factory_record(struct module_slot *s, uint32_t i)
{
    kmemset(s,0,sizeof(*s));
    s->module=factory[i].id;
    kstrcpy(s->slot_a_version,factory[i].base,SLOT_VER_MAX);
    kstrcpy(s->slot_b_version,"-",SLOT_VER_MAX);
    s->active_slot=SLOT_A;s->slot_a_verified=1;s->slot_b_verified=0;
    s->rollback_slot=SLOT_NONE;s->pending_verify=0;
}

/* Expected LBA 56 bytes for the factory image, from the documented format:
   "KSLT", version 1, count 4, then module(1) + A[8] + B[8] + 5 flag bytes per
   record, then a two's-complement checksum byte over the 94 used bytes. */
static void factory_sector(uint8_t sec[512])
{
    kmemset(sec,0,512);
    sec[0]='K';sec[1]='S';sec[2]='L';sec[3]='T';sec[4]=1;sec[5]=SLOT_MODULE_COUNT;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        uint8_t *r=sec+6+i*22;
        r[0]=(uint8_t)factory[i].id;
        for (uint32_t c=0;factory[i].base[c];c++) r[1+c]=(uint8_t)factory[i].base[c];
        r[1+SLOT_VER_MAX]='-';
        r[17]=SLOT_A;r[18]=1;r[19]=0;r[20]=SLOT_NONE;r[21]=0;
    }
    uint8_t sum=0;
    for (int i=0;i<94;i++) sum=(uint8_t)(sum+sec[i]);
    sec[94]=(uint8_t)(0u-sum);
}

static struct module_slot *ram_slot(uint32_t i) { return (struct module_slot *)module_slot_get(i); }
static void propose_factory(void) { for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) factory_record(ram_slot(i),i); }

/* Acquire a legitimate reset grant and durably clear package state, exactly
   as reset-packages does before its slot step. history=1 starts from a
   verified update (non-pristine slots); history=0 from a pristine baseline. */
static int grant_with_empty_state(int history)
{
    setup();
    if (history ? !update("PKG-201",1) : pkg_approve("PKG-201")!=1) return 0;
    if (!package_authority_begin_reset() || !package_state_clear()) return 0;
    return package_state_durable_count()==0 && package_authority_reset_active();
}

static int rejected_before_ata_write(void)
{
    static uint8_t before[64][512];
    memcpy(before,disk_image,sizeof(before));
    uint32_t writes=write_count;
    return !module_slots_save() && write_count==writes && bytes_equal(before,disk_image,sizeof(before));
}

/* ---------------------------------------------------------------- F1 */

int test_reset_rejects_duplicate_module_identity(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    factory_record(ram_slot(0),2);           /* a valid factory AI Rules record at index 0 */
    return rejected_before_ata_write();
}

int test_reset_rejects_reordered_module_identities(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    struct module_slot tmp=*ram_slot(0);*ram_slot(0)=*ram_slot(1);*ram_slot(1)=tmp;
    return rejected_before_ata_write();
}

int test_reset_rejects_inactive_version_tail_byte(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    ram_slot(0)->slot_b_version[3]='X';      /* after "-" and its terminator */
    return rejected_before_ata_write();
}

int test_reset_rejects_active_version_tail_byte(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    ram_slot(2)->slot_a_version[SLOT_VER_MAX-1]='Z';
    return rejected_before_ata_write();
}

int test_reset_rejects_changed_flags_and_foreign_identity(void)
{
    /* Controls already enforced before this correction. */
    if (!grant_with_empty_state(1)) return 0;
    propose_factory(); ram_slot(3)->slot_b_verified=1;
    if (!rejected_before_ata_write()) return 0;
    if (!grant_with_empty_state(1)) return 0;
    propose_factory(); ram_slot(1)->module=MOD_KERNEL;
    return rejected_before_ata_write();
}

int test_reset_accepts_exact_factory_image(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    uint8_t expected[512];factory_sector(expected);
    if (!module_slots_save() || !bytes_equal(expected,disk_image[56],512)) return 0;
    cold_boot();
    return package_authority_trusted() && update("PKG-201",1);
}

int test_legitimate_reset_workflow_writes_canonical_image(void)
{
    setup();
    if (!update("PKG-201",1) || !recovery_package_reset()) return 0;
    uint8_t expected[512];factory_sector(expected);
    if (!bytes_equal(expected,disk_image[56],512) || package_authority_reset_active()) return 0;
    cold_boot();
    return package_authority_trusted() && package_state_durable_count()==0 && update("PKG-201",1);
}

int test_factory_image_is_not_an_ordinary_update_exception(void)
{
    setup();
    if (!update("PKG-201",1)) return 0;      /* no reset grant */
    propose_factory();
    return rejected_before_ata_write() && package_authority_trusted();
}

/* ---------------------------------------------------------------- F2 */

int test_rejected_reset_slot_proposal_revokes_grant(void)
{
    if (!grant_with_empty_state(0)) return 0;
    /* A proposal rejected even before the F1 correction isolates F2. */
    propose_factory(); ram_slot(3)->slot_b_verified=1;
    if (module_slots_save() || package_authority_reset_active()) return 0;
    /* A corrected proposal is NOT covered by the rejected attempt's grant. */
    propose_factory();
    if (!rejected_before_ata_write()) return 0;
    /* A new, legitimately acquired grant restores the normal path. */
    if (!package_authority_begin_reset() || !module_slots_save()) return 0;
    uint8_t expected[512];factory_sector(expected);
    return bytes_equal(expected,disk_image[56],512) && package_authority_trusted();
}

int test_rejected_reset_state_proposal_revokes_grant(void)
{
    if (!grant_with_empty_state(0)) return 0;
    uint32_t writes=write_count;
    if (!package_state_ensure("PKG-201") || package_state_save() || write_count!=writes) return 0;
    if (package_authority_reset_active()) return 0;
    if (package_state_clear()) return 0;     /* retry needs a new grant */
    if (!package_authority_begin_reset() || !package_state_clear() || !module_slots_clear()) return 0;
    return package_authority_trusted();
}

int test_out_of_order_reset_slot_write_revokes_grant(void)
{
    /* Slots before durable package state is empty: refused and revoked. */
    setup();
    if (!update("PKG-201",1) || !package_authority_begin_reset()) return 0;
    propose_factory();
    return rejected_before_ata_write() && !package_authority_reset_active();
}

int test_reset_io_failures_revoke_grant_and_hold(void)
{
    /* Controls: I/O failures already latched uncertainty and revoked. */
    if (!grant_with_empty_state(0)) return 0;
    write_failure=56; propose_factory();
    if (module_slots_save() || package_authority_reset_active() || package_authority_trusted()) return 0;
    setup();
    if (pkg_approve("PKG-201")!=1 || !package_authority_begin_reset()) return 0;
    fail_state_writes=1;
    return !package_state_clear() && !package_authority_reset_active() && !package_authority_trusted();
}

int test_successful_reset_consumes_grant(void)
{
    if (!grant_with_empty_state(1)) return 0;
    propose_factory();
    if (!module_slots_save() || package_authority_reset_active()) return 0;
    uint32_t writes=write_count;
    return !package_state_clear() && !module_slots_clear() && !module_slots_save() &&
        write_count==writes && package_authority_trusted();
}
