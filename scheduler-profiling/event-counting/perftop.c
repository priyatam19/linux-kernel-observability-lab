#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>

#ifndef CONFIG_X86_64
#error "This probe decodes the Linux v6.12 x86-64 register ABI"
#endif
#define MAX_RECORDS 4096
static unsigned int record_count;
static unsigned long dropped;

#define PERFTOP_HASH_BITS 10  // Define the size of the hash table

static struct kprobe kp;
DEFINE_HASHTABLE(perftop_table, PERFTOP_HASH_BITS);
DEFINE_SPINLOCK(perftop_lock);

struct pid_count {
    struct hlist_node node;
    pid_t pid;
    unsigned long count;
};

/* Hook function for kprobe */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *prev_task;
    pid_t pid;
    struct pid_count *entry;
    struct pid_count *new_entry;
    unsigned long flags;

    /* Linux v6.12 x86-64: argument 2 is prev, not the selected task. */
    prev_task = (struct task_struct *)regs->si;
    if (!prev_task) {
        return 0;
    }

    /* Extract the PID correctly */
    pid = task_pid_nr(prev_task);

    /* If PID is invalid, return early */
    if (pid < 0) {
        return 0;
    }

    spin_lock_irqsave(&perftop_lock, flags);

    /* Check if the PID exists in the hash table */
    hash_for_each_possible(perftop_table, entry, node, pid) {
        if (entry->pid == pid) {
            entry->count++;
            spin_unlock_irqrestore(&perftop_lock, flags);
            return 0;
        }
    }

    /* If not found, add a new entry */
    if (record_count >= MAX_RECORDS) {
        dropped++;
        spin_unlock_irqrestore(&perftop_lock, flags);
        return 0;
    }
    new_entry = kmalloc(sizeof(struct pid_count), GFP_ATOMIC);
    if (new_entry) {
        new_entry->pid = pid;
        new_entry->count = 1;
        hash_add(perftop_table, &new_entry->node, pid);
        record_count++;
    } else {
        dropped++;
    }

    spin_unlock_irqrestore(&perftop_lock, flags);
    return 0;
}

/* Function to display results in /proc/perftop */
static int perftop_show(struct seq_file *m, void *v)
{
    struct pid_count *entry;
    int bkt;
    unsigned long flags;

    spin_lock_irqsave(&perftop_lock, flags);
    seq_puts(m, "Previous TID\tFair-picker entry count\n");

    seq_printf(m, "Dropped samples: %lu; kprobe misses: %lu\n", dropped, kp.nmissed);
    hash_for_each(perftop_table, bkt, entry, node) {
        seq_printf(m, "%d\t%lu\n", entry->pid, entry->count);
    }

    spin_unlock_irqrestore(&perftop_lock, flags);
    return 0;
}

/* Open function for /proc */
static int perftop_open(struct inode *inode, struct file *file)
{
    return single_open(file, perftop_show, NULL);
}

/* File operations structure */
static const struct proc_ops perftop_fops = {
    .proc_open    = perftop_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Module initialization */
static int __init perftop_init(void)
{
    int ret;

    if (!proc_create("perftop", 0400, NULL, &perftop_fops))
        return -ENOMEM;

    /* Register the kprobe */
    kp.symbol_name = "pick_prev_task_fair";
    kp.pre_handler = handler_pre;
    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("Failed to register kprobe: %d\n", ret);
        remove_proc_entry("perftop", NULL);
        return ret;
    }


    pr_info("perftop module loaded successfully\n");
    return 0;
}

/* Module cleanup */
static void __exit perftop_exit(void)
{
    struct pid_count *entry;
    struct hlist_node *tmp;
    int bkt;

    /* Unregister the kprobe */
    unregister_kprobe(&kp);

    /* Remove /proc entry */
    remove_proc_entry("perftop", NULL);

    /* Free hash table memory */
    hash_for_each_safe(perftop_table, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }

    pr_info("perftop module unloaded\n");
}

module_init(perftop_init);
module_exit(perftop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Priyatam Annambhotla");
MODULE_DESCRIPTION("Kernel module that counts fair-picker entries by previous TID");
MODULE_VERSION("1.0");
