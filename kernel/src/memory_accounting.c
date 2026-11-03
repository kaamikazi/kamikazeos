#include "memory_accounting.h"
#include "string.h"

static int valid_map(const struct mp_ram_region *m, uint32_t n)
{
    if (!m || !n || n>MP_RAM_MAX) return 0;
    for (uint32_t i=0;i<n;i++) if (m[i].end<m[i].start || m[i].usable>1) return 0;
    return 1;
}
static int page_usable(const struct mp_ram_region *m, uint32_t n, uint64_t page)
{
    uint64_t end=page+MP_PAGE_SIZE;
    for (uint32_t i=0;i<n;i++)
        if (!m[i].usable && m[i].start<m[i].end && m[i].start<end && m[i].end>page) return 0;
    /* Adjacent/overlapping usable entries may jointly cover a whole page. */
    uint64_t at=page;
    while (at<end) {
        uint64_t next=at;
        for (uint32_t i=0;i<n;i++)
            if (m[i].usable && m[i].start<=at && m[i].end>next)
                next=m[i].end<end?m[i].end:end;
        if (next==at) return 0;
        at=next;
    }
    return 1;
}
int mp_ram_page_usable(const struct mp_ram_region *m, uint32_t n, uint64_t page)
{
    if (!valid_map(m,n) || (page&(MP_PAGE_SIZE-1u)) ||
        page>UINT64_MAX-MP_PAGE_SIZE) return 0;
    return page_usable(m,n,page);
}

static uint64_t usable_union(const struct mp_ram_region *m, uint32_t n)
{
    uint64_t points[MP_RAM_MAX*2u], total=0;
    uint32_t count=0;
    for (uint32_t i=0;i<n;i++) { points[count++]=m[i].start;points[count++]=m[i].end; }
    for (uint32_t i=1;i<count;i++) {
        uint64_t p=points[i];uint32_t j=i;
        while (j && points[j-1]>p) {points[j]=points[j-1];j--;}
        points[j]=p;
    }
    for (uint32_t i=1;i<count;i++) {
        uint64_t a=points[i-1],b=points[i];int usable=0,reserved=0;
        if (a==b) continue;
        for (uint32_t j=0;j<n;j++) if (m[j].start<b && m[j].end>a) {
            if (m[j].usable) usable=1;else reserved=1;
        }
        if (usable && !reserved) total+=b-a; /* disjoint intervals cannot overflow */
    }
    return total;
}

static int runtime_reserved(const struct mp_layout *l, uint32_t page,
                             const struct mp_reservation *extra, uint32_t n)
{
    uint64_t end=(uint64_t)page+MP_PAGE_SIZE;
    if (page<0x100000u ||
        (page>=l->regions[MP_TEXT].start && page<l->regions[MP_USER_GUARD_HIGH].end) ||
        (l->heap_available && page>=l->regions[MP_HEAP].start && page<l->regions[MP_HEAP].end)) return 1;
    for (uint32_t i=0;i<n;i++)
        if (extra[i].start<extra[i].end && extra[i].start<end && extra[i].end>page) return 1;
    return 0;
}

int mp_account_ram(const struct mp_layout *l, const struct mp_ram_region *m, uint32_t n,
                   const struct mp_reservation *extra, uint32_t extra_count,
                   uint32_t tracked_end, int trusted, int mapping_active,
                   struct mp_ram_accounting *out)
{
    if (!out) return 0;
    kmemset(out,0,sizeof(*out));out->reason="invalid_accounting_input";
    if (!mp_layout_validate(l,0) || !valid_map(m,n) || extra_count>MP_RESERVATION_MAX ||
        (extra_count && !extra) || tracked_end>MP_TRACKED_END ||
        (tracked_end&(MP_PAGE_SIZE-1u)) || (trusted!=0 && trusted!=1) ||
        (mapping_active!=0 && mapping_active!=1)) return 0;
    for (uint32_t i=0;i<extra_count;i++) if (extra[i].end<extra[i].start) return 0;
    out->valid=1;out->discovery_trusted=(uint32_t)trusted;
    out->mapping_active=(uint32_t)mapping_active;
    out->described_usable_bytes=usable_union(m,n);
    out->detected_usable_bytes=trusted?out->described_usable_bytes:0;
    out->mapped_present_pages=mapping_active?MP_APERTURE_END/MP_PAGE_SIZE:0;
    uint32_t end=tracked_end>MP_APERTURE_END?tracked_end:MP_APERTURE_END;
    for (uint32_t page=0;page<end;page+=MP_PAGE_SIZE) {
        int usable=page_usable(m,n,page);
        int mapped=mapping_active && page<MP_APERTURE_END;
        if (mapped) {
            if (usable) out->mapped_usable_pages++;else out->mapped_nonram_pages++;
        }
        if (page>=tracked_end || !usable) continue;
        out->tracked_usable_pages++;
        int reserved=runtime_reserved(l,page,extra,extra_count);
        if (reserved) out->reserved_usable_pages++;
        if (!mapped) out->unmapped_usable_pages++;
        if (trusted && mapped && !reserved &&
            mp_page_flags(l,page,MP_LEGACY_ACTIVE,1)==(MP_PRESENT|MP_WRITABLE)) out->eligible_pages++;
    }
    out->reason=trusted?"shadow_only_not_an_allocator":"untrusted_discovery_eligibility_held";
    return 1;
}
