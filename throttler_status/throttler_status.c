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

struct rw_semaphore status_sem ;

typedef enum {
    ON,
    OFF
} THROTTLER_STATUS;

THROTTLER_STATUS STATUS = OFF ;

void setup_throttler_status(void) {
    init_rwsem(&status_sem) ;
}

void cleanup_throttler_status(void) {

}

void set_throttler_status_on(void) {
    down_write(&status_sem) ;
    STATUS = ON ;
    up_write(&status_sem) ;
}

void set_throttler_status_off(void) {
    down_write(&status_sem) ;
    STATUS = OFF ;
    up_write(&status_sem) ;
}

THROTTLER_STATUS get_throttler_status() {
    down_read(&status_sem) ;
    THROTTLER_STATUS read_status = STATUS;
    up_read(&status_sem) ;
    return read_status ;
}