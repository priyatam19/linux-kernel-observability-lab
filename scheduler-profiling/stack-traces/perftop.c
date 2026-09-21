#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/stacktrace.h>

#define MAX_RECORDS 4096
static unsigned int record_count;
static unsigned long dropped;

#define PERFTOP_HASH_BITS 10
#define STACK_TRACE_MAX_DEPTH 16

static struct kprobe kp;
DEFINE_HASHTABLE(perftop_table, PERFTOP_HASH_BITS);
DEFINE_SPINLOCK(perftop_lock);

struct stack_count {
    struct hlist_node node;
    unsigned long stack_trace[STACK_TRACE_MAX_DEPTH];
    unsigned int depth;
    unsigned long count;
    unsigned long hash;
};

/* Compute hash for stack trace */
static unsigned long hash_stack_trace(unsigned long *stack, int depth)
{
    unsigned long hash = 0;
    int i;
    for (i = 0; i < depth; i++) {
        hash ^= stack[i] + (hash << 6) + (hash >> 2);
    }
    return hash;
}

/* Hook function for kprobe */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct stack_count *entry, *new_entry;
    unsigned long stack_trace[STACK_TRACE_MAX_DEPTH] = {0};
    unsigned long hash;
    int depth;
    unsigned long flags;

    /* Capture the current kernel stack only. */
    depth = stack_trace_save(stack_trace, STACK_TRACE_MAX_DEPTH, 0);

    hash = hash_stack_trace(stack_trace, depth);

    spin_lock_irqsave(&perftop_lock, flags);

    /* Check if the stack trace exists in the hash table */
    hash_for_each_possible(perftop_table, entry, node, hash) {
        if (entry->hash == hash && entry->depth == depth &&
            memcmp(entry->stack_trace, stack_trace, depth * sizeof(unsigned long)) == 0) {
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
    new_entry = kzalloc(sizeof(struct stack_count), GFP_ATOMIC);
    if (new_entry) {
        memcpy(new_entry->stack_trace, stack_trace, depth * sizeof(unsigned long));
        new_entry->depth = depth;
        new_entry->count = 1;
        new_entry->hash = hash;
        hash_add(perftop_table, &new_entry->node, hash);
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
    struct stack_count *entry;
    int bkt;
    unsigned long flags;
    int i;

    spin_lock_irqsave(&perftop_lock, flags);
    seq_puts(m, "Stack Trace (up to 16 frames) | Fair-picker entry count\n");
    seq_puts(m, "--------------------------------------------------\n");

    seq_printf(m, "Dropped samples: %lu; kprobe misses: %lu\n", dropped, kp.nmissed);
    hash_for_each(perftop_table, bkt, entry, node) {
        seq_printf(m, "Count: %lu\n", entry->count);
        for (i = 0; i < entry->depth; i++) {
            seq_printf(m, "  [%d] %pS\n", i, (void *)entry->stack_trace[i]);
        }
        seq_puts(m, "--------------------------------------\n");
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
    kp.symbol_name = "pick_next_task_fair";
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
    struct stack_count *entry;
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
MODULE_DESCRIPTION("Kernel module that tracks scheduling events using stack traces with kprobes");
MODULE_VERSION("2.1");
