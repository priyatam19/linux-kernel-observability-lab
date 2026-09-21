#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include "wpt-util.h"

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
