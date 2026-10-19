/* One consistency/permission boundary for package and module-slot persistence.
   All state here is RAM-only. See docs/TRUST_INVARIANTS.md. */
#include "package_authority.h"
#include "module_slots.h"
#include "recovery_package.h"
#include "errors.h"
#include "string.h"

static int authority_latched;
static const char *latched_reason = "not_initialized";
static int reset_grant;

void package_authority_note_uncertain(const char *reason)
{
    if (!authority_latched) latched_reason = reason;
    authority_latched = 1;
    reset_grant = 0;
}

static uint32_t view_count(int ram)
{ return ram ? package_state_count() : package_state_durable_count(); }
static const struct pkg_pstate *view_get(int ram, uint32_t i)
{ return ram ? package_state_get(i) : package_state_durable_get(i); }
static const struct module_slot *slot_view(int ram, uint32_t i)
{ return ram ? module_slot_get(i) : module_slot_durable_get(i); }
static const char *version_at(const struct module_slot *s, uint8_t index)
{ return index == SLOT_A ? s->slot_a_version : s->slot_b_version; }
static int verified_at(const struct module_slot *s, uint8_t index)
{ return index == SLOT_A ? s->slot_a_verified : s->slot_b_verified; }
static int live(const struct pkg_pstate *s)
{ return s->status == PKGST_APPLIED || s->status == PKGST_VERIFIED || s->status == PKGST_FAILED; }
static int text_terminated(const char *s, uint32_t n)
{ for (uint32_t i=0;i<n;i++) if (!s[i]) return 1; return 0; }
static int record_flags_valid(const struct pkg_pstate *s)
{
    return s && text_terminated(s->package_id, PM_ID_MAX) && s->package_id[0] &&
        text_terminated(s->from_version, PM_VER_MAX) && text_terminated(s->to_version, PM_VER_MAX) &&
        s->status <= PKGST_BLOCKED && s->approved == (s->status == PKGST_APPROVED) &&
        s->applied == (s->status == PKGST_APPLIED) && s->verified == (s->status == PKGST_VERIFIED) &&
        s->failed == (s->status == PKGST_FAILED) && s->rolled_back == (s->status == PKGST_ROLLED_BACK) &&
        s->transaction_phase <= PST_TX_ROLLBACK_STAGED && s->status_before_transaction <= PKGST_BLOCKED &&
        (s->slot_before_apply <= SLOT_B || s->slot_before_apply == SLOT_NONE) &&
        (s->slot_after_apply <= SLOT_B || s->slot_after_apply == SLOT_NONE) &&
        (!s->extension_valid || s->transaction_phase >= PST_TX_COMMITTED);
}
static int bound_frame(const struct pkg_pstate *s)
{
    if (!s->extension_valid || s->slot_before_apply > SLOT_B || s->slot_after_apply > SLOT_B ||
        s->slot_before_apply == s->slot_after_apply || !s->from_version[0] || !s->to_version[0] ||
        kstrcmp(s->from_version, s->to_version) == 0 || s->health_at_apply > 100)
        return 0;
    const struct pkg_manifest *m = manifest_find(s->package_id);
    const char *why;
    return m && manifest_verify(m, &why) && s->manifest_identity == manifest_identity(m) &&
        s->target_module == m->target_module && kstrcmp(s->from_version, m->from_version) == 0 &&
        kstrcmp(s->to_version, m->to_version) == 0;
}
static int current_shape(const struct pkg_pstate *s, const struct module_slot *v, int verified)
{
    return bound_frame(s) && v->active_slot == s->slot_after_apply &&
        v->rollback_slot == s->slot_before_apply &&
        kstrcmp(version_at(v,s->slot_after_apply),s->to_version) == 0 &&
        kstrcmp(version_at(v,s->slot_before_apply),s->from_version) == 0 &&
        verified_at(v,s->slot_before_apply) == 1 &&
        verified_at(v,s->slot_after_apply) == verified && v->pending_verify == !verified;
}
static int rollback_shape(const struct pkg_pstate *s, const struct module_slot *v)
{
    return bound_frame(s) && v->active_slot == s->slot_before_apply &&
        v->rollback_slot == SLOT_NONE && !v->pending_verify && verified_at(v,v->active_slot) &&
        kstrcmp(version_at(v,s->slot_before_apply),s->from_version) == 0 &&
        kstrcmp(version_at(v,s->slot_after_apply),s->to_version) == 0;
}

