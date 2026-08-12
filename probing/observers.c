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
#include "include/hash_table/tree.h"

#define UNISTD_64_ARG0(regs, cast) ((cast) regs->di)
#define UNISTD_64_ARG1(regs, cast) ((cast) regs->si)
#define UNISTD_64_ARG2(regs, cast) ((cast) regs->dx)
#define UNISTD_64_ARG3(regs, cast) ((cast) regs->cx)
#define UNISTD_64_ARG4(regs, cast) ((cast) regs->r8)
#define UNISTD_64_ARG5(regs, cast) ((cast) regs->r9)

#define UNISTD_64_RETVAL(regs, cast) ((cast) regs->ax)

static struct kretprobe create_probe ;

static struct unlink_metadata {
    struct dentry *d ;
    struct inode_descriptor desc ;
} ;

static struct rename_metadata {
    struct unlink_metadata unlink ;
    struct dentry *dentry ;
} ;

/* VFS CREATE */

static int vfs_create_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG2(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_create_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 
    struct inode *inode = d_inode(dentry) ;
    struct inode_descriptor desc = {
        .device_id = inode->i_rdev,
        .inode_number = inode->i_ino 
    } ;

    if (!regs->ax) {
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
        }
    }
    dput(dentry) ;

    return 0 ;
}

/* VFS LINK */

static int vfs_link_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG3(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_link_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 
    struct inode *inode = d_inode(dentry) ;
    struct inode_descriptor desc = {
        .device_id = inode->i_rdev,
        .inode_number = inode->i_ino 
    } ;

    if (!regs->ax) {
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
        }
    }
    dput(dentry) ;

    return 0 ;
}

/* VFS MKNOD */

static int vfs_mknod_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG2(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_mknod_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 
    struct inode *inode = d_inode(dentry) ;
    struct inode_descriptor desc = {
        .device_id = inode->i_rdev,
        .inode_number = inode->i_ino 
    } ;

    if (!regs->ax) {
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
        }
    }
    dput(dentry) ;

    return 0 ;
}

/* VFS TMPFILE */

static int vfs_tmpfile_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    return 0 ;
}

static int vfs_tmpfile_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_RETVAL(regs, struct dentry *) ;

    struct inode *inode = d_inode(dentry) ;
    struct inode_descriptor desc = {
        .device_id = inode->i_rdev,
        .inode_number = inode->i_ino 
    } ;

    if (dentry) {
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
        }
    }

    return 0 ;
}

/* VFS UNLINK */

static int vfs_unlink_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG2(regs, struct dentry *) ;
    struct inode *inode = d_inode(dentry) ;

    if (dentry) {
        struct unlink_metadata *payload = (struct unlink_metadata *) ki->data ;
        payload->d = dentry ;
        payload->desc.device_id = inode->i_rdev ;
        payload->desc.inode_number = inode->i_ino ;
        return 0 ;
    } else return 1 ;
}

static int vfs_unlink_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct unlink_metadata *payload = (struct unlink_metadata *) ki->data ;

    if (!regs->ax) {
        path_tree_entry *entry = get_path_tree_entry_by_dentry(payload->d) ;
        if (entry) {
            hash_table_unbind_inode(&entry->pts, &payload->desc) ;
            dematerialize_entry(entry) ;
        }
    }

    return 0 ;
}

/* VFS RENAME */

static int vfs_rename_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct renamedata *rd = UNISTD_64_ARG0(regs, struct renamedata *) ;
    if (rd->old_dentry && rd->new_dentry) {
        struct inode *inode ;
        struct rename_metadata *payload = (struct rename_metadata *) ki->data ;
        dget(rd->old_dentry) ;
        dget(rd->new_dentry) ;
        inode = d_inode(rd->old_dentry) ;

        payload->dentry = rd->new_dentry ;
        payload->unlink.d = rd->old_dentry ;
        payload->unlink.desc.device_id = inode->i_rdev ;
        payload->unlink.desc.inode_number = inode->i_ino ;

        return 0 ;
    } else return 1 ;
}

static int vfs_rename_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct rename_metadata *payload = (struct unlink_metadata *) ki->data ;

    if (!regs->ax) {
        path_tree_entry *entry = get_path_tree_entry_by_dentry(payload->unlink.d) ;
        if (entry) {
            // Unbind old node
            hash_table_unbind_inode(&entry->pts, &payload->unlink.desc) ;
            dematerialize_entry(entry) ;

            // Bind new node 
            path_tree_entry *entry = materialize_child(payload->dentry->d_parent, payload->dentry) ;
            
            if (entry) {
                struct inode *inode = d_inode(payload->dentry) ;
                struct inode_descriptor desc = {
                    .device_id = inode->i_rdev ,
                    .inode_number = inode->i_ino
                } ;
                hash_table_bind_inode(&entry->pts, &desc) ;
            }
        }
    }

    return 0 ;
}