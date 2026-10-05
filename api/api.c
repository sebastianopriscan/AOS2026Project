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
#include <linux/hrtimer.h>
#include <linux/spinlock.h>

#include "include/names/names.h"
#include "include/api/api.h"
#include "include/hash_table/hash_table.h"
#include "include/hash_table/tree.h"

#define CODE_MASK 0xf0000000U
#define CHECK_PATH 0x40000000

// How long a dump lock may stay held without the reader showing up
#define DUMP_LOCK_TIMEOUT_SEC 10

// Room for the largest argument copied from/to user space
#define POLICIES_BUF_SIZE max3(sizeof(throttleA_path), sizeof(throttleA_syscall_map), sizeof(struct stats_register))

static struct kmem_cache *policies_cache ;

static inline bool euid_is_root(void) {
    return uid_eq(current_euid(), GLOBAL_ROOT_UID) ;
}

/**
 * Lock held by an open dump file. A timer frees it if no read shows up for
 * DUMP_LOCK_TIMEOUT_SEC, and the next read of that file tries to take it back
 */
struct dump_lock {
    struct hrtimer timer ;
    spinlock_t slock ;      // Protects the fields below and serializes the store lock/unlock

    int (*trylock)(void) ;  // Returns 0 if the store lock is acquired
    void (*unlock)(void) ;
    unsigned long (*generation)(void) ;
    void (*reset_handle)(void) ;

    unsigned long epoch ;   // Identifies the last open, a file keeps the one it got at open
    bool held ;             // The store lock is held on behalf of epoch
    bool reading ;          // A read of epoch is running
    bool truncated ;        // The lock was lost for good, reads of epoch return 0
    unsigned long gen ;     // Store generation when the lock was freed by the timer
    ktime_t deadline ;      // The timer frees the lock if still idle by then
} ;

static struct dump_lock dump_locks[2] = {
    {
        .trylock = hash_table_lock,
        .unlock = hash_table_unlock,
        .generation = hash_table_generation,
        .reset_handle = reset_ht_file_handle
    },
    {
        .trylock = path_tree_lock,
        .unlock = path_tree_unlock,
        .generation = path_tree_generation,
        .reset_handle = reset_pt_file_handle
    }
} ;

static inline ktime_t dump_lock_deadline(void) {
    return ktime_add(ktime_get(), ktime_set(DUMP_LOCK_TIMEOUT_SEC, 0)) ;
}

static enum hrtimer_restart dump_lock_expire(struct hrtimer *timer) {
    struct dump_lock *dl = container_of(timer, struct dump_lock, timer) ;
    enum hrtimer_restart restart = HRTIMER_NORESTART ;
    unsigned long flags ;

    spin_lock_irqsave(&dl->slock, flags) ;
    if (dl->held) {
        // A running read is activity, and reads push the deadline forward
        if (dl->reading) dl->deadline = dump_lock_deadline() ;
        if (ktime_before(ktime_get(), dl->deadline)) {
            hrtimer_set_expires(timer, dl->deadline) ;
            restart = HRTIMER_RESTART ;
        } else {
            dl->gen = dl->generation() ;
            dl->unlock() ;
            dl->held = false ;
            pr_debug("%s: Dump lock freed after %d idle seconds\n", MODNAME, DUMP_LOCK_TIMEOUT_SEC) ;
        }
    }
    spin_unlock_irqrestore(&dl->slock, flags) ;

    return restart ;
}

static int dump_lock_open(struct dump_lock *dl, struct file *file) {
    unsigned long flags ;

    spin_lock_irqsave(&dl->slock, flags) ;
    if (dl->trylock()) {
        spin_unlock_irqrestore(&dl->slock, flags) ;
        return -EBUSY ;
    }

    dl->reset_handle() ;
    dl->held = true ;
    dl->reading = false ;
    dl->truncated = false ;
    file->private_data = (void *) ++dl->epoch ;
    dl->deadline = dump_lock_deadline() ;
    hrtimer_start(&dl->timer, ktime_set(DUMP_LOCK_TIMEOUT_SEC, 0), HRTIMER_MODE_REL) ;
    spin_unlock_irqrestore(&dl->slock, flags) ;

    return 0 ;
}

static void dump_lock_release(struct dump_lock *dl, struct file *file) {
    unsigned long flags ;

    spin_lock_irqsave(&dl->slock, flags) ;
    // If the epoch moved on, a newer open owns the lock and it is not ours to free
    if (dl->epoch == (unsigned long) file->private_data && dl->held) {
        dl->unlock() ;
        dl->held = false ;
    }
    spin_unlock_irqrestore(&dl->slock, flags) ;
}