/* Returns the unique observed side of a staged transition, never an assumed
   abort/failure. -1 preserves the intent and holds authority. */
static int staged_result(const struct pkg_pstate *s, const struct module_slot *v)
{
    if (!bound_frame(s)) return -1;
    if (s->transaction_phase == PST_TX_APPLY_STAGED) {
        if (s->status != PKGST_APPROVED || s->status_before_transaction != PKGST_APPROVED) return -1;
        if (current_shape(s,v,0)) return PKGST_APPLIED;
        if (v->active_slot == s->slot_before_apply && !v->pending_verify && verified_at(v,v->active_slot) &&
            kstrcmp(module_slot_active_version(v),s->from_version) == 0)
            return PKGST_APPROVED;
    } else if (s->transaction_phase == PST_TX_VERIFY_STAGED) {
        if (s->status != PKGST_APPLIED || s->status_before_transaction != PKGST_APPLIED) return -1;
        if (current_shape(s,v,1)) return PKGST_VERIFIED;
        if (current_shape(s,v,0)) return PKGST_APPLIED;
    } else if (s->transaction_phase == PST_TX_ROLLBACK_STAGED) {
        if (!live(s) || s->status != s->status_before_transaction) return -1;
        if (rollback_shape(s,v)) return PKGST_ROLLED_BACK;
        if (current_shape(s,v,s->status == PKGST_VERIFIED)) return s->status_before_transaction;
    }
    return -1;
}

static const char *module_consistency(int ram, const struct module_slot *v, int empty,
                                      int *has_stage)
{
    const struct pkg_pstate *stage=0, *owner=0, *anchor=0;
    int stages=0, owners=0, anchors=0, history=0, approved_baseline=0;
    for (uint32_t i=0;i<view_count(ram);i++) {
        const struct pkg_pstate *s=view_get(ram,i);
        if (s->target_module != v->module || !s->extension_valid) continue;
        if (s->transaction_phase > PST_TX_COMMITTED) { stage=s; stages++; }
        else if (s->slot_before_apply != SLOT_NONE) {
            if (live(s)) { owner=s; owners++; }
            if (s->status == PKGST_ROLLED_BACK) { anchor=s; anchors++; }
            if (s->status == PKGST_APPROVED && rollback_shape(s,v)) approved_baseline=1;
        }
        if (live(s) || s->status == PKGST_ROLLED_BACK || s->transaction_phase > PST_TX_COMMITTED)
            history=1;
    }
    if (empty && (history || approved_baseline)) return "missing_slot_history";
    if (stages > 1 || owners + anchors > 1) return "multiple_claimants";
    if (stages) {
        *has_stage=1;
        int result=staged_result(stage,v);
        if (empty || result < 0) return "staged_slot_contradiction";
        if (stage->transaction_phase == PST_TX_APPLY_STAGED) {
            if (result == PKGST_APPROVED) {
                if (owner && !current_shape(owner,v,owner->status == PKGST_VERIFIED)) return "old_owner_contradiction";
                if (anchor && !rollback_shape(anchor,v)) return "old_baseline_contradiction";
                if (!owner && !anchor && !module_slot_is_pristine(v) && !rollback_shape(stage,v))
                    return "unproven_apply_baseline";
            } else {
                /* Older current authority is now the known-good rollback
                   baseline; its former inactive version was intentionally consumed. */
                if (owner && (!bound_frame(owner) || owner->slot_after_apply != stage->slot_before_apply ||
                              kstrcmp(owner->to_version,stage->from_version))) return "old_owner_contradiction";
                if (anchor && (!bound_frame(anchor) || anchor->slot_before_apply != stage->slot_before_apply ||
                               kstrcmp(anchor->from_version,stage->from_version))) return "old_baseline_contradiction";
            }
        } else if (owners || anchors) {
            return "competing_staged_owner";
        }
        return 0;
    }
    if (owner) {
        if (empty || !current_shape(owner,v,owner->status == PKGST_VERIFIED)) return "current_slot_contradiction";
        return 0;
    }
    if (anchor) return rollback_shape(anchor,v) ? 0 : "rollback_baseline_contradiction";
    if (module_slot_is_pristine(v) && !history) return 0;
    /* An approval after a completed rollback preserves a no-rollback baseline.
       It grants no old rollback rights; an explicit new apply needs staging. */
    if (!empty && approved_baseline) return 0;
    return "missing_current_owner";
}

