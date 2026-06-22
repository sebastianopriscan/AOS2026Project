#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <linux/kernel.h>
#include "include/api/api.h"

struct _policy_with_table {
    struct list_head hash_head ;
    atomic_long_t throttle_counter ;
    atomic_t isActive ;
    throttleA_policy_internal policy ;
    struct kobj_attribute kobj_attribute ;
} ;
typedef struct _policy_with_table policy_with_table ;

/**
 * Initialize the hash table instance
 */
int init_hash_table(void) ;

/**
 * Cleanup the hash table instance
 */
void cleanup_hash_table(void) ;

/**
 * Insert a policy into the hash table
 * @param table: The hash table to insert the policy into
 * @param policy: The policy to insert
 * @param pathName: Resolved absolute pathname from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_insert(throttleA_policy *policy, char *pathName) ;

/**
 * Remove a policy from the hash table
 * @param policy: The policy to be removed
 * @param pathName: Resolved absolute pathname from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_remove(throttleA_policy *policy, char *pathName) ;

/**
 * Delete syscalls from a policy from the hash table
 * @param pathName: Resolved absolute pathname from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_delete(throttleA_policy *policy, char *pathName) ;

/**
 * Get a policy table entry
 * This function keeps the rcu lock active in case of positive return, so a corresponding call to
 * hash_table_put should be invoked when the policy handle is no longer of use.
 * @warning Don't use any blocking API until the hash table is freed.
 * @param uid: The thread's effective user id
 * @param pathname: The thread's program name
 * @returns NULL in case the policy handle is not found, the policy handler otherwise.
 */
policy_with_table *hash_table_get(uid_t uid, const char *pathname) ;

/**
 * To be invoked when a previously obtained policy handle is not of use anymore.  
 * @warning Don't use any blocking API until the hash table is freed.
 */
void hash_table_put(void) ;

/**
 * Refresh the throttle_counter for each entry
 */
void hash_table_refresh(void) ;


#endif