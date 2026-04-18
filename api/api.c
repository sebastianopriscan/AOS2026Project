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

#define CODE_MASK 0xe0000000U
#define CHECK_PATH 0x40000000

static struct kmem_cache *policies_cache ;

static void setup_area(void *buffer) {

}

static int dev_open(struct inode *inode, struct file *file) {
    return 0 ;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0 ;
}

static ssize_t dev_ioctl(struct file *filp, unsigned int code, unsigned long argp) {

    unsigned int size ;
    unsigned int copied ;
    ssize_t retval ;
    throttleA_policy *argp_copied  ;

    printk("%s code is %#08x, code & CODE_MASK is %#08x", MODNAME, code, code & CODE_MASK) ;

    // Handlers for when argp is not needed
    if (code & CODE_MASK == THROTTLER_SET_ENABLE) {
        return set_throttler_on() ;
    } else if (code & CODE_MASK == THROTTLER_SET_DISABLE) {
        return set_throttler_off() ;
    }

    argp_copied = kmem_cache_alloc(policies_cache, GFP_KERNEL) ; 
 
    if (argp_copied == NULL) {
        printk("%s: Error allocating buffer for copying", MODNAME) ;
        return -ENOMEM ;
    }

    size = code & ~CODE_MASK ;
    if(size != sizeof(throttleA_policy)) { 
        printk("%s: Data pointed by argp was not of correct size", MODNAME) ; 
        kmem_cache_free(policies_cache, argp_copied) ;
        return 1 ; 
    } 

    copied = copy_from_user(argp_copied, (void *) argp, size) ;
    if (copied != 0) {
        printk("%s: Error, unable to copy all memory from user, copied %d of %d", MODNAME, copied, size) ;
        kmem_cache_free(policies_cache, argp_copied) ;
        return -EACCES ;
    }

    printk("%s: Copied data from user buffer", MODNAME) ;

    switch (code & CODE_MASK) {
        case ADD_POLICY :
            retval = throttleA_policy_add(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        case RM_POLICY :
            retval = throttleA_policy_rm(argp_copied) ;
            kmem_cache_free(policies_cache, argp_copied) ;
            return retval ;
        default :
            kmem_cache_free(policies_cache, argp_copied) ;
            printk("%s: Invoked non-existant operation", MODNAME) ;
            return -EOPNOTSUPP ;
    }
}

unsigned int major ;
module_param(major, uint, 0400) ;

static struct file_operations fops = {
    //TODO IMPLEMENT READ
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl
} ;

int setup_api(void) {

    policies_cache = kmem_cache_create(
        MODNAME"_policies",
        8192,
        8192,
        SLAB_POISON,
        setup_area
    );

    if (policies_cache == NULL) {
        printk("%s: Unable to allocate kmem path cache", MODNAME) ;
        return 1 ;
    }

    major = __register_chrdev(0,0, 256, API_CHARDEV_NAME, &fops) ;

    return 0 ;
}

void cleanup_api(void) {
    unregister_chrdev(major, API_CHARDEV_NAME) ;
}