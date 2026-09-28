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

#include "include/names/names.h"
#include "include/timers/timers.h"
#include "include/throttler_status/throttler_status.h"
#include "include/stats/stats.h"

static struct hrtimer throttler_timer ;
static ktime_t oneSecond ;

DECLARE_WAIT_QUEUE_HEAD(throttler_waitqueue) ;

static enum hrtimer_restart throttler_poller(struct hrtimer *timer) {
    reset_tolerance() ;
    wake_up(&throttler_waitqueue) ;
    return HRTIMER_RESTART ;
}

void setup_timers(void) {
    hrtimer_init(&throttler_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL) ;
    throttler_timer.function = throttler_poller ;
    oneSecond = ktime_set(1,0) ;

    hrtimer_start(&throttler_timer, oneSecond, HRTIMER_MODE_REL) ;
}

void cleanup_timers(void) {
    hrtimer_cancel(&throttler_timer) ;    
}

void throttle(void) {
    wait_event(throttler_waitqueue, 1) ;
    return ;
}