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
#include "include/hash_table/tree.h"
#include "include/syscalls/syscalls.h"
#include "include/stats/stats.h"

static int throttleA_init(void) {
	int ret ;
	init_hash_table() ;
	init_path_tree() ;
	init_syscall_monitor() ;
	init_stats() ;
	setup_throttler_status();
	ret = setup_preempt_kprobe() ;
	if (ret != 0) {
		cleanup_path_tree() ;
		cleanup_hash_table() ;
		return ret ;
	}
	ret = setup_api() ;
	if (ret != 0) {
		cleanup_path_tree() ;
		cleanup_hash_table() ;
		return ret ;
	}
	return 0 ;
}

static void  throttleA_exit(void) {
	cleanup_api();
	cleanup_throttler_status() ;
	cleanup_path_tree() ;
	cleanup_hash_table() ;
}

module_init(throttleA_init)
module_exit(throttleA_exit)
MODULE_LICENSE("Dual MIT/GPL");
