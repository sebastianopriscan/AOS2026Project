#ifndef TREE_H
#define TREE_H

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
#include <linux/fs_struct.h>

#include "include/api/api.h"
#include "include/oracles/oracles.h"
#include "include/hash_table/hash_table.h"

typedef enum {
    PATH_TREE_ENTRY_ACTIVE,
    PATH_TREE_ENTRY_INACTIVE
} path_tree_entry_status ;

typedef struct _path_tree_entry {
    /**
     * Linking to other entries
     */
    struct _path_tree_entry *parent ;
    struct list_head siblings, children ;

    // Name of the entry
    struct qstr name ;

    // Status
    path_tree_entry_status entry_status ;

    // Locking
    struct mutex entry_mutex ;

    // Cleanup
    struct list_head remove_list ;

} path_tree_entry ;


/**
 * Obtain the path_tree_entry corresponding to the passed fullPath, creating it if needed
 *
 * @param fullPath The path being searched
 * @returns 0 in case of positive return, an error code otherwise
 */
int insert_path_tree_entry(char *fullPath) ;

/**
 * Unbind the path_with_table pt from the path_tree, removing entries if not busy (either with children or with associated path_with_table entries)
 * @param fullPath The path being removed
 * @returns 0 if the path was removed, -ENOENT if it is not monitored, -EBUSY if the tree is being dumped
 */
int remove_path_tree_entry(char *fullPath) ;

/**
 * Check if the fullPath is under management of the path_tree
 *
 * @param fullPath The path being searched
 * @returns A boolean indicating the presence of the program in the path tree
 */
bool path_tree_has(const char *fullPath) ;

/**
 * Trylock the whole path tree for
 * dumping purposes
 * 
 * returns 0 if the lock is acquired, 1 otherwise
 */
int path_tree_lock(void) ;

/**
 * Unlock the previously locked whole path tree
 */
void path_tree_unlock(void) ;

/**
 * @returns A value that changes whenever the tree may have been modified
 */
unsigned long path_tree_generation(void) ;

void init_path_tree(void) ;

void cleanup_path_tree(void) ;

ssize_t pt_file_handle_read(char __user *buf, ssize_t len) ;
void reset_pt_file_handle(void) ;


#endif