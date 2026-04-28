#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "include/api/api.h"

struct _policy_with_table {
    struct list_head hash_head ;
    throttleA_policy policy ;
    unsigned long throttle_counter ;
} ;
typedef struct _policy_with_table policy_with_table ;

/**
 * Initialize the hash table instance
 */
void init_hash_table(void) ;

/**
 * Insert a policy into the hash table
 * @param table: The hash table to insert the policy into
 * @param policy: The policy to insert
 * @return 0 in case of success, 1 otherwise
 */
int hash_table_insert(throttleA_policy *policy) ;

/**
 * Remove a policy from the hash table
 * @param policy: The policy to be removed
 */
int hash_table_remove(throttleA_policy *policy) ;

/**
 * Get a policy table entry
 * @param uid: The thread's effective user id
 * @param pathname: The thread's program name
 */
policy_with_table *hash_table_get(uid_t uid, const char *pathname) ;

/**
 * Refresh the throttle_counter for each entry
 */
void hash_table_refresh(void) ;


#endif