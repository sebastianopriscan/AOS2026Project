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
#include "include/preempt_kprobe/preempt_kprobe.h"

#define UNISTD_64_ARG0(regs, cast) ((cast) regs->di)
#define UNISTD_64_ARG1(regs, cast) ((cast) regs->si)
#define UNISTD_64_ARG2(regs, cast) ((cast) regs->dx)
#define UNISTD_64_ARG3(regs, cast) ((cast) regs->cx)
#define UNISTD_64_ARG4(regs, cast) ((cast) regs->r8)
#define UNISTD_64_ARG5(regs, cast) ((cast) regs->r9)

#define UNISTD_64_RETVAL(regs, cast) ((cast) regs->ax)

static struct kretprobe vfs_create_probe, vfs_tmpfile_probe, vfs_mknod_probe, vfs_mkdir_probe, vfs_rmdir_probe, vfs_unlink_probe, vfs_symlink_probe, vfs_link_probe, vfs_rename_probe ;

struct unlink_metadata {
    struct dentry *d ;
    struct inode_descriptor desc ;
} ;

struct rename_metadata {
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
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
            put_path_tree_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
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
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
            put_path_tree_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
    }

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
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
            put_path_tree_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
    }
    dput(dentry) ;

    return 0 ;
}

/* VFS MKDIR */

static int vfs_mkdir_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG2(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_mkdir_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 

    if (!regs->ax) {
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        put_path_tree_entry(entry) ;
        SET_KPROBE_CONTEXT() ;
    }
    dput(dentry) ;

    return 0 ;
}

/* VFS RMDIR */

static int vfs_rmdir_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG2(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_rmdir_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 

    if (!regs->ax) {
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = get_path_tree_entry_by_dentry(dentry) ;
        if (entry) {
            dematerialize_entry(entry) ;
            put_path_tree_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
    }
    dput(dentry) ;

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
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = get_path_tree_entry_by_dentry(payload->d) ;
        if (entry) {
            hash_table_unbind_inode(&entry->pts, &payload->desc) ;
            dematerialize_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
    }

    return 0 ;
}

/* VFS SYMLINK */

static int vfs_symlink_pre_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = UNISTD_64_ARG3(regs, struct dentry *) ;
    if (dentry) {
        *((struct dentry **) ki->data) = dentry ; 
        dget(dentry) ;
        return 0 ;
    } else return 1 ;
}

static int vfs_symlink_ret_hook(struct kretprobe_instance *ki, struct pt_regs *regs) {
    struct dentry *dentry = *((struct dentry **) ki->data) ; 

    if (!regs->ax) {
        RESET_KPROBE_CONTEXT() ;
        // The module doesn't support path name resolution, it identifies programs only by hard links
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        put_path_tree_entry(entry) ;
        SET_KPROBE_CONTEXT() ;
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
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = materialize_child(dentry->d_parent, dentry) ;
        if (entry) {
            hash_table_bind_inode(&entry->pts, &desc) ;
            put_path_tree_entry(entry) ;
        }
        SET_KPROBE_CONTEXT() ;
    }
    dput(dentry) ;

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
    struct rename_metadata *payload = (struct rename_metadata *) ki->data ;

    if (!regs->ax) {
        RESET_KPROBE_CONTEXT() ;
        path_tree_entry *entry = get_path_tree_entry_by_dentry(payload->unlink.d) ;
        if (entry) {
            // Unbind old node
            hash_table_unbind_inode(&entry->pts, &payload->unlink.desc) ;
            dematerialize_entry(entry) ;
            put_path_tree_entry(entry) ;

            // Bind new node 
            entry = materialize_child(payload->dentry->d_parent, payload->dentry) ;
            
            if (entry) {
                struct inode *inode = d_inode(payload->dentry) ;
                struct inode_descriptor desc = {
                    .device_id = inode->i_rdev ,
                    .inode_number = inode->i_ino
                } ;
                hash_table_bind_inode(&entry->pts, &desc) ;
                put_path_tree_entry(entry) ;
            }
        }
        SET_KPROBE_CONTEXT() ;
    }

    return 0 ;
}

