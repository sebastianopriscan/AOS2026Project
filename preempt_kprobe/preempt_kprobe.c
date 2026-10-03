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

#include "include/preempt_kprobe/preempt_kprobe.h"

// Per-CPU offset of the kernel's current_kprobe slot.
static struct kprobe * __percpu *kprobe_context_pointer ;

static DEFINE_PER_CPU(unsigned long, search_base) ;

#define setup_taget_func "probe_dummy"
static noinline void probe_dummy(void) {
    printk(KERN_DEBUG "processor %d inside of probe_dummy\n", task_cpu(current)) ;
    return ;
}

static int search_kprobe_context_pointer(struct kprobe *kp, struct pt_regs *the_regs) { 

	unsigned long* anchor = this_cpu_ptr(&search_base);
	unsigned long* temp = anchor;
	unsigned long* lowest = (unsigned long *) this_cpu_ptr(&fixed_percpu_data);

    printk(KERN_DEBUG "processor %d has entered kprobe search\n", task_cpu(current)) ;

	while (temp > lowest) {
        //brute force search of the current_kprobe per-CPU variable
        //for enabling blocking execution of the kprobe
        temp -= 1; 
        if (*temp == (unsigned long) kp) {
            // absolute address on this CPU -> per-CPU offset
            kprobe_context_pointer = (struct kprobe * __percpu *)
                ((unsigned long) temp - (unsigned long) anchor + (unsigned long) &search_base) ;
            printk(KERN_DEBUG "processor %d has found the probe address at per-CPU offset %lx\n",
                   task_cpu(current), (unsigned long) kprobe_context_pointer) ;
            break;
        }
    }

	return 0;
}

static struct kprobe setup_probe = {
    .symbol_name = setup_taget_func,
    .pre_handler = search_kprobe_context_pointer
} ;  

void reset_kprobe_context(void) {
    __this_cpu_write(*kprobe_context_pointer, NULL) ;
}

void set_kprobe_context(struct kprobe *probe) {
    __this_cpu_write(*kprobe_context_pointer, probe) ;
}

int setup_preempt_kprobe(void) {
	int ret ;

	ret = register_kprobe(&setup_probe);
	if (ret < 0) {
		return ret;
	}

    probe_dummy() ;

	unregister_kprobe(&setup_probe);

	if (kprobe_context_pointer == NULL) {
		return -ENODEV;
	}

    return 0 ;
}
