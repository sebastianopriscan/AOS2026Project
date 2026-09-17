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

static inline void lock_path_tree_entry(path_tree_entry *entry, struct list_head *unlock_stack) {
    if (entry->dentry) {
        struct inode *entry_inode = d_inode(entry->dentry) ;
        if (entry_inode) inode_lock(entry_inode) ;
    }
    down_write(&entry->entry_sem) ;
    list_add(&entry->unlock_data.stack, unlock_stack) ;
}

static inline void lock_path_tree_entry_with_ht(path_tree_entry *entry, struct list_head *unlock_stack) {
    if (entry->dentry) {
        struct hash_table_record *record ;
        struct inode *entry_inode = d_inode(entry->dentry) ;
        if (entry_inode) inode_lock(entry_inode) ;

        record = &dentry_table.records[evaluate_hash(entry->dentry)] ;
        down_write(&record->sem) ;
    }
    down_write(&entry->entry_sem) ;
    list_add(&entry->unlock_data.stack, unlock_stack) ;
}

static inline void unlock_path_tree_entry(path_tree_entry *entry) {
    struct dentry *entry_dentry = entry->dentry ;
    struct inode *entry_inode = NULL ;
    if (entry_dentry) entry_inode = d_inode(entry->dentry) ;
    up_write(&entry->entry_sem) ;

    if (entry->ht_sem) {
        struct hash_table_record *record = &dentry_table.records[evaluate_hash(entry->dentry)] ;
        up_write(&record->sem) ;
        entry->ht_sem = NULL ;
    }
    if (entry_inode) inode_unlock(entry_inode) ;
}

static void unlock_data_callback(unlock_data *data) {
    unlock_path_tree_entry(container_of(data, path_tree_entry, unlock_data)) ;
}

static inline void INIT_PATH_TREE_ENTRY(path_tree_entry *entry, struct dentry *root_dentry) {
    struct hash_table_record *record = &dentry_table.records[evaluate_hash(root_dentry)] ;

    entry->parent = entry ; 
    INIT_LIST_HEAD(&entry->children) ; 
    INIT_LIST_HEAD(&entry->siblings) ; 
    entry->name.len = 1 ; 
    entry->name.name = "/" ; 
    entry->entry_status = PATH_TREE_ENTRY_ACTIVE ; 
    dget(root_dentry) ;
    entry->dentry = root_dentry ;
    entry->ht_sem = &record->sem ;
    list_add(&entry->overflow_list, &record->overflow_list) ;
    init_rwsem(&entry->entry_sem) ;
    entry->unlock_data.unlock = unlock_data_callback ;
}

