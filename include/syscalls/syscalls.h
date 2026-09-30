#ifndef THROTTLEA_SYSCALL_H
#define THROTTLEA_SYSCALL_H

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include "include/api/api.h"


extern atomic_long_t syscalls ;

/**
 * Add the syscalls contained in the map
 * to the monitor
 * 
 * @param newSyscalls: Syscall map of the syscalls to add
 */
void monitor_syscalls(throttleA_syscall_map *newSyscalls) ;

/**
 * Remove the syscalls contained in the map
 * from the monitor
 * 
 * @param newSyscalls: Syscall map of the syscalls to remove
 */
void unmonitor_syscalls(throttleA_syscall_map *newSyscalls) ;

/**
 * Dump the currently managed syscalls
 * 
 * @param map: Area in which the status wil be dumped 
 */
void dump_syscalls(throttleA_syscall_map *map) ;

/**
 * Initialize the syscall monitor with an empty map
 */
void init_syscall_monitor(void) ;

#define SYSCALL_MAP_ENTRY_BITS (sizeof(throttleA_syscall_map_type) * 8)

static inline int is_syscall_monitored(unsigned long syscall) {
    unsigned long entry = syscall / SYSCALL_MAP_ENTRY_BITS ;
    unsigned long index = syscall % SYSCALL_MAP_ENTRY_BITS ;
    int result ;

    if (syscall >= SYSCALL_LIMIT) return 0 ;

    rcu_read_lock() ;

    result = (((throttleA_syscall_map *) atomic_long_read(&syscalls))->map[entry] >> index) & 1UL ;

    rcu_read_unlock() ;
    return result ;
}

#endif