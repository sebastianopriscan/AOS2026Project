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

MODULE_AUTHOR("Sebastian Roberto Opriscan <sebastianroberto.opriscan@gmail.com>");
MODULE_DESCRIPTION("This module implements a throttler for system calls invocations based on \
	specific user-IDs / programs");

#define MODNAME "ThrottleA"

#include "include/api/api.h"
#include "include/throttler_status/throttler_status.h"

static int throttleA_init(void) {

	if (setup_api() != 0) {
		return 1 ;
	}
	setup_throttler_status();
	return 0 ;
}

static void  throttleA_exit(void) {
	cleanup_api();
	cleanup_throttler_status() ;
}

module_init(throttleA_init)
module_exit(throttleA_exit)
MODULE_LICENSE("Dual MIT/GPL");
