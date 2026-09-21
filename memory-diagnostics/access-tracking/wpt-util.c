
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include "wpt-util.h"
#include <linux/pgtable.h>
#include <asm/pgtable.h>

// Use get_user_pages_fast to retrieve PFN from a user VA
u64 hva2hpa(struct mm_struct *mm, unsigned long addr) {
    struct page *page = NULL;
    unsigned long pfn;
    u64 pa;

    if (!mm)
        return 0;

    // Pin the page (non-write, non-force)
    if (get_user_pages_fast(addr, 1, 0, &page) <= 0) {
        pr_err("hva2hpa: failed to get page for VA 0x%lx\n", addr);
        return 0;
    }

    pfn = page_to_pfn(page);
    pa = ((u64)pfn << PAGE_SHIFT) | (addr & ~PAGE_MASK);
    put_page(page);  // release reference
    return pa;
}



pte_t *va2pte(struct mm_struct *mm, unsigned long addr) {
    pgd_t *pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return NULL;

    p4d_t *p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return NULL;

    pud_t *pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud)) return NULL;

    pmd_t *pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) return NULL;

    // Skip large page check (we only use 4KB pages)
    pte_t *pte = pte_offset_kernel(pmd, addr);
    if (!pte) return NULL;

    return pte;
}
