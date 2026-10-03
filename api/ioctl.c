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
#include "include/hash_table/tree.h"
#include "include/throttler_status/throttler_status.h"
#include "include/oracles/oracles.h"
#include "include/syscalls/syscalls.h"

ssize_t throttleA_uid_add(unsigned long uid) {
    if (uid >= (uid_t) -1) return -EINVAL ;
    return hash_table_insert_uid(uid) ;
}

ssize_t throttleA_uid_rm(unsigned long uid) {
    if (uid >= (uid_t) -1) return -EINVAL ;
    return hash_table_remove_uid(uid) ;
}

/**
 * Runs op on the path the oracle makes out of the one received from user space
 */
static ssize_t path_op(throttleA_path *path, int (*op)(char *)) {
    ssize_t retval ;
    char *resolved = pathname_oracle(path->pathName) ;

    if (IS_ERR(resolved)) return PTR_ERR(resolved) ;
    retval = op(resolved) ;
    kfree(resolved) ;
    return retval ;
}

ssize_t throttleA_path_add(throttleA_path *path) {
    return path_op(path, insert_path_tree_entry) ;
}

ssize_t throttleA_path_rm(throttleA_path *path) {
    return path_op(path, remove_path_tree_entry) ;
}

ssize_t throttleA_syscalls_add(throttleA_syscall_map *map) {
    monitor_syscalls(map) ;
    return 0 ;
}

ssize_t throttleA_syscalls_rm(throttleA_syscall_map *map) {
    unmonitor_syscalls(map) ;
    return 0 ;
}

ssize_t throttleA_syscalls_dump(throttleA_syscall_map *map) {
    dump_syscalls(map) ;
    return 0 ;
}

ssize_t throttleA_stats_dump(struct stats_register *map) {
    dump_stats(map) ;
    return 0 ;
}

ssize_t throttleA_reset_max(unsigned long max) {
    if (max == 0) return -EINVAL ;
    if (max > LONG_MAX /2) return -ERANGE ;
    reset_max_value(max) ;
    return 0 ;
}

ssize_t set_throttler_on() {
    return set_throttler_status_on() ;
}

ssize_t set_throttler_off() {
    set_throttler_status_off() ;
    return 0 ;
}
