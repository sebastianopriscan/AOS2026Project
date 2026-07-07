#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/printk.h>      
#include <linux/ptrace.h>       
#include <linux/syscalls.h>
#include <linux/version.h>

#include "include/hash_table/hash_table.h"

typedef struct dentry *(*dentry_extractor)(struct pt_regs *) ;

static struct kretprobe create_probe ;

static void linking_observer(dentry_extractor extractor, struct pt_regs *regs) {
    hash_table_bind_inode(extractor(regs)) ;
}

static void unlinking_observer(dentry_extractor extractor, struct pt_regs *regs) {
    hash_table_unbind_inode(extractor(regs)) ;
}

static struct dentry *extract_from_vfs_create(struct pt_regs *regs) {
    return (struct dentry *) regs->cx ;
}

static int vfs_create_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(extract_from_vfs_create(regs)) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_create_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    hash_table_bind_inode(dentry) ;

    return 0 ;
}