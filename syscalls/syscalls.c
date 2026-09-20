#include <linux/mutex.h>

#include "include/syscalls/syscalls.h"

static struct mutex syscalls_mutex ;
static unsigned int idx = 0 ;

throttleA_syscall_map maps[2] ;
throttleA_syscall_map *syscalls = maps ;

void monitor_syscalls(throttleA_syscall_map *newSyscalls) {
    int i ;
    throttleA_syscall_map *new, *curr ;

    mutex_lock(&syscalls_mutex) ;

    curr = &maps[idx] ;
    idx = (idx +1) %2 ;
    new = &maps[idx] ;

    memcpy(new, curr, sizeof(throttleA_syscall_map)) ;

    for (i = 0 ; i < DATA_PER_LIMIT(throttleA_syscall_map_type); i++) {
        new->map[i] |= newSyscalls->map[i] ;
    }

    syscalls = &maps[idx] ;
    synchronize_rcu() ;
    mutex_unlock(&syscalls_mutex) ;
}


void unmonitor_syscalls(throttleA_syscall_map *newSyscalls) {
    int i ;
    throttleA_syscall_map *new, *curr ;

    mutex_lock(&syscalls_mutex) ;

    curr = &maps[idx] ;
    idx = (idx +1) %2 ;
    new = &maps[idx] ;

    memcpy(new, curr, sizeof(throttleA_syscall_map)) ;

    for (i = 0 ; i < DATA_PER_LIMIT(throttleA_syscall_map_type); i++) {
        new->map[i] &= ~(newSyscalls->map[i]) ;
    }

    syscalls = &maps[idx] ;
    synchronize_rcu() ;
    mutex_unlock(&syscalls_mutex) ;
}

void init_syscall_monitor(void) {
    int i ;

    mutex_init(&syscalls_mutex) ;

    for (i = 0; i < DATA_PER_LIMIT(throttleA_syscall_map_type); i++) {
        maps[0].map[i] = (throttleA_syscall_map_type) 0 ;
        maps[1].map[i] = (throttleA_syscall_map_type) 0 ;
    }

    return ;
}