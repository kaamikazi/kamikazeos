/* Actual persistence/authority/lifecycle code and the established ATA fixture.
   Stages are acknowledged by the real state writer, not authority mocks. */
#include "joint_authority_fixture.c"

static int bytes_equal(const void *a, const void *b, uint32_t n)
{
    const uint8_t *x=a, *y=b;
    for (uint32_t i=0;i<n;i++) if (x[i]!=y[i]) return 0;
    return 1;
}

static int stage_ghost(uint8_t phase)
{
    setup();
    if (phase==PST_TX_APPLY_STAGED) {
        if (pkg_approve("PKG-201")!=1) return 0;
    } else if (!update("PKG-201",0)) return 0;
    return stage_transaction(&manifests[0],phase,SLOT_A,SLOT_B,0,80);
}

/* Independent candidate construction from the documented mutation contract. */
static void propose_forward(struct module_slot *s, uint8_t phase, const char *to)
{
    if (phase==PST_TX_APPLY_STAGED) {
        kstrcpy(s->slot_b_version,to,SLOT_VER_MAX);
        s->slot_b_verified=0;s->active_slot=SLOT_B;
        s->rollback_slot=SLOT_A;s->pending_verify=1;
    } else if (phase==PST_TX_VERIFY_STAGED) {
        s->slot_b_verified=1;s->pending_verify=0;
    } else {
        s->active_slot=SLOT_A;s->rollback_slot=SLOT_NONE;s->pending_verify=0;
    }
}

static int reject_without_persistence(void)
{
    static uint8_t before[64][512];
    struct module_slot old[SLOT_MODULE_COUNT];
    memcpy(before,disk_image,sizeof(before));
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) old[i]=*module_slot_durable_get(i);
    uint32_t writes=write_count;
    int accepted=module_slots_save();
    if (accepted || write_count!=writes || !bytes_equal(before,disk_image,sizeof(before))) return 0;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++)
        if (!bytes_equal(&old[i],module_slot_durable_get(i),sizeof(old[i]))) return 0;
    return package_authority_trusted(); /* rejecting a proposal must not poison durable truth */
}

static int reverse_is_rejected(uint8_t phase)
{
    if (!stage_ghost(phase)) return 0;
    struct module_slot *s=module_slot_for(MOD_GHOSTATTACH), before=*s;
    propose_forward(s,phase,"0.4");
    if (!module_slots_save()) return 0;
    *s=before;
    return reject_without_persistence();
}

int test_apply_stage_cannot_reverse_acknowledged_switch(void)
{ return reverse_is_rejected(PST_TX_APPLY_STAGED); }
int test_verify_stage_cannot_reverse_acknowledged_verification(void)
{ return reverse_is_rejected(PST_TX_VERIFY_STAGED); }
int test_rollback_stage_cannot_reverse_acknowledged_rollback(void)
{ return reverse_is_rejected(PST_TX_ROLLBACK_STAGED); }

static int foreign_proposal(int also_forward)
{
    setup();
    if (!update("PKG-301",1) || pkg_rollback("PKG-301")!=1 ||
        pkg_approve("PKG-301")!=1 || pkg_approve("PKG-201")!=1 ||
        !stage_transaction(&manifests[0],PST_TX_APPLY_STAGED,SLOT_A,SLOT_B,0,80)) return 0;
    /* AI has a coherent approved rollback baseline, but NO staged intent.
       Erasing its inactive history to pristine is coherent, not authorized. */
    struct module_slot *ai=module_slot_for(MOD_AI_RULES);
    kstrcpy(ai->slot_b_version,"-",SLOT_VER_MAX);ai->slot_b_verified=0;
    if (also_forward) propose_forward(module_slot_for(MOD_GHOSTATTACH),PST_TX_APPLY_STAGED,"0.4");
    return reject_without_persistence();
}

