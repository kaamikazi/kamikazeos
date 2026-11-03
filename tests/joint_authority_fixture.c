/* Actual package engine + authority with actual persistence/slot modules linked.
   Only ATA outcomes, manifest trust and diagnostic/health APIs are mocked. */
#include "../kernel/src/package_authority.c"
#include "../kernel/src/recovery_package.c"

static uint8_t disk_image[64][512];
static struct pkg_manifest manifests[4];
static int read_failure=-1, write_failure=-1, fail_state_writes, arm_publish_failure;
static uint32_t write_count;

int ata_available(void) { return 1; }
uint32_t ata_capacity_sectors(void) { return 64; }
int ata_read_sector(uint32_t lba,uint8_t *out)
{
    if(lba>=64 || (int)lba==read_failure) return 0;
    memcpy(out,disk_image[lba],512); return 1;
}
int ata_write_sector(uint32_t lba,const uint8_t *in)
{
    if(lba>=64 || (int)lba==write_failure || (fail_state_writes && (lba==48 || lba==49))) return 0;
    write_count++; memcpy(disk_image[lba],in,512);
    if(lba==56 && arm_publish_failure) { arm_publish_failure=0; fail_state_writes=1; }
    return 1;
}
void package_manifest_init(void) {}
uint32_t manifest_count(void) { return 4; }
struct pkg_manifest *manifest_get(uint32_t i) { return i<4 ? &manifests[i] : 0; }
struct pkg_manifest *manifest_find(const char *id)
{
    for(int i=0;i<4;i++) if(!kstrcmp(id,manifests[i].package_id)) return &manifests[i];
    return 0;
}
uint32_t manifest_identity(const struct pkg_manifest *m)
{
    uint32_t h=2166136261u;
    const char *fields[]={m->package_id,m->from_version,m->to_version};
    for(int f=0;f<3;f++) for(const char*p=fields[f];*p;p++) h=(h^(uint8_t)*p)*16777619u;
    return h ^ (uint32_t)m->target_module;
}
int manifest_verify(const struct pkg_manifest *m,const char **why)
{ *why="controlled trusted fixture"; return m!=0; }
void error_log(enum err_severity s,const char*c,const char*o,const char*m,const char*h)
{ (void)s;(void)c;(void)o;(void)m;(void)h; }
void error_log_class(enum err_severity s,enum health_class k,const char*c,const char*o,const char*m,const char*h)
{ (void)s;(void)k;(void)c;(void)o;(void)m;(void)h; }
uint32_t error_health_count(enum health_class k,enum err_severity s) { (void)k;(void)s;return 0; }
void boot_snapshot_health_now(struct health_breakdown *h) { kmemset(h,0,sizeof(*h));h->score=80; }
void recovery_journal_append(enum journal_event e,uint8_t id,uint8_t r,uint8_t risk,uint8_t rev,const char*c)
{ (void)e;(void)id;(void)r;(void)risk;(void)rev;(void)c; }

static void cold_boot(void)
{
    /* Simulate a NEW kernel BSS, never an in-session registry reload. No
       production API exposes this reset of the sticky authority latch. */
    authority_latched=0; reset_grant=0;
    recovery_package_init();
}
static void setup(void)
{
    kmemset(disk_image,0,sizeof(disk_image));kmemset(manifests,0,sizeof(manifests));
    read_failure=write_failure=-1;fail_state_writes=arm_publish_failure=0;write_count=0;
    const char *ids[]={"PKG-201","PKG-202","PKG-203","PKG-301"};
    for(int i=0;i<4;i++) {
        kstrcpy(manifests[i].package_id,ids[i],PM_ID_MAX);
        kstrcpy(manifests[i].name,ids[i],sizeof(manifests[i].name));
        kstrcpy(manifests[i].from_version,i==1?"0.4":i==3?"0.4":"0.3",PM_VER_MAX);
        kstrcpy(manifests[i].to_version,i==1?"0.3":i==3?"0.5":"0.4",PM_VER_MAX);
        manifests[i].target_module=i==3?MOD_AI_RULES:MOD_GHOSTATTACH;
        manifests[i].risk_level=RISK_LOW;manifests[i].reversible=1;
    }
    cold_boot();
}
static int update(const char *id,int verify)
{ return pkg_approve(id)==1 && pkg_apply(id)==1 && (!verify || pkg_verify(id)==1); }
static const struct pkg_pstate *durable_find(const char *id)
{
    for(uint32_t i=0;i<package_state_durable_count();i++)
        if(!kstrcmp(package_state_durable_get(i)->package_id,id)) return package_state_durable_get(i);
    return 0;
}
static int prefix(void)
{ setup(); return update("PKG-201",1) && update("PKG-202",1) && pkg_approve("PKG-203")==1; }

