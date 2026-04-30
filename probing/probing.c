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
#include "include/probing/probing.h"
#include "include/hash_table/hash_table.h"

const char syscall_handler_name[] = "do_syscall_64" ;

int throttler(struct kprobe *kprobe, struct pt_regs *regs) {
    struct pt_regs *syscall_regs = ((struct pt_regs *)regs->di) ;
    kuid_t thread_uid = current_cred()->uid ;
    struct file *thread_file = get_task_exe_file(current) ;
    const char *thread_name = thread_file->f_path.dentry->d_name.name ;

    policy_with_table *policy = hash_table_get(thread_uid.val, thread_name) ;

    fput(thread_file) ;

    
}

struct kprobe throttler_kprobe = {
    .symbol_name = syscall_handler_name
};

int enable_monitor(void) {
    int ret ;

    ret = register_kprobe(&throttler_kprobe) ;
    if (ret < 0) {
        pr_warn(MODNAME": Error registering throttler kprobe, return code is %d\n", ret) ;
    }
    
    return ret ;
}

int disable_monitor(void) {
    unregister_kprobe(&throttler_kprobe) ;
}