int test_one_modules_stage_cannot_authorize_another_modules_change(void)
{ return foreign_proposal(0); }
int test_mixed_authorized_and_foreign_changes_are_rejected_atomically(void)
{ return foreign_proposal(1); }

int test_rollback_cannot_change_inactive_verification_flag(void)
{
    if (!stage_ghost(PST_TX_ROLLBACK_STAGED)) return 0;
    struct module_slot *s=module_slot_for(MOD_GHOSTATTACH);
    propose_forward(s,PST_TX_ROLLBACK_STAGED,"0.4");
    s->slot_b_verified=1; /* rollback does not verify the failed/inactive update */
    return reject_without_persistence();
}

int test_forward_phases_preserve_unaffected_records_and_fields(void)
{
    for (uint8_t phase=PST_TX_APPLY_STAGED;phase<=PST_TX_ROLLBACK_STAGED;phase++) {
        if (!stage_ghost(phase)) return 0;
        uint8_t before[512];memcpy(before,disk_image[56],512);
        struct module_slot *s=module_slot_for(MOD_GHOSTATTACH), expected=*s;
        propose_forward(&expected,phase,"0.4");*s=expected;
        if (!module_slots_save() || !bytes_equal(&expected,module_slot_durable_get(0),sizeof(expected))) return 0;
        /* Other records and reserved bytes: only GhostAttach and checksum change. */
        for (int i=28;i<512;i++)
            if (i!=94 && before[i]!=disk_image[56][i]) return 0;
        uint32_t writes=write_count;
        if (!module_slots_save() || write_count!=writes+1) return 0; /* safe staged retry */
    }
    return 1;
}

int test_interrupted_phases_reconcile_both_observed_sides_without_slot_rewrite(void)
{
    for (uint8_t phase=PST_TX_APPLY_STAGED;phase<=PST_TX_ROLLBACK_STAGED;phase++) {
        for (int after=0;after<=1;after++) {
            if (!stage_ghost(phase)) return 0;
            if (after) {
                propose_forward(module_slot_for(MOD_GHOSTATTACH),phase,"0.4");
                if (!module_slots_save()) return 0;
            }
            uint8_t slots_before[512];memcpy(slots_before,disk_image[56],512);
            cold_boot(); /* existing fixture's explicit new-BSS context */
            const struct pkg_pstate *s=durable_find("PKG-201");
            int expected=after ? (phase==PST_TX_APPLY_STAGED ? PKGST_APPLIED :
                phase==PST_TX_VERIFY_STAGED ? PKGST_VERIFIED : PKGST_ROLLED_BACK) :
                (phase==PST_TX_APPLY_STAGED ? PKGST_APPROVED : PKGST_APPLIED);
            if (!package_authority_trusted() || s->transaction_phase!=PST_TX_COMMITTED ||
                s->status!=expected || !bytes_equal(slots_before,disk_image[56],512)) return 0;
        }
    }
    return 1;
}

int test_each_of_two_changed_modules_requires_its_own_stage(void)
{
    if (!stage_ghost(PST_TX_APPLY_STAGED) || pkg_approve("PKG-301")!=1 ||
        !stage_transaction(&manifests[3],PST_TX_APPLY_STAGED,SLOT_A,SLOT_B,0,80)) return 0;
    propose_forward(module_slot_for(MOD_GHOSTATTACH),PST_TX_APPLY_STAGED,"0.4");
    propose_forward(module_slot_for(MOD_AI_RULES),PST_TX_APPLY_STAGED,"0.5");
    if (!module_slots_save()) return 0;
    cold_boot();
    return package_authority_trusted() && durable_find("PKG-201")->applied &&
        durable_find("PKG-301")->applied;
}

int test_verify_stage_cannot_authorize_rollback(void)
{
    if (!stage_ghost(PST_TX_VERIFY_STAGED)) return 0;
    propose_forward(module_slot_for(MOD_GHOSTATTACH),PST_TX_ROLLBACK_STAGED,"0.4");
    return reject_without_persistence();
}
