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

static atomic_long_t next_ticket ;
static unsigned long served ;

static unsigned long window_budget(void) {
    return min_t(unsigned long, get_max_value(), LONG_MAX / 2) ;
}

static enum hrtimer_restart throttler_poller(struct hrtimer *timer) {
    unsigned long next = atomic_long_read(&next_ticket) ;
    unsigned long base = served ;

    if ((long)(next - base) > 0) register_blocked(next - base) ;
    else base = next ;

    WRITE_ONCE(served, base + window_budget()) ;
    wake_up_all(&throttler_waitqueue) ;
    hrtimer_forward_now(timer, oneSecond) ;
    return HRTIMER_RESTART ;
}

void setup_timers(void) {
    hrtimer_init(&throttler_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL) ;
    throttler_timer.function = throttler_poller ;
    oneSecond = ktime_set(1,0) ;
    WRITE_ONCE(served, atomic_long_read(&next_ticket) + window_budget()) ;

    hrtimer_start(&throttler_timer, oneSecond, HRTIMER_MODE_REL) ;
}

void cleanup_timers(void) {
    hrtimer_cancel(&throttler_timer) ;
    WRITE_ONCE(served, atomic_long_read(&next_ticket)) ;
    wake_up_all(&throttler_waitqueue) ;
}

unsigned long take_ticket(void) {
    return (unsigned long) atomic_long_inc_return(&next_ticket) ;
}

bool ticket_served(unsigned long ticket) {
    return (long)(READ_ONCE(served) - ticket) >= 0 ;
}

int throttle(unsigned long ticket) {
    return wait_event_killable(throttler_waitqueue, ticket_served(ticket)) ;
}