/**
 * @return true if the file holds the lock and may read, false if the read has to return 0
 */
static bool dump_lock_read_begin(struct dump_lock *dl, struct file *file) {
    bool ok = false ;
    unsigned long flags ;

    spin_lock_irqsave(&dl->slock, flags) ;
    if (dl->epoch != (unsigned long) file->private_data || dl->truncated) goto out ;

    if (!dl->held) {
        // The timer freed the lock: take it back, but only if the store was not touched meanwhile,
        // otherwise the read handle may point to entries that are gone
        if (dl->trylock()) {
            dl->truncated = true ;
            goto out ;
        }
        if (dl->generation() != dl->gen) {
            dl->unlock() ;
            dl->truncated = true ;
            goto out ;
        }
        dl->held = true ;
        dl->deadline = dump_lock_deadline() ;
        hrtimer_start(&dl->timer, ktime_set(DUMP_LOCK_TIMEOUT_SEC, 0), HRTIMER_MODE_REL) ;
    }

    dl->reading = true ;
    ok = true ;
out:
    spin_unlock_irqrestore(&dl->slock, flags) ;
    return ok ;
}

static void dump_lock_read_end(struct dump_lock *dl) {
    unsigned long flags ;

    spin_lock_irqsave(&dl->slock, flags) ;
    dl->reading = false ;
    dl->deadline = dump_lock_deadline() ;
    spin_unlock_irqrestore(&dl->slock, flags) ;
}

static int dev_open(struct inode *inode, struct file *file) {
    return euid_is_root() ? 0 : -EPERM ;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0 ;
}

static ssize_t dev_ioctl(struct file *filp, unsigned int code, unsigned long argp) {

    unsigned int size ;
    unsigned int missing ;
    ssize_t retval ;
    void *argp_copied ;

    if (!euid_is_root()) return -EPERM ;

    pr_debug("%s code is %#08x, code & CODE_MASK is %#08x\n", MODNAME, code, code & CODE_MASK) ;

    // Handlers for when argp is not needed
    if ((code & CODE_MASK) == THROTTLER_SET_ENABLE) {
        return set_throttler_on() ;
    } else if ((code & CODE_MASK) == THROTTLER_SET_DISABLE) {
        return set_throttler_off() ;
    } else if ((code & CODE_MASK) == DUMP_STATUS) {
        return throttleA_status_dump() ;
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

    missing = copy_from_user(argp_copied, (void *) argp, size) ;
    if (missing != 0) {
        printk("%s: Error, unable to copy all memory from user, copied %u of %u", MODNAME, size - missing, size) ;
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
    if (!euid_is_root()) return -EPERM ; // BUG_REPORT R1
    if (minor >= ARRAY_SIZE(dump_locks)) return -ENXIO ; // BUG_REPORT A4
    return dump_lock_open(&dump_locks[minor], file) ;
}

static int dump_release(struct inode *inode, struct file *file) {
    dump_lock_release(&dump_locks[MINOR(inode->i_rdev)], file) ;
    return 0 ;
}

static ssize_t dump_read(struct file *file, char __user *data, size_t bufLen, loff_t *offset) {
    unsigned int minor = MINOR(file->f_inode->i_rdev) ;
    struct dump_lock *dl = &dump_locks[minor] ;
    ssize_t retval ;

    if (!dump_lock_read_begin(dl, file)) return 0 ;
    if (minor == 0) {
        retval = ht_file_handle_read(data, bufLen) ;
    } else {
        retval = pt_file_handle_read(data, bufLen) ;
    }
    dump_lock_read_end(dl) ;
    return retval ;
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

    for (int i = 0; i < ARRAY_SIZE(dump_locks); i++) {
        spin_lock_init(&dump_locks[i].slock) ;
        hrtimer_init(&dump_locks[i].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL) ;
        dump_locks[i].timer.function = dump_lock_expire ;
    }

    // The whole object is copied to/from user space, so whitelist all of it for hardened usercopy
    policies_cache = kmem_cache_create_usercopy(
        MODNAME"_policies",
        POLICIES_BUF_SIZE,
        0,
        0,
        0,
        POLICIES_BUF_SIZE,
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
    for (int i = 0; i < ARRAY_SIZE(dump_locks); i++) hrtimer_cancel(&dump_locks[i].timer) ;
    kmem_cache_destroy(policies_cache) ;
}