static const char *assess(int ram_state, int ram_slots, int *has_stage, int *fresh)
{
    *has_stage=0; *fresh=0;
    if (!package_state_copies_trusted()) return package_state_copy_reason();
    enum module_slot_evidence evidence=module_slots_evidence();
    if (evidence == MS_UNAVAILABLE || evidence == MS_UNCERTAIN) return module_slots_evidence_reason();
    uint32_t n=view_count(ram_state);
    if (n > PSTATE_MAX) return "package_record_bounds";
    for (uint32_t i=0;i<n;i++) {
        const struct pkg_pstate *s=view_get(ram_state,i);
        if (!record_flags_valid(s)) return "package_record_inconsistent";
        if (!s->extension_valid && (live(s) || s->rolled_back || s->transaction_phase > PST_TX_COMMITTED))
            return "legacy_ownership_unbound";
        for (uint32_t j=0;j<i;j++)
            if (!kstricmp(view_get(ram_state,j)->package_id,s->package_id)) return "duplicate_package_identity";
        if (s->extension_valid) {
            int found=0;
            for (uint32_t j=0;j<SLOT_MODULE_COUNT;j++)
                if (slot_view(ram_slots,j) && slot_view(ram_slots,j)->module == s->target_module) found=1;
            if (!found) return "unsupported_owned_module";
        }
    }
    int pristine=1;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        const struct module_slot *v=slot_view(ram_slots,i);
        const struct module_slot *old=module_slot_durable_get(i);
        if (!v || !old || v->module != old->module || !module_slot_record_valid(v)) return "invalid_slot_record";
        const char *why=module_consistency(ram_state,v,evidence == MS_EMPTY && !ram_slots,has_stage);
        if (why) return why;
        if (!module_slot_is_pristine(v)) pristine=0;
    }
    *fresh=pristine && n==0;
    return 0;
}

int package_authority_trusted(void)
{
    if (authority_latched) return 0;
    int pending,fresh;
    const char *why=assess(0,0,&pending,&fresh);
    if (why && kstrcmp(package_state_copy_state(),"unavailable") && module_slots_evidence()!=MS_UNAVAILABLE)
        package_authority_note_uncertain(why);
    return !why;
}
const char *package_authority_state(void)
{
    if (!package_authority_trusted()) {
        if (!authority_latched && (!kstrcmp(package_state_copy_state(),"unavailable") || module_slots_evidence()==MS_UNAVAILABLE))
            return "unavailable";
        return "uncertain";
    }
    int pending,fresh; assess(0,0,&pending,&fresh);
    if (!kstrcmp(package_state_copy_state(),"degraded_current")) return "degraded_current";
    return fresh ? "fresh" : "verified";
}
const char *package_authority_reason(void)
{
    if (authority_latched) return latched_reason;
    int pending,fresh; const char *why=assess(0,0,&pending,&fresh);
    return why ? why : (pending ? "validated_staged_slot_evidence" : "joint_package_and_slot_evidence");
}

