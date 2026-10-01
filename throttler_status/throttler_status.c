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
#include "include/timers/timers.h"
#include "include/probing/probing.h"

static struct rw_semaphore status_sem ;

THROTTLER_STATUS STATUS = OFF ;

void setup_throttler_status(void) {
    init_rwsem(&status_sem) ;
}

void cleanup_throttler_status(void) {
    set_throttler_status_off() ;
}

int set_throttler_status_on(void) {
    int retval = 0;
    down_write(&status_sem) ;
    if (STATUS == OFF) {
        retval = enable_monitor() ;
        if (retval >= 0) {
            STATUS = ON ;
            setup_timers() ;
        }
    }
    up_write(&status_sem) ;
    return retval ;
}

void set_throttler_status_off(void) {
    down_write(&status_sem) ;
    if (STATUS == ON) {
        disable_monitor() ;
        STATUS = OFF ;
        cleanup_timers() ;
    }
    up_write(&status_sem) ;
}

THROTTLER_STATUS get_throttler_status() {
    THROTTLER_STATUS read_status ;

    down_read(&status_sem) ;
    read_status = STATUS;
    up_read(&status_sem) ;
    return read_status ;
}

void up_throttler_status(THROTTLER_LOCK lockKind) {
    lockKind == THROTTLER_LOCK_READ ? down_read(&status_sem) : down_write(&status_sem) ;
}

THROTTLER_STATUS down_throttler_status(THROTTLER_LOCK lockKind) {
    lockKind == THROTTLER_LOCK_READ ? up_read(&status_sem) : up_write(&status_sem) ;
    return STATUS ;
}