#ifndef THROTTLEA_STATS_H
#define THROTTLEA_STATS_H

struct stats_register {
    atomic_long_t tolerance ;
    spinlock_t reg_lock ;
    unsigned long MAX ;

    unsigned long peak_blocked ;
    unsigned long sum_blocked ;
    unsigned long num_blocked ;

    unsigned long peak_delay ;
    unsigned long peak_uid ;
    unsigned char peak_name[2*PAGE_SIZE] ;
} ;

void init_stats(void) ;

void reset_max_value(unsigned long max) ;

void register_delay(unsigned long delay, uid_t uid, char *progName) ;

void reset_tolerance(void) ;

void dump_stats(struct stats_register *reg) ;

bool should_sleep(void) ;

#endif