int test_owner_commit_retires_only_same_module(void)
{
    setup();
    if(!update("PKG-201",1) || !update("PKG-301",1) || !update("PKG-202",1) || !update("PKG-203",0)) return 0;
    return durable_find("PKG-202")->slot_before_apply==SLOT_NONE &&
           durable_find("PKG-202")->verified && durable_find("PKG-203")->applied &&
           durable_find("PKG-301")->slot_before_apply==SLOT_A && package_authority_trusted();
}
int test_failed_commit_and_reconciliation_preserve_staged_intent(void)
{
    if(!prefix()) return 0;
    arm_publish_failure=1;
    if(pkg_apply("PKG-203")!=-4 || package_authority_trusted() ||
       durable_find("PKG-203")->transaction_phase!=PST_TX_APPLY_STAGED ||
       durable_find("PKG-202")->slot_before_apply!=SLOT_B) return 0;
    cold_boot(); /* final publication still fails; durable stage survives */
    if(package_authority_trusted() || durable_find("PKG-203")->transaction_phase!=PST_TX_APPLY_STAGED) return 0;
    fail_state_writes=0; cold_boot();
    return package_authority_trusted() && durable_find("PKG-203")->applied &&
           durable_find("PKG-202")->slot_before_apply==SLOT_NONE;
}
int test_real_slot_failure_retains_stage_until_cold_read(void)
{
    if(!prefix()) return 0;
    write_failure=56;
    if(pkg_apply("PKG-203")!=-4 || package_authority_trusted() ||
       durable_find("PKG-203")->transaction_phase!=PST_TX_APPLY_STAGED) return 0;
    write_failure=-1;cold_boot();
    return package_authority_trusted() && durable_find("PKG-203")->approved && pkg_rollback("PKG-202")==1;
}
int test_slot_read_failure_cannot_rewrite_staged_intent(void)
{
    if(!prefix()) return 0;
    arm_publish_failure=1;
    if(pkg_apply("PKG-203")!=-4) return 0;
    fail_state_writes=0;read_failure=56;uint32_t before=write_count;cold_boot();
    if(package_authority_trusted() || write_count!=before ||
       durable_find("PKG-203")->transaction_phase!=PST_TX_APPLY_STAGED) return 0;
    read_failure=-1;cold_boot();
    return package_authority_trusted() && durable_find("PKG-203")->applied && pkg_rollback("PKG-203")==1;
}
int test_reload_cannot_clear_latched_uncertainty_with_stale_copies(void)
{
    setup();if(!update("PKG-201",1)) return 0;
    uint8_t old[512];memcpy(old,disk_image[48],512);
    if(!update("PKG-202",1) || !update("PKG-203",1)) return 0;
    read_failure=56;recovery_package_init();
    if(package_authority_trusted()) return 0;
    read_failure=-1;memcpy(disk_image[48],old,512);memcpy(disk_image[49],old,512);
    uint32_t before=write_count;recovery_package_init(); /* NOT a cold boot */
    return !package_authority_trusted() && pkg_rollback("PKG-201")==-4 && write_count==before;
}
int test_candidate_approval_cannot_destroy_current_owner(void)
{
    setup();if(!update("PKG-201",1)) return 0;
    uint32_t before=write_count;
    if(commit_state(&manifests[0],PKGST_APPROVED,1) || write_count!=before) return 0;
    return durable_find("PKG-201")->verified && package_authority_trusted() && pkg_rollback("PKG-201")==1;
}
int test_scoped_reset_and_partial_reset_hold(void)
{
    setup();if(!update("PKG-201",1)) return 0;
    if(package_state_clear() || module_slots_clear()) return 0;
    write_failure=56;
    if(recovery_package_reset() || package_authority_trusted()) return 0;
    write_failure=-1;cold_boot();
    if(package_authority_trusted() || recovery_package_reset()) return 0;
    /* Separate pristine scenario (new image/new BSS) tests healthy reset. */
    setup();if(!update("PKG-201",1) || !recovery_package_reset()) return 0;
    cold_boot();
    return package_authority_trusted() && update("PKG-201",1);
}

int test_coherent_owner_transfer_still_requires_durable_intent(void)
{
    setup();if(!update("PKG-201",1) || !update("PKG-202",1) || !update("PKG-203",1)) return 0;
    struct pkg_pstate *a=package_state_find("PKG-201"), *b=package_state_find("PKG-203");
    struct pkg_pstate old_a=*a,old_b=*b;
    a->slot_before_apply=SLOT_A; b->slot_before_apply=SLOT_NONE;
    uint32_t before=write_count;
    int accepted=package_state_save();
    *a=old_a;*b=old_b;
    return !accepted && write_count==before && durable_find("PKG-203")->verified &&
           pkg_rollback("PKG-203")==1;
}

int test_slot_flags_are_not_normalized_into_authority(void)
{
    setup();if(!update("PKG-201",1)) return 0;
    disk_image[56][25]=2; /* old decoder treated every nonzero as verified */
    uint8_t sum=0;for(int i=0;i<94;i++) sum+=disk_image[56][i];
    disk_image[56][94]=0u-sum;
    uint32_t before=write_count;cold_boot();
    return !package_authority_trusted() && pkg_rollback("PKG-201")==-4 && write_count==before;
}

int test_case_aliases_cannot_split_package_authority(void)
{
    setup();if(!update("PKG-201",1) || !update("PKG-202",1) || !update("PKG-203",1)) return 0;
    /* package_state_find is case-insensitive. A retired alias preceding the
       current record must not make the writer and authority select different rows. */
    kstrcpy((char*)disk_image[48]+6,"pkg-203",PM_ID_MAX);
    uint32_t used=6+disk_image[48][5]*37;
    uint8_t sum=0;for(uint32_t i=0;i<used;i++) sum+=disk_image[48][i];
    disk_image[48][used]=0u-sum;memcpy(disk_image[49],disk_image[48],512);
    uint32_t before=write_count;cold_boot();
    return !package_authority_trusted() && write_count==before;
}
