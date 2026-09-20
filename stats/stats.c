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

#include "include/throttler_status/throttler_status.h"

atomic_long_t MAX ;

static struct stats_register {
    atomic_long_t tolerance;
    spinlock_t reg_lock ;

    unsigned long peak_blocked ;
    unsigned long sum_blocked ;
    unsigned long num_blocked ;

    unsigned long peak_delay ;
    unsigned long peak_uid ;
    unsigned char peak_name[2*PAGE_SIZE] ;
} ;

static struct stats_register stats[2] = {
    {
        .peak_blocked = 0UL,
        .sum_blocked = 0UL,
        .num_blocked = 0UL,

        .peak_delay = 0,
        .peak_uid = 0,
        .peak_name = { '\0' },
    },
    {
        .peak_blocked = 0UL,
        .sum_blocked = 0UL,
        .num_blocked = 0UL,

        .peak_delay = 0,
        .peak_uid = 0,
        .peak_name = { '\0' },
    },
} ;

atomic_long_t stat_ptr ;
unsigned long current_index = 0 ;

spinlock_t max_lock ;

void init_stats(void) {
    spin_lock_init(&max_lock) ;
    spin_lock_init(&stats[0].reg_lock) ;
    spin_lock_init(&stats[1].reg_lock) ;

    atomic_long_set(&MAX, ULONG_MAX) ;
    atomic_long_set(&stat_ptr, stats) ;
    atomic_long_set(&stats[0].tolerance, 0) ;
    atomic_long_set(&stats[1].tolerance, 0) ;

    return ;
}

void reset_max_value(unsigned long max) {
    struct stats_register *oldStats ;
    spin_lock(&max_lock) ;

    oldStats = &stats[current_index] ;
    current_index = (current_index +1) %2 ;
    atomic_long_xchg(&MAX, max) ;
    atomic_long_xchg(&stat_ptr, &stats[current_index]) ;

    atomic_long_set(&oldStats->tolerance, 0) ;

    oldStats->peak_blocked = 0UL,
    oldStats->sum_blocked = 0UL,
    oldStats->num_blocked = 0UL,

    oldStats->peak_delay = 0,
    oldStats->peak_uid = 0,
    oldStats->peak_name[0] = '\0',
    
    spin_unlock(&max_lock) ;
}

void register_delay(unsigned long delay, uid_t uid, char *progName) {
    struct stats_register *reg = (struct stats_register *) atomic_long_read(&stat_ptr) ;

    spin_lock(&reg->reg_lock) ;

    if (delay > reg->peak_delay) {
        reg->peak_delay = delay ;
        reg->peak_uid = (unsigned long) uid ;
        strncpy(reg->peak_name, progName, 2*PAGE_SIZE) ;
    } 

    spin_unlock(&reg->reg_lock) ;
    return ;
}

void update_tolerance(void) {
    struct stats_register *reg = (struct stats_register *) atomic_long_read(&stat_ptr) ;
    unsigned long tolerance = atomic_long_xchg(&reg->tolerance, 0) ;

    spin_lock(&reg->reg_lock) ;


    if (tolerance > reg->peak_blocked) reg->peak_blocked = tolerance ;
    if (unlikely(ULONG_MAX - reg->sum_blocked < tolerance || reg->num_blocked == (ULONG_MAX -1))) {
        reg->sum_blocked = reg->sum_blocked / reg->num_blocked + tolerance ;
        reg->num_blocked = 1 ;
    } else {
        reg->sum_blocked += tolerance ;
        reg->num_blocked++ ;
    }

    spin_unlock(&reg->reg_lock) ;
    return ;
}

bool should_sleep(void) {
    unsigned long max = atomic_long_read(&MAX) ;
}