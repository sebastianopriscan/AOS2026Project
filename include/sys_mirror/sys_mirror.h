#ifndef SYS_MIRROR_H
#define SYS_MIRROR_H

#include "include/hash_table/hash_table.h"

/**
 * Add a policy_with_table to the sysfs mirror
 */
int sys_mirror_policy_add(policy_with_table *table) ;

/**
 * Remove a policy_with_table to the sysfs mirror
 */
void sys_mirror_policy_rm(policy_with_table *table) ;

/**
 * Add a path_with_table to the sysfs mirror
 */
int sys_mirror_path_add(path_with_table *table) ;

/**
 * Remove a path_with_table to the sysfs mirror
 */
void sys_mirror_path_rm(path_with_table *table) ;

/**
 * Realizes a two way binding between a policy and
 * a path
 */
int bind_policy_to_path(policy_with_table *table, path_with_table *path) ;

/**
 * Unbinds a path-policy binding
 */
void unbind_path(path_with_table *path) ;

/**
 * Unbinds a policy to path binding
 */
void unbind_policy(policy_with_table *table, unsigned long id) ;

int init_ht_sys_mirror(void) ;

void clean_ht_sys_mirror(void) ;


#endif