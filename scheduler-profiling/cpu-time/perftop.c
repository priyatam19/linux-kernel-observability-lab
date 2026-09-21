/* Per-CPU switch intervals grouped by the kernel stack observed at interval end. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/kprobes.h>
#include <linux/sched/clock.h>
#include <linux/kallsyms.h>

#ifndef CONFIG_X86_64
#error "This experiment targets the x86-64 __switch_to implementation"
#endif
#define MAX_STACK_ENTRIES 4
#define MAX_RECORDS 4096
static unsigned int record_count;
static unsigned long dropped;
MODULE_LICENSE("GPL");

struct stack_record {
    struct rb_node node;
    unsigned long hash;
    u64 total_time;
    unsigned int nr_entries;
    unsigned long entries[MAX_STACK_ENTRIES];
};

static struct rb_root rbtree_root = RB_ROOT;
static DEFINE_SPINLOCK(tree_lock);

struct cpu_task_data {
    u64 last_start;
};
static DEFINE_PER_CPU(struct cpu_task_data, cpu_data);

static unsigned long hash_stack(unsigned long *entries, int nr)
{
    unsigned long hash = 0;
    for (int i = 0; i < nr; i++) {
        hash += entries[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static void insert_or_update_rbtree(unsigned long hash, u64 time, unsigned long *entries, unsigned int nr)
{
    struct rb_node **link = &rbtree_root.rb_node, *parent = NULL;
    struct stack_record *rec;
    unsigned long flags;
    int cmp;

    spin_lock_irqsave(&tree_lock, flags);
    while (*link) {
        parent = *link;
        rec = rb_entry(parent, struct stack_record, node);

        /* A total ordering is required even when stack hashes collide. */
        cmp = hash < rec->hash ? -1 : hash > rec->hash ? 1 : 0;
        if (!cmp)
            cmp = nr < rec->nr_entries ? -1 : nr > rec->nr_entries ? 1 : 0;
        if (!cmp)
            cmp = memcmp(entries, rec->entries, nr * sizeof(*entries));
        if (cmp < 0)
            link = &(*link)->rb_left;
        else if (cmp > 0)
            link = &(*link)->rb_right;
        else {
            rec->total_time += time;
            spin_unlock_irqrestore(&tree_lock, flags);
            return;
        }
    }
    if (record_count >= MAX_RECORDS) {
        dropped++;
        spin_unlock_irqrestore(&tree_lock, flags);
        return;
    }

    rec = kmalloc(sizeof(*rec), GFP_ATOMIC);
    if (!rec) {
        dropped++;
        spin_unlock_irqrestore(&tree_lock, flags);
        return;
    }
    rec->hash = hash;
    rec->total_time = time;
    rec->nr_entries = nr;
    memcpy(rec->entries, entries, nr * sizeof(unsigned long));

    rb_link_node(&rec->node, parent, link);
    rb_insert_color(&rec->node, &rbtree_root);
    record_count++;
    spin_unlock_irqrestore(&tree_lock, flags);
}

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    int cpu = smp_processor_id();
    struct cpu_task_data *data = &per_cpu(cpu_data, cpu);
    u64 now = sched_clock();

    if (data->last_start) {
        unsigned long entries[MAX_STACK_ENTRIES];
        unsigned int nr_entries = stack_trace_save(entries, MAX_STACK_ENTRIES, 0);
        if (nr_entries > 0) {
            unsigned long hash = hash_stack(entries, nr_entries);
            u64 delta = now - data->last_start;
            insert_or_update_rbtree(hash, delta, entries, nr_entries);
        }
    }

    data->last_start = now;
    return 0;
}

static struct kprobe kp = {
    .symbol_name = "__switch_to",
    .pre_handler = handler_pre,
};

static int show_proc(struct seq_file *m, void *v)
{
    struct rb_node *node;
    int rank = 1;
    unsigned long flags;

    seq_puts(m, "=== First 20 stacks in tree order; accumulated switch intervals ===\n");
    spin_lock_irqsave(&tree_lock, flags);
    seq_printf(m, "Dropped samples: %lu; kprobe misses: %lu\n", dropped, kp.nmissed);
    for (node = rb_first(&rbtree_root); node && rank <= 20; node = rb_next(node), rank++) {
        struct stack_record *rec = rb_entry(node, struct stack_record, node);
        seq_printf(m, "Record %d:\n  Hash: 0x%lx\n  Interval sum: %llu ns\n", rank, rec->hash, rec->total_time);
        for (int i = 0; i < rec->nr_entries; i++) {
            char symname[KSYM_SYMBOL_LEN];
            sprint_symbol(symname, rec->entries[i]);
            seq_printf(m, "    [<%px>] %s\n", (void *)rec->entries[i], symname);
        }
        seq_puts(m, "\n");
    }
    spin_unlock_irqrestore(&tree_lock, flags);
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, show_proc, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init mod_init(void)
{
    int ret;

    if (!proc_create("stack_sched", 0400, NULL, &proc_fops))
        return -ENOMEM;
    ret = register_kprobe(&kp);
    if (ret) {
        remove_proc_entry("stack_sched", NULL);
        return ret;
    }
    pr_info("perftop switch-interval profiler loaded using __switch_to\n");
    return 0;
}

static void __exit mod_exit(void)
{
    struct rb_node *node;

    unregister_kprobe(&kp);
    remove_proc_entry("stack_sched", NULL);
    while ((node = rb_first(&rbtree_root))) {
        struct stack_record *rec = rb_entry(node, struct stack_record, node);
        rb_erase(node, &rbtree_root);
        kfree(rec);
    }
    pr_info("perftop switch-interval profiler unloaded\n");
}

module_init(mod_init);
module_exit(mod_exit);


MODULE_AUTHOR("Priyatam Annambhotla");
MODULE_DESCRIPTION("Switch-interval sampling by kernel stack; nanosecond estimates");
