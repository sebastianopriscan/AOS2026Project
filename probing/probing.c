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

#include "include/names/names.h"
#include "include/probing/probing.h"
#include "include/hash_table/hash_table.h"
#include "include/hash_table/tree.h"
#include "include/syscalls/syscalls.h"
#include "include/stats/stats.h"
#include "include/timers/timers.h"
#include "include/throttler_status/throttler_status.h"

const char syscall_handler_name[] = "do_syscall_64" ;

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

static int throttler(struct kprobe *kprobe, struct pt_regs *regs) {
    char *pathBuffer, *pathPtr = NULL ;
    struct pt_regs *syscall_regs = ((struct pt_regs *)regs->di) ;
    const unsigned long syscall_code = syscall_regs->ax ;
    uid_t thread_uid = current_cred()->uid.val ;

    struct file *thread_file = get_task_exe_file(current) ;
    const char *thread_name = thread_file->f_path.dentry->d_name.name ;

    pathBuffer = kmalloc(2*PAGE_SIZE, GFP_KERNEL | GFP_ATOMIC) ;
    if (IS_ERR_OR_NULL(pathBuffer)) {
        printk(KERN_DEBUG "Memory could not be allocated for thread %d, decision will be performed only on uid\n", thread_uid) ;
    } else {
        pathPtr = file_path(thread_file, pathBuffer, 2*PAGE_SIZE) ;
        if (IS_ERR(pathPtr)) {
            printk(KERN_DEBUG "Path name resolution was incomplete for thread %d, decision will be only on UID\n", thread_uid) ;
            pathPtr = NULL ;
        }
    }

    THROTTLER_STATUS status = down_throttler_status(THROTTLER_LOCK_READ) ;
    if (status == ON) {
        if (
            is_syscall_monitored(syscall_code) &&
            ( hash_table_has(thread_uid) || ( pathPtr != NULL && path_tree_has(pathPtr) ) ) &&
            should_sleep()
        ) {
            ktime_t start, end ;
            start = ktime_get() ;
            throttle() ;
            end = ktime_get() ;

            register_delay(end - start, thread_uid, pathPtr) ;
        }
    }
    up_throttler_status(THROTTLER_LOCK_READ) ;
    
    if (pathPtr) kfree(pathBuffer) ;
    fput(thread_file) ;

    return 0 ;
}

struct kprobe throttler_kprobe = {
    .symbol_name = syscall_handler_name,
    .pre_handler = throttler,
};

int enable_monitor(void) {
    int ret ;

    ret = register_kprobe(&throttler_kprobe) ;
    if (ret < 0) {
        pr_warn(MODNAME": Error registering throttler kprobe, return code is %d\n", ret) ;
    }
    
    return ret ;
}

void disable_monitor(void) {
    unregister_kprobe(&throttler_kprobe) ;
}