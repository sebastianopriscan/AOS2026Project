#ifndef PREEMPT_KPROBE_H
#define PREEMPT_KPROBE_H

#include <linux/kprobes.h>

/**
 * This function cleans the current CPU's kprobe pointer, allowing
 * the user to use blocking services inside of a kprobe context
 * 
 * @warning The user must restore the kprobe context with a call to
 *          set_kprobe_context before returning from the probing hook
 * 
 */
void reset_kprobe_context(void) ;

/**
 * reset_kprobe_context's counterpart to be called before returning from a
 * blocking kprobe. Restores the current per CPU's kprobe pointer.
 * 
 * @param probe The kprobe handle the kprobe hook has been called with. Setting it
 *              with the correct kprobe without invoking reset_kprobe_context will have
 *              no effect, while setting it with another kprobe handle will result in undefined
 *              behavior (unless you really know what you're doing)
 */
void set_kprobe_context(struct kprobe *probe) ;

/**
 * Sets up the preemptable kprobe system by obtaining the per CPU kprobe context pointer
 */
int setup_preempt_kprobe(void) ;

#endif