static const struct pkg_pstate *find_view(int ram,const char *id)
{
    for(uint32_t i=0;i<view_count(ram);i++)
        if(!kstricmp(view_get(ram,i)->package_id,id)) return view_get(ram,i);
    return 0;
}
static const struct module_slot *module_durable(uint8_t module)
{
    for(uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        const struct module_slot *v=module_slot_durable_get(i);
        if(v && v->module==module) return v;
    }
    return 0;
}
static int same_binding(const struct pkg_pstate *a,const struct pkg_pstate *b)
{
    return !kstricmp(a->package_id,b->package_id) && !kstrcmp(a->from_version,b->from_version) &&
        !kstrcmp(a->to_version,b->to_version) && a->manifest_identity==b->manifest_identity &&
        a->target_module==b->target_module && a->extension_valid==b->extension_valid;
}
static int same_record(const struct pkg_pstate *a,const struct pkg_pstate *b,int ignore_before)
{
    return same_binding(a,b) && a->status==b->status && a->approved==b->approved && a->applied==b->applied &&
        a->verified==b->verified && a->failed==b->failed && a->rolled_back==b->rolled_back &&
        (ignore_before || a->slot_before_apply==b->slot_before_apply) && a->slot_after_apply==b->slot_after_apply &&
        a->last_result_code==b->last_result_code && a->criticals_at_apply==b->criticals_at_apply &&
        a->health_at_apply==b->health_at_apply && a->transaction_phase==b->transaction_phase &&
        a->status_before_transaction==b->status_before_transaction;
}
static int binding_valid(const struct pkg_pstate *s)
{
    const struct pkg_manifest *m=manifest_find(s->package_id);const char *why;
    return s->extension_valid && m && manifest_verify(m,&why) && s->manifest_identity==manifest_identity(m) &&
        s->target_module==m->target_module && !kstrcmp(s->from_version,m->from_version) && !kstrcmp(s->to_version,m->to_version);
}
static int publishes_apply(uint8_t module)
{
    const struct module_slot *v=module_durable(module);
    for(uint32_t i=0;v && i<package_state_durable_count();i++) {
        const struct pkg_pstate *old=package_state_durable_get(i);
        const struct pkg_pstate *next=find_view(1,old->package_id);
        if(old->target_module==module && old->transaction_phase==PST_TX_APPLY_STAGED && next &&
           next->transaction_phase==PST_TX_COMMITTED && next->status==PKGST_APPLIED &&
           same_binding(old,next) && next->slot_before_apply==old->slot_before_apply &&
           next->slot_after_apply==old->slot_after_apply && staged_result(old,v)==PKGST_APPLIED) return 1;
    }
    return 0;
}
static int state_edges_valid(void)
{
    /* A coherent snapshot is not enough: ownership transfer/retirement must
       publish the specific durable intent, not synthesize a different owner. */
    for(uint32_t i=0;i<package_state_durable_count();i++)
        if(!find_view(1,package_state_durable_get(i)->package_id)) return 0;
    for(uint32_t i=0;i<package_state_count();i++) {
        const struct pkg_pstate *n=package_state_get(i), *o=find_view(0,n->package_id);
        if(o && same_record(o,n,0)) continue;
        if(o && n->slot_before_apply==SLOT_NONE && same_record(o,n,1) && publishes_apply(n->target_module)) continue;
        if(!o) {
            if(n->status!=PKGST_APPROVED || n->transaction_phase!=PST_TX_COMMITTED ||
               n->slot_before_apply!=SLOT_A || n->slot_after_apply!=SLOT_A || !binding_valid(n)) return 0;
            continue;
        }
        if(n->transaction_phase>PST_TX_COMMITTED) {
            if(o->transaction_phase!=PST_TX_COMMITTED || !same_binding(o,n) || n->status!=o->status ||
               n->status_before_transaction!=o->status) return 0;
            if(n->transaction_phase==PST_TX_APPLY_STAGED) {
                if(o->status!=PKGST_APPROVED) return 0;
            } else if(n->slot_before_apply!=o->slot_before_apply || n->slot_after_apply!=o->slot_after_apply ||
                      n->criticals_at_apply!=o->criticals_at_apply || n->health_at_apply!=o->health_at_apply ||
                      (n->transaction_phase==PST_TX_VERIFY_STAGED ? o->status!=PKGST_APPLIED : !live(o))) return 0;
            continue;
        }
        if(o->transaction_phase>PST_TX_COMMITTED) {
            const struct module_slot *v=module_durable(o->target_module);
            if(!v || !same_binding(o,n) || n->status!=staged_result(o,v) ||
               n->status_before_transaction!=n->status || n->slot_before_apply!=o->slot_before_apply ||
               n->slot_after_apply!=o->slot_after_apply || n->criticals_at_apply!=o->criticals_at_apply ||
               n->health_at_apply!=o->health_at_apply) return 0;
            continue;
        }
        if(n->status==PKGST_APPROVED && binding_valid(n) && !(live(o) && o->slot_before_apply!=SLOT_NONE) &&
           n->slot_before_apply==o->slot_before_apply && n->slot_after_apply==o->slot_after_apply) continue;
        if(o->status==PKGST_APPLIED && n->status==PKGST_FAILED && same_binding(o,n) &&
           n->slot_before_apply==o->slot_before_apply && n->slot_after_apply==o->slot_after_apply &&
           n->criticals_at_apply==o->criticals_at_apply && n->health_at_apply==o->health_at_apply &&
           n->last_result_code==0) continue;
        return 0;
    }
    return 1;
}

