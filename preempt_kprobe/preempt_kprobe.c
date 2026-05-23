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

typedef enum {
    ON = 0,
    OFF = 1
} preempt_setup_status ;

static preempt_setup_status STATUS = OFF ;

// TODO: Evaluate if semaphore is really needed
static struct rw_semaphore internal_semaphore ;

static unsigned long SEARCH_COUNTER ;

DEFINE_PER_CPU(unsigned long *, kprobe_context_pointer) ;

#define setup_taget_func "probe_dummy"
static void probe_dummy(void*) {
    return ;
}

static int search_kprobe_context_pointer(struct kretprobe_instance *ri, struct pt_regs *the_regs) { 

	unsigned long* temp = (unsigned long *) this_cpu_ptr(&kprobe_context_pointer);

	while (temp > 0) {
        //brute force search of the current_kprobe per-CPU variable
        //for enabling blocking execution of the kretprobe
        //you can save this time setting up a per CPU-variable via 
        //smp_call_function() upon module startup
        temp -= 1; 
        if ((unsigned long) __this_cpu_read(*temp) == (unsigned long) &ri->rph->rp->kp) {
            atomic_inc((atomic_t*)&SEARCH_COUNTER);//mention we have found the target 
            break;
        }
		if(temp <= 0) return 1;
    }

	__this_cpu_write(kprobe_context_pointer, temp);

	return 0;
}

static struct kretprobe setup_probe = {
    .kp.symbol_name = setup_taget_func,
    .handler = NULL,
    .entry_handler = search_kprobe_context_pointer,
    .maxactive = -1
} ;  

void reset_kprobe_context(void) {
    down_read(&internal_semaphore) ;
    if (STATUS == ON) {
        unsigned long *current_kprobe_context_pointer ; //Question: would current_kprobe be sufficient?
        current_kprobe_context_pointer = __this_cpu_read(kprobe_context_pointer) ;
        __this_cpu_write(*current_kprobe_context_pointer, 0UL) ;
    }
    up_read(&internal_semaphore) ;
}

void set_kprobe_context(struct kprobe *probe) {
    down_read(&internal_semaphore) ;
    if (STATUS == ON) {
        unsigned long *current_kprobe_context_pointer ;
        //Question: would current_kprobe be sufficient?
        current_kprobe_context_pointer = __this_cpu_read(kprobe_context_pointer) ;
        __this_cpu_write(*current_kprobe_context_pointer, (unsigned long) probe) ;
    }
    up_read(&internal_semaphore) ;
}

int setup_preempt_kprobe(void) {
	int ret ;

    init_rwsem(&internal_semaphore) ;

    down_write(&internal_semaphore) ;
	ret = register_kretprobe(&setup_probe);
	if (ret < 0) {
        up_write(&internal_semaphore) ;
		return ret;
	}

	get_cpu();

    smp_call_function(probe_dummy,NULL,1);
    probe_dummy(NULL) ;

	put_cpu();

	unregister_kretprobe(&setup_probe);

	if(SEARCH_COUNTER != num_online_cpus()){
        up_write(&internal_semaphore) ;
		return -1;
	}
    
    STATUS = ON ;
    up_write(&internal_semaphore) ;

    return 0 ;
}