int init_observers(void) {
    int retval ;
    
    vfs_create_probe.kp.symbol_name = "vfs_create" ;
    vfs_create_probe.entry_handler = vfs_create_pre_hook ;
    vfs_create_probe.handler = vfs_create_ret_hook ;
    vfs_create_probe.data_size = sizeof(struct dentry *) ;
    vfs_create_probe.maxactive = -1 ;

    vfs_tmpfile_probe.kp.symbol_name = "vfs_tmpfile" ;
    vfs_tmpfile_probe.entry_handler = vfs_tmpfile_pre_hook ;
    vfs_tmpfile_probe.handler = vfs_tmpfile_ret_hook ;
    vfs_tmpfile_probe.data_size = 0 ;
    vfs_tmpfile_probe.maxactive = -1 ;

    vfs_mknod_probe.kp.symbol_name = "vfs_mknod" ;
    vfs_mknod_probe.entry_handler = vfs_mknod_pre_hook ;
    vfs_mknod_probe.handler = vfs_mknod_ret_hook ;
    vfs_mknod_probe.data_size = sizeof(struct dentry *) ;
    vfs_mknod_probe.maxactive = -1 ;

    vfs_mkdir_probe.kp.symbol_name = "vfs_mkdir" ;
    vfs_mkdir_probe.entry_handler = vfs_mkdir_pre_hook ;
    vfs_mkdir_probe.handler = vfs_mkdir_ret_hook ;
    vfs_mkdir_probe.data_size = sizeof(struct dentry *) ;
    vfs_mkdir_probe.maxactive = -1 ;

    vfs_rmdir_probe.kp.symbol_name = "vfs_rmdir" ;
    vfs_rmdir_probe.entry_handler = vfs_rmdir_pre_hook ;
    vfs_rmdir_probe.handler = vfs_rmdir_ret_hook ;
    vfs_rmdir_probe.data_size = sizeof(struct dentry *) ;
    vfs_rmdir_probe.maxactive = -1 ;

    vfs_unlink_probe.kp.symbol_name = "vfs_unlink" ;
    vfs_unlink_probe.entry_handler = vfs_unlink_pre_hook ;
    vfs_unlink_probe.handler = vfs_unlink_ret_hook ;
    vfs_unlink_probe.data_size = sizeof(struct unlink_metadata) ;
    vfs_unlink_probe.maxactive = -1 ;

    vfs_symlink_probe.kp.symbol_name = "vfs_symlink" ;
    vfs_symlink_probe.entry_handler = vfs_symlink_pre_hook ;
    vfs_symlink_probe.handler = vfs_symlink_ret_hook ;
    vfs_symlink_probe.data_size = sizeof(struct dentry *) ;
    vfs_symlink_probe.maxactive = -1 ;

    vfs_link_probe.kp.symbol_name = "vfs_link" ;
    vfs_link_probe.entry_handler = vfs_link_pre_hook ;
    vfs_link_probe.handler = vfs_link_ret_hook ;
    vfs_link_probe.data_size = sizeof(struct dentry *) ;
    vfs_link_probe.maxactive = -1 ;

    vfs_rename_probe.kp.symbol_name = "vfs_rename" ;
    vfs_rename_probe.entry_handler = vfs_rename_pre_hook ;
    vfs_rename_probe.handler = vfs_rename_ret_hook ;
    vfs_rename_probe.data_size = sizeof(struct rename_metadata) ;
    vfs_rename_probe.maxactive = -1 ;

    retval = register_kretprobe(&vfs_create_probe) ;
    if (!retval) {
        retval = register_kretprobe(&vfs_tmpfile_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_mknod_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_mkdir_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_rmdir_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            unregister_kretprobe(&vfs_mkdir_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_unlink_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            unregister_kretprobe(&vfs_mkdir_probe) ;
            unregister_kretprobe(&vfs_rmdir_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_symlink_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            unregister_kretprobe(&vfs_mkdir_probe) ;
            unregister_kretprobe(&vfs_rmdir_probe) ;
            unregister_kretprobe(&vfs_unlink_probe) ;
            return retval ;
        }

        retval = register_kretprobe(&vfs_link_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            unregister_kretprobe(&vfs_mkdir_probe) ;
            unregister_kretprobe(&vfs_rmdir_probe) ;
            unregister_kretprobe(&vfs_unlink_probe) ;
            unregister_kretprobe(&vfs_symlink_probe) ;
            return retval ;
        }
        retval = register_kretprobe(&vfs_rename_probe) ;
        if (retval) {
            unregister_kretprobe(&vfs_create_probe) ;
            unregister_kretprobe(&vfs_tmpfile_probe) ;
            unregister_kretprobe(&vfs_mknod_probe) ;
            unregister_kretprobe(&vfs_mkdir_probe) ;
            unregister_kretprobe(&vfs_rmdir_probe) ;
            unregister_kretprobe(&vfs_unlink_probe) ;
            unregister_kretprobe(&vfs_symlink_probe) ;
            unregister_kretprobe(&vfs_link_probe) ;
            return retval ;
        }
    }

    return retval ;
}

void cleanup_observers(void) {
    unregister_kretprobe(&vfs_create_probe) ;
    unregister_kretprobe(&vfs_tmpfile_probe) ;
    unregister_kretprobe(&vfs_mknod_probe) ;
    unregister_kretprobe(&vfs_mkdir_probe) ;
    unregister_kretprobe(&vfs_rmdir_probe) ;
    unregister_kretprobe(&vfs_unlink_probe) ;
    unregister_kretprobe(&vfs_symlink_probe) ;
    unregister_kretprobe(&vfs_link_probe) ;
    unregister_kretprobe(&vfs_rename_probe) ;
}