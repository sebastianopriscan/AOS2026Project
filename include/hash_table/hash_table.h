#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <linux/kernel.h>
#include "include/api/api.h"

#define MODULUS (unsigned long) 193

/**
 * Initialize the hash table instance
 */
void init_hash_table(void) ;

/**
 * Cleanup the hash table instance
 */
void cleanup_hash_table(void) ;

/**
 * Insert a uid into its hash table
 * @param uid: The uid to insert
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_insert_uid(uid_t uid) ;

/**
 * Remove a policy from the hash table
 * @param uid: The uid to remove
 * @return 0 in case of success, non-zero otherwise
 */
int hash_table_remove_uid(uid_t uid) ;

/**
 * Get a policy table entry
 * @param uid: The thread's effective user id
 * @returns A boolean indicating the uid is contained in the table or not
 */
bool hash_table_has(uid_t uid) ;

#endif