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

#include "include/api/api.h"
#include "include/hash_table/hash_table.h"
#include "include/throttler_status/throttler_status.h"
#include "include/oracles/oracles.h"

ssize_t throttleA_policy_add(throttleA_policy *policy) {
    char *fullPath = pathname_oracle(policy->path.pathName) ;
    ssize_t ret ;
    if (IS_ERR_OR_NULL(fullPath)) {
        return (ssize_t) PTR_ERR(fullPath) ;
    }
    ret = (ssize_t) hash_table_insert(policy, fullPath) ;
    kfree(fullPath) ;
    return ret ;
}

ssize_t throttleA_policy_rm(throttleA_policy *policy) {
    char *fullPath = pathname_oracle(policy->path.pathName) ;
    ssize_t ret ;
    if (IS_ERR_OR_NULL(fullPath)) {
        return (ssize_t) PTR_ERR(fullPath) ;
    }
    ret = (ssize_t) hash_table_remove(policy, fullPath) ;
    kfree(fullPath) ;
    return ret ;
}

ssize_t throttleA_policy_delete(throttleA_policy *policy) {
    char *fullPath = pathname_oracle(policy->path.pathName) ;
    ssize_t ret ;
    if (IS_ERR_OR_NULL(fullPath)) {
        return (ssize_t) PTR_ERR(fullPath) ;
    }
    ret = (ssize_t) hash_table_delete(policy, fullPath) ;
    kfree(fullPath) ;
    return ret ;
}

ssize_t set_throttler_on() {
    set_throttler_status_on() ;
    return 0 ;
}

ssize_t set_throttler_off() {
    set_throttler_status_off() ;
    return 0 ;
}

throttleA_policy *dump_throttleA_status() {
    return NULL ;
}