static inline void CLEANUP_PATH_TREE_ENTRY(path_tree_entry *root) {

    path_tree_entry *entry = root;

    do {
        if (!list_is_head((&entry->children)->next, &entry->children)) {
            entry = container_of((&entry->children)->next, path_tree_entry, siblings) ;
        } else {
            if (entry != root) {
                struct list_head *pos, *tmp ;
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

static inline path_tree_entry *__get_path_tree_entry(char *fullPath, struct list_head *unlock_stack, void (*lockFunc)(path_tree_entry *, struct list_head *), const bool unlock) {
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;
    int components = 0 ;

    lockFunc(&ROOT, unlock_stack) ;

    do {
        path_tree_entry *newEntry = NULL ;
        struct list_head *pos ;
        char *name ;

        list_for_each(pos, &base->children) {
            path_tree_entry *entry = list_entry(pos, path_tree_entry, siblings) ;
            if (slashcmp(entry->name.name, pathPtr) == 0) {
                if (base->parent != base && unlock) {
                    list_del(&base->parent->unlock_data.stack) ;
                    unlock_path_tree_entry(base->parent) ;
                }
                lockFunc(entry, unlock_stack) ;
                base = entry ;
                components++ ;
                goto incr_step;
            }
        }

        newEntry = kmalloc(sizeof(path_tree_entry), GFP_KERNEL) ;
        if (IS_ERR_OR_NULL(newEntry)) goto free_allocations ;
        name = kmalloc(slashlen(pathPtr), GFP_KERNEL) ;
        if (IS_ERR_OR_NULL(name)) {
            kfree(newEntry) ;
            goto free_allocations ;
        }

        INIT_LIST_HEAD(&newEntry->children) ;

        newEntry->entry_status = PATH_TREE_ENTRY_INACTIVE ;
        newEntry->dentry = NULL ;
        newEntry->flags = 0UL ;
        newEntry->name.name = name ;
        newEntry->name.len = slashlen(name) ;
        INIT_LIST_HEAD(&newEntry->overflow_list) ;
        if (base->entry_status == PATH_TREE_ENTRY_ACTIVE) {
            struct dentry *child = d_hash_and_lookup(base->dentry, &newEntry->name) ;
            if (!IS_ERR_OR_NULL(child)) {
                struct inode *inode = d_inode(child) ;
                struct hash_table_record *record = &dentry_table.records[evaluate_hash(child)] ;

                if (inode) inode_lock(inode) ;
                down_write(&record->sem) ;
                list_add(&newEntry->overflow_list, &record->overflow_list) ;
                up_write(&record->sem) ;

                newEntry->ht_sem = &record->sem ;
                newEntry->entry_status = PATH_TREE_ENTRY_ACTIVE ;
                newEntry->dentry = child ;
                if (d_is_dir(child)) set_pt_directory(newEntry) ;
                if (d_is_symlink(child)) set_pt_symlink(newEntry) ;
            }
        }

        init_rwsem(&newEntry->entry_sem) ;
        down_write(&newEntry->entry_sem) ;
        newEntry->unlock_data.unlock = unlock_data_callback ;
        list_add(&newEntry->unlock_data.stack, unlock_stack) ;

        newEntry->parent = base ;
        list_add(&newEntry->siblings, &base->children) ;
        INIT_LIST_HEAD(&newEntry->pts) ;

        if (base->parent != base && unlock) unlock_path_tree_entry(base->parent) ;
        base = newEntry ;
        components++ ;

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

free_allocations:
    perform_unlocking(unlock_stack) ;
    remove_path_tree_entry(fullPath) ;

    return NULL ;
}

path_tree_entry *get_path_tree_entry(char *fullPath, struct list_head *unlock_stack) {
    return __get_path_tree_entry(fullPath, unlock_stack, lock_path_tree_entry, true) ;
}


path_tree_entry *get_path_tree_entry_by_dentry(struct dentry *dentry) {
    path_tree_entry *entry ;
    int hash = evaluate_hash(dentry) ;
    struct hash_table_record *record = &dentry_table.records[hash] ;
    down_read(&record->sem) ;

    list_for_each_entry(entry, &dentry_table.records[hash].overflow_list, overflow_list) {
        if (entry->dentry == dentry) {
            entry->ht_sem = &record->sem ;
            down_read(&entry->parent->entry_sem) ;
            down_read(&entry->entry_sem) ;
            return entry ;
        }
    }

    up_read(&record->sem) ;
    return NULL ;
}

void put_path_tree_entry(path_tree_entry *entry) {
    unlock_path_tree_entry(entry) ;
}

path_tree_entry *materialize_child(struct dentry *parent, struct dentry *child) {
    path_tree_entry *ptChild = NULL, *ptParent = get_path_tree_entry_by_dentry(parent) ;
    
    if (ptParent) {
        list_for_each_entry(ptChild, &(ptParent->children), siblings) {
            if (strcmp(child->d_name.name, ptChild->name.name) == 0) {
                struct hash_table_record *record = &dentry_table.records[evaluate_hash(child)] ;
                ptChild->entry_status = PATH_TREE_ENTRY_ACTIVE ;
                dget(child) ;
                ptChild->dentry = child ;
                down_write(&record->sem) ;
                list_add(&ptChild->overflow_list, &record->overflow_list) ;
                up_write(&record->sem) ;
                ptChild->ht_sem = &record->sem ;
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
    struct list_head unlock_stack ;
    path_tree_entry *base = __get_path_tree_entry(fullPath, &unlock_stack, lock_path_tree_entry_with_ht, false) ;
    
    do {
        path_tree_entry *entry = base ;

        if (base == &ROOT) return ;

        if (list_empty(&base->children) && list_empty(&base->pts)) {
            base = base->parent ;
            list_del(&entry->siblings) ;
            if (entry->dentry) {
                dput(entry->dentry) ;
                list_del(&entry->overflow_list) ;
            }
            kfree(entry->name.name) ;
            kfree(entry) ;
        } else {
            unlock_path_tree_entry(entry) ;
        }

    } while (1) ;
}

void init_path_tree(void) {
    struct path root ;

    for (int i = 0; i < MODULUS; i++) {
        INIT_LIST_HEAD(&dentry_table.records[i].overflow_list) ;
        init_rwsem(&dentry_table.records[i].sem) ;
    }
    get_fs_root(current->fs, &root) ;

    INIT_PATH_TREE_ENTRY(&ROOT, root.dentry) ;

    path_put(&root) ;

    return ;
}

void cleanup_path_tree(void) {
    CLEANUP_PATH_TREE_ENTRY(&ROOT) ;
}