int package_authority_allow_state_write(void)
{
    /* Any refused write while a reset grant is held consumes the grant: a
       retry, corrected or not, must acquire a new one. */
    if (authority_latched || !package_state_copies_trusted()) { reset_grant=0; return 0; }
    if (reset_grant) {
        if (package_state_count()==0) return 1;
        reset_grant=0;
        error_log_class(SEV_WARNING,HEALTH_OPERATOR_INPUT,"PST-REJECT-005","PackageAuthority",
                        "Reset grant permits only empty package state","reset_grant_revoked");
        return 0;
    }
    if (!package_authority_trusted()) return 0;
    int pending,fresh;
    const char *why=assess(1,0,&pending,&fresh);
    if (!why && !state_edges_valid()) why="unproven_state_transition";
    if (why) error_log_class(SEV_WARNING,HEALTH_OPERATOR_INPUT,"PST-REJECT-004","PackageAuthority",
                             "Proposed package state lacks joint authority",why);
    return !why;
}

static int same_slot_record(const struct module_slot *a,const struct module_slot *b)
{
    if (a->module!=b->module || a->active_slot!=b->active_slot ||
        a->rollback_slot!=b->rollback_slot || a->pending_verify!=b->pending_verify ||
        a->slot_a_verified!=b->slot_a_verified || a->slot_b_verified!=b->slot_b_verified) return 0;
    /* Compare every serialized byte, not struct padding or just string prefixes. */
    for (uint32_t i=0;i<SLOT_VER_MAX;i++)
        if (a->slot_a_version[i]!=b->slot_a_version[i] || a->slot_b_version[i]!=b->slot_b_version[i]) return 0;
    return 1;
}

