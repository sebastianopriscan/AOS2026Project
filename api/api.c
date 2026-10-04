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
#include <linux/fdtable.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>

#include "include/names/names.h"
#include "include/api/api.h"
#include "include/hash_table/hash_table.h"
#include "include/hash_table/tree.h"

#define CODE_MASK 0xf0000000U
#define CHECK_PATH 0x40000000

static struct kmem_cache *policies_cache ;

static inline bool euid_is_root(void) {
    return uid_eq(current_euid(), GLOBAL_ROOT_UID) ;
}

static int dev_open(struct inode *inode, struct file *file) {
    return euid_is_root() ? 0 : -EPERM ;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0 ;
}

static ssize_t dev_ioctl(struct file *filp, unsigned int code, unsigned long argp) {

    unsigned int size ;
    unsigned int copied ;
    ssize_t retval ;
    void *argp_copied ;

    if (!euid_is_root()) return -EPERM ;

    pr_debug("%s code is %#08x, code & CODE_MASK is %#08x\n", MODNAME, code, code & CODE_MASK) ;

    // Handlers for when argp is not needed
    if ((code & CODE_MASK) == THROTTLER_SET_ENABLE) {
        return set_throttler_on() ;
    } else if ((code & CODE_MASK) == THROTTLER_SET_DISABLE) {
        return set_throttler_off() ;
    } else if ((code & CODE_MASK) == ADD_UID) {
        return throttleA_uid_add(argp) ;
    } else if ((code & CODE_MASK) == RM_UID) {
        return throttleA_uid_rm(argp) ;
    } else if ((code & CODE_MASK) == RESET_MAX) {
        return throttleA_reset_max(argp) ;
    }

    size = code & ~CODE_MASK ;
    if (
        ((code & CODE_MASK) == ADD_PATH || (code & CODE_MASK) == RM_PATH) &&
        size != sizeof(throttleA_path)
    ) { 
        printk("%s: Data pointed by argp was not of correct size for path operations", MODNAME) ; 
        return -EINVAL ;
    } else if (
        ((code & CODE_MASK) == ADD_SYSCALLS || (code & CODE_MASK) == RM_SYSCALLS || (code & CODE_MASK) == DUMP_SYSCALLS) &&
        size != sizeof(throttleA_syscall_map)
    ) {
        printk("%s: Data pointed by argp was not of correct size for syscall operations", MODNAME) ; 
        return -EINVAL ;
    } else if (
        ((code & CODE_MASK) == DUMP_STATS) && size != sizeof(struct stats_register)
    ) {
        printk("%s: Data pointed by argp was not of correct size for stats dump operation", MODNAME) ; 
        return -EINVAL ;
    } else if (
        (code & CODE_MASK) != ADD_PATH && (code & CODE_MASK) != RM_PATH &&
        (code & CODE_MASK) != ADD_SYSCALLS && (code & CODE_MASK) != RM_SYSCALLS &&
        (code & CODE_MASK) != DUMP_SYSCALLS && (code & CODE_MASK) != DUMP_STATS
    ) {
        printk("%s: Invoked non-existant operation", MODNAME) ;
        return -EOPNOTSUPP ;
    }

    argp_copied = kmem_cache_alloc(policies_cache, GFP_KERNEL) ; 
    if (argp_copied == NULL) {
        printk("%s: Error allocating buffer for copying", MODNAME) ;
        return -ENOMEM ;
    }

    copied = copy_from_user(argp_copied, (void *) argp, size) ;
    if (copied != 0) {
        printk("%s: Error, unable to copy all memory from user, copied %d of %d", MODNAME, copied, size) ;
        kmem_cache_free(policies_cache, argp_copied) ;
        return -EFAULT ;
    }

    pr_debug("%s: Copied data from user buffer\n", MODNAME) ;

    switch (code & CODE_MASK) {
        case ADD_PATH :
            retval = throttleA_path_add(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case RM_PATH :
            retval = throttleA_path_rm(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case ADD_SYSCALLS :
            retval = throttleA_syscalls_add(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case RM_SYSCALLS :
            retval = throttleA_syscalls_rm(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case DUMP_SYSCALLS :
            retval = throttleA_syscalls_dump(argp_copied) ;
            if (retval == 0 && copy_to_user((void *) argp, argp_copied, size) != 0) retval = -EFAULT ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case DUMP_STATS :
            retval = throttleA_stats_dump(argp_copied) ;
            if (retval == 0 && copy_to_user((void *) argp, argp_copied, size) != 0) retval = -EFAULT ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        default :
            kmem_cache_free(policies_cache, argp_copied) ;
            printk("%s: Invoked non-existant operation", MODNAME) ;
            return -EOPNOTSUPP ;
    }
}

static int dump_open(struct inode *inode, struct file *file) {
    unsigned int minor = MINOR(inode->i_rdev) ;
    if (!euid_is_root()) return -EPERM ;
    if (minor == 0) {
        if (hash_table_lock()) return -EBUSY ;
        reset_ht_file_handle() ;
        return 0 ;
    }
    if (minor == 1) {
        if(path_tree_lock()) return  -EBUSY ;
        reset_pt_file_handle() ;
        return 0 ;
    }
    else return -ENXIO ;
}

static int dump_release(struct inode *inode, struct file *file) {
    unsigned int minor = MINOR(inode->i_rdev) ;
    if (minor == 0) { 
        hash_table_unlock() ;
        return 0 ;
    } else {
        path_tree_unlock() ;
        return 0 ;
    }
}

static ssize_t dump_read(struct file *file, char __user *data, size_t bufLen, loff_t *offset) {
    unsigned int minor = MINOR(file->f_inode->i_rdev) ;
    if (minor == 0) { 
        return ht_file_handle_read(data, bufLen) ;
    } else {
        return pt_file_handle_read(data, bufLen) ;
    }
}

static loff_t dump_llseek(struct file *, loff_t, int) {
    return -ESPIPE ;
}

unsigned int ioctl_major ;
module_param(ioctl_major, uint, 0400) ;

unsigned int dump_major ;
module_param(dump_major, uint, 0400) ;

static struct file_operations ioctl_fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl
} ;

static struct file_operations dump_fops = {
    .owner = THIS_MODULE,
    .open = dump_open,
    .release = dump_release,
    .read = dump_read,
    .llseek = dump_llseek
} ;

int setup_api(void) {
    int ret ;

    // The whole object is copied to/from user space, so whitelist all of it for hardened usercopy
    policies_cache = kmem_cache_create_usercopy(
        MODNAME"_policies",
        3 * PAGE_SIZE,
        3 * PAGE_SIZE,
        SLAB_POISON,
        0,
        3 * PAGE_SIZE,
        NULL
    );

    if (policies_cache == NULL) {
        printk("%s: Unable to allocate kmem path cache", MODNAME) ;
        return -ENOMEM ;
    }

    ret = __register_chrdev(0,0, 256, API_CHARDEV_IOCTL_NAME, &ioctl_fops) ;
    if (ret < 0) {
        kmem_cache_destroy(policies_cache) ;
        return ret ;
    }
    ioctl_major = ret ;

    ret = __register_chrdev(0,0, 256, API_CHARDEV_DUMP_NAME, &dump_fops) ;
    if (ret < 0) {
        unregister_chrdev(ioctl_major, API_CHARDEV_IOCTL_NAME) ;
        kmem_cache_destroy(policies_cache) ;
        return ret ;
    }
    dump_major = ret ;

    return 0 ;
}

void cleanup_api(void) {
    unregister_chrdev(ioctl_major, API_CHARDEV_IOCTL_NAME) ;
    unregister_chrdev(dump_major, API_CHARDEV_DUMP_NAME) ;
    kmem_cache_destroy(policies_cache) ;
}