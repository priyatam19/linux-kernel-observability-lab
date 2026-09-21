#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "wpt-util.h"

#define PROC_FILENAME "lkp25p3"
#define INPUT_BUF_SIZE 128
#define MONITOR_DURATION_SEC 120
#define MONITOR_INTERVAL_MS 100
#define LOG_INTERVAL_MS 1000
#define PAGE_SIZE_KB 4
#define NUM_PAGES (1024 * 1024 * 1024 / 4096)

static struct proc_dir_entry *proc_entry;
static char input_buf[INPUT_BUF_SIZE];
static void *test_buffer = NULL;
static struct task_struct *monitor_thread;
static struct mm_struct *test_mm = NULL;

static inline struct page *va_to_page(struct mm_struct *mm, unsigned long va) {
    return virt_to_page((void *)va);
}

static int monitor_fn(void *data) {
    struct mm_struct *mm = test_mm;
    u64 *page_access_counts = kcalloc(NUM_PAGES, sizeof(u64), GFP_KERNEL);
    u64 elapsed_time_ms = 0;

    if (!page_access_counts || !test_buffer)
        return -ENOMEM;

    pr_info("[lkp25p3] Monitoring thread started\n");

    while (!kthread_should_stop() && elapsed_time_ms < MONITOR_DURATION_SEC * 1000) {
        unsigned long va = (unsigned long)test_buffer;

        for (int i = 0; i < NUM_PAGES; i++, va += PAGE_SIZE) {
            pte_t *pte = va2pte(mm, va);
            if (pte && pte_present(*pte)) {
                if (pte_young(*pte)) {
                    page_access_counts[i]++;
                    *pte = pte_mkold(*pte);             // clear the A-bit
                    set_pte_at(mm, va, pte, *pte);      // write it back
                }
                pte_unmap(pte);
            }
        }

        if (elapsed_time_ms % LOG_INTERVAL_MS == 0) {
            pr_info("[lkp25p3] Access stats at %llu sec:\n", elapsed_time_ms / 1000);
            for (int i = 0; i < NUM_PAGES; i++) {
                if (page_access_counts[i] > 0)
                    pr_info("Page %d (VA 0x%lx): %llu accesses\n", i,
                            (unsigned long)test_buffer + i * PAGE_SIZE,
                            page_access_counts[i]);
            }
        }

        msleep(MONITOR_INTERVAL_MS);
        elapsed_time_ms += MONITOR_INTERVAL_MS;
        pr_info("[lkp25p3] Monitor loop at %llu ms\n", elapsed_time_ms);

    }

    pr_info("[lkp25p3] Monitoring complete.\n");
    kfree(page_access_counts);
    return 0;
}

static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos) {
    if (count >= INPUT_BUF_SIZE)
        return -EINVAL;

    if (copy_from_user(input_buf, ubuf, count))
        return -EFAULT;
    input_buf[count] = '\0';

    if (strncmp(input_buf, "alloc", 5) == 0) {
        pid_t target_pid;
        if (sscanf(input_buf + 6, "%d", &target_pid) != 1) {
            pr_err("[lkp25p3] Failed to parse PID\n");
            return -EINVAL;
        }

        struct task_struct *task = pid_task(find_vpid(target_pid), PIDTYPE_PID);
        if (!task || !task->mm) {
            pr_err("[lkp25p3] Could not find mm_struct for PID %d\n", target_pid);
            return -ESRCH;
        }

        test_mm = task->mm;

        if (test_buffer) {
            pr_info("[lkp25p3] Buffer already allocated.\n");
            return count;
        }

        test_buffer = vmalloc_user(1024L * 1024L * 1024L);  // Optional if you use shared VA instead
        if (!test_buffer)
            return -ENOMEM;

        pr_info("[lkp25p3] Got mm_struct for PID %d\n", target_pid);

        // Start monitor thread AFTER allocation
        if (!monitor_thread) {
            monitor_thread = kthread_run(monitor_fn, NULL, "monitor_kthread");
            if (IS_ERR(monitor_thread)) {
                pr_err("[lkp25p3] Failed to start monitoring thread\n");
                monitor_thread = NULL;
                return PTR_ERR(monitor_thread);
            }
            pr_info("[lkp25p3] Monitor thread started.\n");
        }

        return count;
    }

    return count;
}

static const struct proc_ops proc_fops = {
    .proc_write = proc_write,
};

static int __init lkp25p3_init(void) {
    proc_entry = proc_create(PROC_FILENAME, 0666, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("[lkp25p3] Failed to create /proc entry\n");
        return -ENOMEM;
    }

    pr_info("[lkp25p3] Module loaded. /proc/%s ready.\n", PROC_FILENAME);
    return 0;
}

static void __exit lkp25p3_exit(void) {
    if (monitor_thread)
        kthread_stop(monitor_thread);
    if (test_buffer)
        vfree(test_buffer);
    proc_remove(proc_entry);
    pr_info("[lkp25p3] Module unloaded.\n");
}

module_init(lkp25p3_init);
module_exit(lkp25p3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Priyatam Annambhotla");
MODULE_DESCRIPTION("Project 3 Part 2.4: Page Access Monitor");
