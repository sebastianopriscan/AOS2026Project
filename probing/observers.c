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

#define UNISTD_64_ARG0(regs, cast) ((cast) regs->di)
#define UNISTD_64_ARG1(regs, cast) ((cast) regs->si)
#define UNISTD_64_ARG2(regs, cast) ((cast) regs->dx)
#define UNISTD_64_ARG3(regs, cast) ((cast) regs->cx)
#define UNISTD_64_ARG4(regs, cast) ((cast) regs->r8)
#define UNISTD_64_ARG5(regs, cast) ((cast) regs->r9)

static struct kretprobe create_probe ;

/* VFS CREATE */

static int vfs_create_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(UNISTD_64_ARG2(regs, struct dentry *)) ;
    if (dentry) {
        dget(dentry) ;
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_create_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    if (!regs->ax) hash_table_bind_inode(dentry) ;
    dput(dentry) ;

    return 0 ;
}

/* VFS LINK */

static int vfs_link_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(UNISTD_64_ARG2(regs, struct dentry *)) ;
    if (dentry) {
        dget(dentry) ;
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_link_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    if (!regs->ax) hash_table_bind_inode(dentry) ;
    dput(dentry) ;

    return 0 ;
}

/* VFS MKNOD */

static int vfs_mknod_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(UNISTD_64_ARG2(regs, struct dentry *)) ;
    if (dentry) {
        dget(dentry) ;
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_mknod_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    if (!regs->ax) hash_table_bind_inode(dentry) ;
    dput(dentry) ;

    return 0 ;
}

/* VFS TMPFILE */

static int vfs_tmpfile_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(UNISTD_64_ARG1(regs, struct dentry *)) ;
    if (dentry) {
        dget(dentry) ;
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_tmpfile_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    if (regs->ax) hash_table_bind_inode(dentry) ;
    dput(dentry) ;

    return 0 ;
}

/* VFS UNLINK */

static int vfs_unlink_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = dget(UNISTD_64_ARG1(regs, struct dentry *)) ;
    if (dentry) {
        dget(dentry) ;
        *((struct dentry **) ki->data) = dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_unlink_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ;

    if (!regs->ax) hash_table_unbind_inode(dentry) ;
    dput(dentry) ;

    return 0 ;
}

/* VFS RENAME */

static int vfs_rename_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct renamedata *rd = dget(UNISTD_64_ARG0(regs, struct renamedata *)) ;
    if (rd->old_dentry && rd->new_dentry) {
        dget(rd->old_dentry) ;
        dget(rd->new_dentry) ;
        *(((struct dentry **) ki->data))     = rd->old_dentry ;
        *(((struct dentry **) ki->data) +1 ) = rd->new_dentry ;
        return 0 ;
    } else return 1 ;
}

static int vfs_rename_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *old_dentry = *((struct dentry **) ki->data) ;
    struct dentry *new_dentry = *(((struct dentry **) ki->data) +1) ;

    if (!regs->ax) {
        hash_table_unbind_inode(old_dentry) ;
        hash_table_bind_inode(new_dentry) ;
    }
    dput(new_dentry) ;
    dput(old_dentry) ;

    return 0 ;
}