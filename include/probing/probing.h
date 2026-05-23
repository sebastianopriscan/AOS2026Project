#ifndef THROTTLEA_PROBING_H
#define THROTTLEA_PROBING_H

/**
 * Installs a kprobe on the systemcall dispatcher that will manage
 * system call throttling
 * 
 * This function does not check if the probe has already been installed nor has
 * locking mechanisms
 * 
 * @returns register_kprobe's result
 */
int enable_monitor(void) ;

/**
 * Uninstalls a previously loaded kprobe on the systemcall dispatcher that managed
 * system call throttling

 * This function does not check if the probe has already been installed nor has
 * locking mechanisms
 */
void disable_monitor(void) ;

#endif