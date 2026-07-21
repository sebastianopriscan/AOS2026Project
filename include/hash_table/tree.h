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
    path_tree_entry *parent ;
    struct list_head siblings, children ;

    // Name of the entry
    struct qstr name ;

    // Status
    path_tree_entry_status entry_status ;
    struct dentry *dentry ;
    struct list_head overflow_list ;

    // List of path_with_table that match against the current path
    struct list_head pts ;

} path_tree_entry ;

/**
 * Obtain the path_tree_entry corresponding to the passed fullPath, creating it if needed
 *
 * @param fullPath The path being searched
 * @returns the corresponding path_tree_entry, or ERR_PTR in case of errors.
 */
path_tree_entry *get_path_tree_entry(char *fullPath) ;

/**
 * Unbind the path_with_table pt from the path_tree, removing entries if not busy (either with children or with associated path_with_table entries)
 * @param fullPath The path being removed
 */
void remove_path_tree_entry(char *fullPath) ;

/**
 * Obtain the path_tree_entry corresponding to the struct dentry * through the hash table
 * 
 * @param dentry The dentry to search the path_tree_entry against.
 * @returns the path_tree_entry corresponding to dentry, if found, or NULL otherwise.
 */
path_tree_entry *get_path_tree_entry_by_dentry(struct dentry *dentry) ;

void init_path_tree(void) ;

void cleanup_path_tree(void) ;

#endif