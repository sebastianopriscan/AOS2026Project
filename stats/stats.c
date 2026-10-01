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
#include <linux/mutex.h>

#include "include/throttler_status/throttler_status.h"
#include "include/stats/stats.h"

static struct stats_register stats[2] = {
    {
        .MAX = ULONG_MAX,

        .peak_blocked = 0UL,
        .sum_blocked = 0UL,
        .num_blocked = 0UL,

        .peak_delay = 0,
        .peak_uid = 0,
        .peak_name = { '\0' },
    },
    {
        .MAX = ULONG_MAX,
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

static struct mutex max_lock ;

void init_stats(void) {
    mutex_init(&max_lock) ;
    spin_lock_init(&stats[0].reg_lock) ;
    spin_lock_init(&stats[1].reg_lock) ;

    atomic_long_set(&stat_ptr, (long) stats) ;
    atomic_long_set(&stats[0].tolerance, 0) ;
    atomic_long_set(&stats[1].tolerance, 0) ;

    return ;
}

void reset_max_value(unsigned long max) {
    struct stats_register *oldStats, *newStats;
    unsigned long flags ;
    mutex_lock(&max_lock) ;

    oldStats = &stats[current_index] ;
    current_index = (current_index +1) %2 ;
    newStats = &stats[current_index] ;

    newStats->MAX = max ;

    atomic_long_xchg(&stat_ptr, (long) newStats) ;

    synchronize_rcu() ;

    atomic_long_set(&oldStats->tolerance, 0) ;

    spin_lock_irqsave(&oldStats->reg_lock, flags) ;

    oldStats->peak_blocked = 0UL ;
    oldStats->sum_blocked = 0UL ;
    oldStats->num_blocked = 0UL ;

    oldStats->peak_delay = 0 ;
    oldStats->peak_uid = 0 ;
    oldStats->peak_name[0] = '\0' ;

    spin_unlock_irqrestore(&oldStats->reg_lock, flags) ;

    mutex_unlock(&max_lock) ;
}

void register_delay(unsigned long delay, uid_t uid, char *progName) {
    struct stats_register *reg ;
    unsigned long flags ;

    rcu_read_lock() ;
    reg = (struct stats_register *) atomic_long_read(&stat_ptr) ;

    spin_lock_irqsave(&reg->reg_lock, flags) ;

    if (delay > reg->peak_delay) {
        reg->peak_delay = delay ;
        reg->peak_uid = (unsigned long) uid ;
        if (progName) strncpy(reg->peak_name, progName, 2*PAGE_SIZE) ;
        else reg->peak_name[0] = '\0' ;
    } 

    spin_unlock_irqrestore(&reg->reg_lock, flags) ;
    rcu_read_unlock() ;
    return ;
}

void reset_tolerance(void) {
    struct stats_register *reg ;
    unsigned long tolerance ;

    rcu_read_lock() ;
    reg = (struct stats_register *) atomic_long_read(&stat_ptr) ;
    tolerance = atomic_long_xchg(&reg->tolerance, 0) ;

    if (tolerance > reg->MAX) {
        unsigned long excess = tolerance - reg->MAX ;
        spin_lock(&reg->reg_lock) ;

        if (excess > reg->peak_blocked) reg->peak_blocked = excess ;
        if (unlikely(ULONG_MAX - reg->sum_blocked < excess || reg->num_blocked == (ULONG_MAX -1))) {
            reg->sum_blocked = reg->sum_blocked / reg->num_blocked + excess ;
            reg->num_blocked = 1 ;
        } else {
            reg->sum_blocked += excess ;
            reg->num_blocked++ ;
        }

        spin_unlock(&reg->reg_lock) ;
    }
    rcu_read_unlock() ;
    return ;
}

void dump_stats(struct stats_register *reg) {
    struct stats_register *src ;
    unsigned long tolerance ;

    rcu_read_lock() ;
    src = (struct stats_register *) atomic_long_read(&stat_ptr) ;
    memcpy(reg, src, sizeof(struct stats_register)) ;

    rcu_read_unlock() ;
    return ;
}

bool should_sleep(void) {
    struct stats_register *reg ;
    unsigned long tolerance ;

    rcu_read_lock() ;
    reg = (struct stats_register *) atomic_long_read(&stat_ptr) ;
    tolerance = atomic_long_inc_return(&reg->tolerance) ;

    rcu_read_unlock() ;
    return tolerance > reg->MAX ;
}