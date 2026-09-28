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
#include "include/preempt_kprobe/preempt_kprobe.h"
#include "include/hash_table/hash_table.h"

static int throttleA_init(void) {
	init_hash_table() ;
	setup_throttler_status();
	if (setup_preempt_kprobe() != 0) {
		cleanup_hash_table() ;
		return -1 ;
	}
	if (setup_api() != 0) {
		cleanup_hash_table() ;
		return -1 ;
	}
	return 0 ;
}

static void  throttleA_exit(void) {
	cleanup_api();
	cleanup_throttler_status() ;
	cleanup_hash_table() ;
}

module_init(throttleA_init)
module_exit(throttleA_exit)
MODULE_LICENSE("Dual MIT/GPL");