static int slot_edges_valid(void)
{
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        const struct module_slot *old=module_slot_durable_get(i), *next=module_slot_get(i);
        if (!old || !next) return 0;
        if (same_slot_record(old,next)) continue; /* unchanged records grant no rights */
        if (module_slots_evidence()!=MS_VALID) return 0;
        const struct pkg_pstate *stage=0;
        for (uint32_t j=0;j<package_state_durable_count();j++) {
            const struct pkg_pstate *s=package_state_durable_get(j);
            if (s->target_module==old->module && s->transaction_phase>PST_TX_COMMITTED) {
                if (stage) return 0;
                stage=s;
            }
        }
        if (!stage || !bound_frame(stage) || stage->status_before_transaction!=stage->status) return 0;

        /* Recovery may classify either observed side. A WRITE must instead
           advance the durable before side and preserve every unaffected field. */
        struct module_slot expected=*old;
        if (stage->transaction_phase==PST_TX_APPLY_STAGED) {
            if (stage->status!=PKGST_APPROVED || old->active_slot!=stage->slot_before_apply ||
                old->pending_verify || !verified_at(old,old->active_slot) ||
                kstrcmp(module_slot_active_version(old),stage->from_version)) return 0;
            if (stage->slot_after_apply==SLOT_A) {
                kstrcpy(expected.slot_a_version,stage->to_version,SLOT_VER_MAX);
                expected.slot_a_verified=0;
            } else {
                kstrcpy(expected.slot_b_version,stage->to_version,SLOT_VER_MAX);
                expected.slot_b_verified=0;
            }
            expected.active_slot=stage->slot_after_apply;
            expected.rollback_slot=stage->slot_before_apply;
            expected.pending_verify=1;
        } else if (stage->transaction_phase==PST_TX_VERIFY_STAGED) {
            if (stage->status!=PKGST_APPLIED || !current_shape(stage,old,0)) return 0;
            if (old->active_slot==SLOT_A) expected.slot_a_verified=1;
            else expected.slot_b_verified=1;
            expected.pending_verify=0;
        } else if (stage->transaction_phase==PST_TX_ROLLBACK_STAGED) {
            if (!live(stage) || !current_shape(stage,old,stage->status==PKGST_VERIFIED)) return 0;
            expected.active_slot=stage->slot_before_apply;
            expected.rollback_slot=SLOT_NONE;
            expected.pending_verify=0;
        } else return 0;
        if (!same_slot_record(&expected,next)) return 0;
    }
    return 1;
}

/* The reset exception's only acceptable proposal: each record equals, in every
   serialized field and version-array byte, the canonical factory record at the
   same table position. This fixes identity, order and uniqueness. */
static int exact_factory_image(void)
{
    struct module_slot canon;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        const struct module_slot *v=module_slot_get(i);
        if (!v || !module_slot_factory_record(i,&canon) || !same_slot_record(&canon,v)) return 0;
    }
    return 1;
}

int package_authority_allow_slot_write(void)
{
    if (authority_latched || !package_state_copies_trusted()) { reset_grant=0; return 0; }
    if (reset_grant) {
        /* One-shot: the factory slot image is reset's final write. The grant is
           consumed whether this proposal is accepted or rejected; an accepted
           write that then fails at the ATA layer also latches uncertainty. */
        reset_grant=0;
        if (package_state_durable_count()==0 && exact_factory_image()) return 1;
        error_log_class(SEV_WARNING,HEALTH_OPERATOR_INPUT,"SLOT-REJECT-002","PackageAuthority",
                        "Reset proposal is not the exact factory slot image","reset_grant_revoked");
        return 0;
    }
    int all_factory=1;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++)
        if (!module_slot_record_valid(module_slot_get(i)) || !module_slot_is_pristine(module_slot_get(i))) all_factory=0;
    if (!package_authority_trusted()) return 0;
    int pending,fresh;
    const char *why=assess(0,1,&pending,&fresh);
    if (why) return 0;
    if (!slot_edges_valid()) {
        error_log_class(SEV_WARNING,HEALTH_OPERATOR_INPUT,"SLOT-REJECT-001","PackageAuthority",
                        "Proposed module slots lack matching forward intent","unproven_slot_transition");
        return 0;
    }
    return pending || (module_slots_evidence()==MS_EMPTY && all_factory);
}
int package_authority_recovery_status(const struct pkg_pstate *s)
{
    if (!package_authority_trusted()) return -1;
    for (uint32_t i=0;i<SLOT_MODULE_COUNT;i++) {
        const struct module_slot *v=module_slot_durable_get(i);
        if (v && v->module==s->target_module) return staged_result(s,v);
    }
    return -1;
}
int package_authority_begin_reset(void)
{
    if (!package_authority_trusted()) return 0;
    int pending,fresh; if (assess(0,0,&pending,&fresh) || pending) return 0;
    reset_grant=1;
    return 1;
}
void package_authority_end_reset(void) { reset_grant=0; }
int package_authority_reset_active(void) { return reset_grant && !authority_latched; }
