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
#include <linux/ktime.h>
#include <linux/wait_bit.h>

#include "include/names/names.h"
#include "include/probing/probing.h"
#include "include/hash_table/hash_table.h"
#include "include/hash_table/tree.h"
#include "include/syscalls/syscalls.h"
#include "include/stats/stats.h"
#include "include/timers/timers.h"
#include "include/throttler_status/throttler_status.h"
#include "include/preempt_kprobe/preempt_kprobe.h"

const char syscall_handler_name[] = "x64_sys_call" ;

static atomic_t throttled_threads = ATOMIC_INIT(0) ;

/**
 * get_mm_exe_file - acquire a reference to the mm's executable file
 *
 * Returns %NULL if mm has no associated executable file.
 * User must release file via fput().
 */
struct file *get_mm_exe_file(struct mm_struct *mm)
{
	struct file *exe_file;

	rcu_read_lock();
	exe_file = rcu_dereference(mm->exe_file);
	if (exe_file && !get_file_rcu(exe_file))
		exe_file = NULL;
	rcu_read_unlock();
	return exe_file;
}

/**
 * get_task_exe_file - acquire a reference to the task's executable file
 *
 * Returns %NULL if task's mm (if any) has no associated executable file or
 * this is a kernel thread with borrowed mm (see the comment above get_task_mm).
 * User must release file via fput().
 */
struct file *get_task_exe_file(struct task_struct *task)
{
	struct file *exe_file = NULL;
	struct mm_struct *mm;

	task_lock(task);
	mm = task->mm;
	if (mm) {
		if (!(task->flags & PF_KTHREAD))
			exe_file = get_mm_exe_file(mm);
	}
	task_unlock(task);
	return exe_file;
}

static char *get_exe_path(struct file **file, char **buffer, gfp_t flags) {
    char *pathPtr ;

    *file = get_task_exe_file(current) ;
    if (*file == NULL) return NULL ;

    *buffer = kmalloc(2*PAGE_SIZE, flags) ;
    if (*buffer == NULL) {
        printk(KERN_DEBUG "Memory could not be allocated for thread %d, its path is not resolved\n", current->pid) ;
        return NULL ;
    }

    pathPtr = file_path(*file, *buffer, 2*PAGE_SIZE) ;
    if (IS_ERR(pathPtr)) {
        printk(KERN_DEBUG "Path name resolution was incomplete for thread %d\n", current->pid) ;
        return NULL ;
    }

    return pathPtr ;
}

static int throttler(struct kprobe *kprobe, struct pt_regs *regs) {
    char *pathBuffer = NULL, *pathPtr = NULL ;
    struct file *thread_file = NULL ;
    const unsigned int syscall_code = (unsigned int) regs->si ;
    uid_t thread_uid ;
    unsigned long ticket ;
    ktime_t start, end ;

    if (!is_syscall_monitored(syscall_code)) return 0 ;

    thread_uid = from_kuid(&init_user_ns, current_euid()) ;
    if (!hash_table_has(thread_uid)) {
        pathPtr = get_exe_path(&thread_file, &pathBuffer, GFP_ATOMIC) ;
        if (pathPtr == NULL || !path_tree_has(pathPtr)) goto out ;
    }

    ticket = take_ticket() ;
    if (ticket_served(ticket)) goto out ;

    if (preempt_count() != PREEMPT_DISABLE_OFFSET) {
        pr_warn_once(MODNAME": Probe context is not preemptible (preempt_count %#x), throttling is skipped\n", preempt_count()) ;
        goto out ;
    }

    atomic_inc(&throttled_threads) ;
    reset_kprobe_context() ;
    preempt_enable() ;

    start = ktime_get() ;
    throttle(ticket) ;
    end = ktime_get() ;

    if (pathPtr == NULL) pathPtr = get_exe_path(&thread_file, &pathBuffer, GFP_KERNEL) ;

    preempt_disable() ;
    set_kprobe_context(kprobe) ;

    register_delay(end - start, thread_uid, pathPtr) ;
    if (atomic_dec_and_test(&throttled_threads)) wake_up_var(&throttled_threads) ;

out:
    if (pathBuffer) kfree(pathBuffer) ;
    if (thread_file) fput(thread_file) ;

    return 0 ;
}

struct kprobe throttler_kprobe = {
    .symbol_name = syscall_handler_name,
    .pre_handler = throttler,
};

int enable_monitor(void) {
    int ret ;

    throttler_kprobe.addr = NULL ;
    throttler_kprobe.flags = 0 ;

    ret = register_kprobe(&throttler_kprobe) ;
    if (ret < 0) {
        pr_warn(MODNAME": Error registering throttler kprobe, return code is %d\n", ret) ;
    }
    
    return ret ;
}

void disable_monitor(void) {
    unregister_kprobe(&throttler_kprobe) ;
    cleanup_timers() ;
    wait_var_event(&throttled_threads, !atomic_read(&throttled_threads)) ;
    synchronize_rcu() ;
}