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
    return hash_table_insert_uid(uid) ;
}

ssize_t throttleA_uid_rm(unsigned long uid) {
    return hash_table_remove_uid(uid) ;
}

ssize_t throttleA_path_add(throttleA_path *path) {
    return insert_path_tree_entry(path->pathName) ;
}

ssize_t throttleA_path_rm(throttleA_path *path) {
    remove_path_tree_entry(path->pathName) ;
    return 0 ;
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
    reset_max_value(max) ;
    return 0 ;
}

ssize_t set_throttler_on() {
    set_throttler_status_on() ;
    return 0 ;
}

ssize_t set_throttler_off() {
    set_throttler_status_off() ;
    return 0 ;
}
