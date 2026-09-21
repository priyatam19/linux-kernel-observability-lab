#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched/mm.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>

#define PROC_FILENAME "lkp25p3"
#define INPUT_BUF_SIZE 128

static struct proc_dir_entry *proc_entry;
static char input_buf[INPUT_BUF_SIZE];

extern u64 hva2hpa(struct mm_struct *mm, unsigned long addr);

static ssize_t proc_write(struct file *file, const char __user *ubuf,
                          size_t count, loff_t *ppos)
{
    unsigned long va;
    u64 pa;
    struct timespec64 start, end;
    u64 latency_us;

    if (count >= INPUT_BUF_SIZE)
        return -EINVAL;

    if (copy_from_user(input_buf, ubuf, count))
        return -EFAULT;
    input_buf[count] = '\0';

    // === Handle VMA DUMP ===
    if ((strcmp(input_buf, "0") == 0) || (strcmp(input_buf, "0\n") == 0)) {
        struct task_struct *task = pid_task(find_vpid(1), PIDTYPE_PID);
        struct mm_struct *mm;
        struct vm_area_struct *vma;
        unsigned long start = 0;
        char perms[5];

        if (!task) {
            pr_err("[lkp25p3] Could not find PID 1\n");
            return -ESRCH;
        }

        mm = get_task_mm(task);
        if (!mm) {
            pr_err("[lkp25p3] Failed to get mm_struct for PID 1\n");
            return -EINVAL;
        }

        down_read(&mm->mmap_lock);
        while ((vma = find_vma(mm, start))) {
            snprintf(perms, sizeof(perms), "%c%c%c%c",
                     vma->vm_flags & VM_READ    ? 'r' : '-',
                     vma->vm_flags & VM_WRITE   ? 'w' : '-',
                     vma->vm_flags & VM_EXEC    ? 'x' : '-',
                     vma->vm_flags & VM_SHARED  ? 's' : 'p');

            if (vma->vm_file) {
                char *path = (char *)__get_free_page(GFP_KERNEL);
                char *filename = d_path(&vma->vm_file->f_path, path, PAGE_SIZE);

                pr_info("VMA: [0x%lx - 0x%lx] (%lu KB)\nPermissions: %s\nType: File-backed Mapping\nFile: %s\nOffset: 0x%lx\n",
                        vma->vm_start, vma->vm_end,
                        (vma->vm_end - vma->vm_start) >> 10,
                        perms,
                        IS_ERR(filename) ? "unknown" : filename,
                        vma->vm_pgoff << PAGE_SHIFT);

                free_page((unsigned long)path);
            } else {
                pr_info("VMA: [0x%lx - 0x%lx] (%lu KB)\nPermissions: %s\nType: Anonymous Mapping\n",
                        vma->vm_start, vma->vm_end,
                        (vma->vm_end - vma->vm_start) >> 10,
                        perms);
            }

            start = vma->vm_end;
        }
        up_read(&mm->mmap_lock);
        mmput(mm);
        return count;
    }

    // === VA -> PA Translation ===
    if (kstrtoul(input_buf, 16, &va) != 0) {
        pr_err("[lkp25p3] Invalid input: %s\n", input_buf);
        return -EINVAL;
    }

    ktime_get_real_ts64(&start);
    pa = hva2hpa(current->mm, va);
    ktime_get_real_ts64(&end);

    latency_us = (end.tv_sec - start.tv_sec) * 1000000 +
                 (end.tv_nsec - start.tv_nsec) / 1000;

    pr_info("[lkp25p3] VA: 0x%lx -> PA: 0x%llx | Latency: %llu us\n",
            va, pa, latency_us);

    return count;
}

static const struct proc_ops proc_fops = {
    .proc_write = proc_write,
};

static int __init lkp25p3_init(void)
{
    proc_entry = proc_create(PROC_FILENAME, 0666, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("[lkp25p3] Failed to create /proc entry\n");
        return -ENOMEM;
    }

    pr_info("[lkp25p3] Module loaded. /proc/%s ready.\n", PROC_FILENAME);
    return 0;
}

static void __exit lkp25p3_exit(void)
{
    proc_remove(proc_entry);
    pr_info("[lkp25p3] Module unloaded.\n");
}

module_init(lkp25p3_init);
module_exit(lkp25p3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Priyatam Annambhotla");
//MODULE_DESCRIPTION("Project 3 Part 2.3: VMA walker and VA->PA translator");
