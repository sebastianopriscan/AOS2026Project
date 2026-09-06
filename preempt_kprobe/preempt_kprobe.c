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

rwlock_t internal_lock ;

static unsigned long SEARCH_COUNTER ;

DEFINE_PER_CPU(unsigned long *, kprobe_context_pointer) ;

#define setup_taget_func "probe_dummy"
static void __attribute__((optimize("O0"))) probe_dummy(void*) {
    return ;
}

static int search_kprobe_context_pointer(struct kprobe *kp, struct pt_regs *the_regs) { 

	unsigned long* temp = (unsigned long *) this_cpu_ptr(&kprobe_context_pointer);

	while (temp > 0) {
        //brute force search of the current_kprobe per-CPU variable
        //for enabling blocking execution of the kretprobe
        //you can save this time setting up a per CPU-variable via 
        //smp_call_function() upon module startup
        temp -= 1; 
        if (*temp == (unsigned long) kp) {
            atomic_inc((atomic_t*)&SEARCH_COUNTER);//mention we have found the target 
            break;
        }
		if(temp <= 0) return 1;
    }

	__this_cpu_write(kprobe_context_pointer, temp);

	return 0;
}

static struct kprobe setup_probe = {
    .symbol_name = setup_taget_func,
    .pre_handler = search_kprobe_context_pointer
} ;  

struct kprobe *reset_kprobe_context(void) {
    struct kprobe *retVal ;
    read_lock(&internal_lock) ;
    if (STATUS == ON) {
        unsigned long *current_kprobe_context_pointer ; //Question: would current_kprobe be sufficient?
        current_kprobe_context_pointer = __this_cpu_read(kprobe_context_pointer) ;
        retVal = (void *) __this_cpu_read(*current_kprobe_context_pointer) ;
        __this_cpu_write(*current_kprobe_context_pointer, 0UL) ;
        preempt_enable() ;
    }
    read_unlock(&internal_lock) ;
    return retVal ;
}

void set_kprobe_context(struct kprobe *probe) {
    read_lock(&internal_lock) ;
    if (STATUS == ON) {
        unsigned long *current_kprobe_context_pointer ;
        //Question: would current_kprobe be sufficient?
        current_kprobe_context_pointer = __this_cpu_read(kprobe_context_pointer) ;
        __this_cpu_write(*current_kprobe_context_pointer, (unsigned long) probe) ;
        preempt_disable() ;
    }
    read_unlock(&internal_lock) ;
}

int setup_preempt_kprobe(void) {
	int ret ;

    rwlock_init(&internal_lock) ;

    write_lock(&internal_lock) ;
	ret = register_kprobe(&setup_probe);
	if (ret < 0) {
        write_unlock(&internal_lock) ;
		return ret;
	}

	get_cpu();

    smp_call_function(probe_dummy,NULL,1);
    probe_dummy(NULL) ;

	put_cpu();

	unregister_kprobe(&setup_probe);

	if(SEARCH_COUNTER != num_online_cpus()){
        write_unlock(&internal_lock) ;
		return -1;
	}
    
    STATUS = ON ;
    write_unlock(&internal_lock) ;

    return 0 ;
}