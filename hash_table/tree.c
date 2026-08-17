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
#include <linux/namei.h>

#include "include/hash_table/tree.h"
#include "include/hash_table/hash_table.h"
#include "include/utils/strings.h"

static struct hash_table dentry_table ;

static inline int evaluate_hash(const void *ptr) {
    return ((int) (((unsigned long) ptr) % MODULUS)) ;
}

static inline void INIT_PATH_TREE_ENTRY(path_tree_entry *entry, struct dentry *root_dentry) {
    entry->parent = entry ; 
    INIT_LIST_HEAD(&entry->children) ; 
    INIT_LIST_HEAD(&entry->siblings) ; 
    entry->name.len = 1 ; 
    entry->name.name = "/" ; 
    entry->entry_status = PATH_TREE_ENTRY_ACTIVE ; 
    dget(root_dentry) ;
    entry->dentry = root_dentry ;
    list_add(&entry->overflow_list, &dentry_table.records[evaluate_hash(root_dentry)].overflow_list) ;
}

static inline void CLEANUP_PATH_TREE_ENTRY(path_tree_entry *root) {

    path_tree_entry *entry = root;

    do {
        if (!list_is_head((&entry->children)->next, &entry->children)) {
            entry = container_of((&entry->children)->next, path_tree_entry, siblings) ;
        } else {
            if (entry != root) {
                struct list_head *pos, *tmp ;
                path_with_table *ptToDelete ;
                path_tree_entry *toDelete = entry ;
                entry = entry->parent ;
                list_del(&entry->siblings) ;

                pos = toDelete->overflow_list.next ;
                do {
                    tmp = pos ;
                    pos = pos->next ;

                    list_del(tmp) ;
                    kfree(list_entry(tmp, path_with_table, overflow_list)) ;
                } while (!list_is_head(pos, &toDelete->overflow_list)) ;

                kfree(toDelete) ;
            }
        }
    } while (entry != root) ;
}

path_tree_entry ROOT ;

path_tree_entry *get_path_tree_entry(char *fullPath) {
    struct list_head *pos ;
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;

    do {
        path_tree_entry *newEntry = NULL ;
        struct list_head *pos ;

        list_for_each(pos, &base->children) {
            path_tree_entry *entry = list_entry(pos, path_tree_entry, siblings) ;
            if (slashcmp(entry->name.name, pathPtr) == 0) {
                base = entry ;
                goto incr_step;
            }
        }

        newEntry = kmalloc(sizeof(path_tree_entry), GFP_KERNEL) ;
        if (IS_ERR_OR_NULL(newEntry)) return newEntry ;

        INIT_LIST_HEAD(&newEntry->children) ;

        newEntry->entry_status = PATH_TREE_ENTRY_INACTIVE ;
        newEntry->dentry = NULL ;
        newEntry->flags = 0UL ;
        INIT_LIST_HEAD(&newEntry->overflow_list) ;
        if (base->entry_status == PATH_TREE_ENTRY_ACTIVE) {
            list_for_each(pos, &base->dentry->d_subdirs) {
                struct dentry *child = list_entry(pos, struct dentry, d_child) ; 
                dget(child) ;
                if (slashcmp(child->d_name.name, pathPtr)) {
                    newEntry->entry_status = PATH_TREE_ENTRY_ACTIVE ;
                    newEntry->dentry = child ;
                    if (d_is_dir(child)) set_pt_directory(newEntry) ;
                    if (d_is_symlink(child)) set_pt_symlink(newEntry) ;
                    list_add(&newEntry->overflow_list, &dentry_table.records[evaluate_hash(child)].overflow_list) ;
                    break ;
                }
                dput(child) ;
            }
        }

        newEntry->name.name = pathPtr ;
        newEntry->name.len = slashlen(pathPtr) ;
        newEntry->parent = base ;
        list_add(&newEntry->siblings, &base->children) ;
        INIT_LIST_HEAD(&newEntry->pts) ;
        base = newEntry ;

incr_step:
        pathPtr += slashlen(pathPtr) ;

        if (*pathPtr == '\0') {
            if (newEntry) {
                return newEntry ;
            }
            return base ;
        }
        pathPtr++ ;

    } while (1) ;
}

path_tree_entry *get_path_tree_entry_by_dentry(struct dentry *dentry) {
    path_tree_entry *entry ;

    list_for_each_entry(entry, &dentry_table.records[evaluate_hash(dentry)].overflow_list, overflow_list) {
        if (entry->dentry == dentry) return entry ;
    }

    return NULL ;
}

path_tree_entry *materialize_child(struct dentry *parent, struct dentry *child) {
    path_tree_entry *ptChild = NULL, *ptParent = get_path_tree_entry_by_dentry(parent) ;
    
    if (ptParent) {
        list_for_each_entry(ptChild, &(ptParent->children), siblings) {
            if (strcmp(child->d_name.name, ptChild->name.name) == 0) {
                ptChild->entry_status = PATH_TREE_ENTRY_ACTIVE ;
                dget(child) ;
                ptChild->dentry = child ;
                list_add(&ptChild->overflow_list, &dentry_table.records[evaluate_hash(child)].overflow_list) ;
                if (d_is_symlink(child)) set_pt_symlink(ptChild) ;
                if (d_is_dir(child)) set_pt_directory(ptChild) ;
                return ptChild ;
            }
        }
    }
    return NULL;
}

void dematerialize_entry(path_tree_entry *entry) {
    entry->entry_status = PATH_TREE_ENTRY_INACTIVE ;
    dput(entry->dentry) ;
    entry->dentry = NULL ;
    list_del(&entry->overflow_list) ;
}

void remove_path_tree_entry(char *fullPath) {
    struct list_head *pos ;
    path_tree_entry *base = &ROOT ;
    char *pathPtr = fullPath +1;

    do {
        list_for_each(pos, &base->children) {
            path_tree_entry *entry = list_entry(pos, path_tree_entry, siblings) ;
            if (slashcmp(entry->name.name, pathPtr) == 0) {
                base = entry ;
                pathPtr += slashlen(pathPtr) ;
                if (*pathPtr == '\0') {
                    break ;
                }
                pathPtr++ ;
                continue ;
            }
        }
        return ;
    } while (1) ;
    
    do {
        if (base == &ROOT) return ;

        if (list_empty(&base->children) && list_empty(&base->pts)) {
            path_tree_entry *entry = base ;
            base = base->parent ;
            if (entry->entry_status == PATH_TREE_ENTRY_ACTIVE) {
                list_del(&entry->overflow_list) ;
                dput(entry->dentry) ;
            }
            list_del(&entry->siblings) ;
            kfree(entry) ;
        } else {
            return ;
        }
    } while (1) ;
}

void init_path_tree(void) {
    struct path root ;
    get_fs_root(current->fs, &root) ;

    INIT_PATH_TREE_ENTRY(&ROOT, root.dentry->d_inode) ;

    path_put(&root) ;

    return ;
}

void cleanup_path_tree(void) {
    CLEANUP_PATH_TREE_ENTRY(&ROOT) ;
}