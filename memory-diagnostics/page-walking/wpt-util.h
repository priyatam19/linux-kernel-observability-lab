#ifndef _WPT_UTIL_H_
#define _WPT_UTIL_H_

#include <linux/mm.h>
#include <linux/types.h>

u64 hva2hpa(struct mm_struct *mm, unsigned long addr);
pte_t *va2pte(struct mm_struct *mm, unsigned long addr);

#endif
