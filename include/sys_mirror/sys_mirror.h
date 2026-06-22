#ifndef SYS_MIRROR_H
#define SYS_MIRROR_H

#include "include/hash_table/hash_table.h"

/**
 * Add a policy_with_table to the sysfs mirror
 */
int sys_mirror_add(policy_with_table *table) ;

/**
 * Remove a policy_with_table to the sysfs mirror
 */
int sys_mirror_rm(policy_with_table *table) ;

int init_ht_sys_mirror(void) ;

void clean_ht_sys_mirror(void) ;


#endif