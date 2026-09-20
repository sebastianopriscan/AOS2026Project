#ifndef THROTTLEA_SYSCALL_H
#define THROTTLEA_SYSCALL_H

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include "include/api/api.h"


extern throttleA_syscall_map *syscalls ;

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

static inline int is_syscall_monitored(unsigned long syscall) {
    int entry = syscall / sizeof(throttleA_syscall_map_type) ; 
    int index = syscall % sizeof(throttleA_syscall_map_type) ;
    int result ;
    rcu_read_lock() ;

    result = syscalls->map[entry] & ((throttleA_syscall_map_type) 1UL << index) ;

    rcu_read_unlock() ;
    return result ;
}

#endif