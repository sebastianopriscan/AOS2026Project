#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <linux/kernel.h>
#include "include/api/api.h"
#include "include/oracles/oracles.h"

#define PATH_TABLE_SYMLINK_NAME "inode"

/**
 * Internal version with atomic types and
 * inode support
 */
struct _throttleA_policy_internal {
    policy_kind policy;
    unsigned int uid ;
    inode_descriptor inode ;
    atomic_t tolerance ;
    atomic_long_t syscalls[DATA_PER_LIMIT(unsigned long)] ;
} ;
typedef struct _throttleA_policy_internal throttleA_policy_internal ;

struct path_with_table {
    struct list_head overflow_list, handle_list ;
    throttleA_path path ;
    throttleA_policy_internal policy ;
    unsigned long long id ;
    bool bound ;
    struct kobject *child, *desc, *name ;
    struct kobj_attribute desc_attribute, path_attribute ;
} ;
typedef struct path_with_table path_with_table ;

struct _policy_with_table {
    struct list_head hash_head ;
    struct list_head bound_paths ;
    atomic_long_t throttle_counter ;
    atomic_t isActive, refCount ;
    throttleA_policy_internal policy ;
    struct kobject *kobj ;
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
 * @param pathName: Resolved path_decree from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_insert(throttleA_policy *policy, path_decree *decree) ;

/**
 * Remove a policy from the hash table
 * @param policy: The policy to be removed
 * @param decree: Resolved path_decree from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_remove(throttleA_policy *policy, path_decree *decree) ;

/**
 * Delete syscalls from a policy from the hash table
 * @param policy: The policy to be deleted
 * @param decree: Resolved path_decree from the policy
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_delete(throttleA_policy *policy, path_decree *decree) ;

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