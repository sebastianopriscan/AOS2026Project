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
#include "include/utils/seeds.h"
#include "include/utils/strings.h"


/**
 * Object that manages the read state for the UID hash table
 */
struct pt_file_handle {
    path_tree_entry *curr ;
    int state ;

    int depth ;
    bool children_visited ;

} ;

static struct pt_file_handle handle ;

static struct rw_semaphore PT_LOCK ;

path_tree_entry ROOT ;

static inline void lock_path_tree_entry(path_tree_entry *entry) {
    mutex_lock(&entry->entry_mutex) ;
}

static inline void unlock_path_tree_entry(path_tree_entry *entry) {
    mutex_unlock(&entry->entry_mutex) ;
}

static inline void unlock_all_entries(path_tree_entry *entry) {
    path_tree_entry *base = entry ;
    do {
        unlock_path_tree_entry(base) ;
        if (base == &ROOT) return ;
        base = base->parent ;
    } while (1) ;
}

static inline bool check_entry_path(const struct qstr *name, const char *pathComponent) {
    u32 hash = full_name_hash((void *)(FULL_RANDOM), pathComponent, slashlen(pathComponent)) ;
    return hash == name->hash && slashcmp(name->name, pathComponent) == 0 ;
}

static inline void INIT_PATH_TREE_ENTRY(path_tree_entry *entry) {
    entry->parent = entry ; 
    INIT_LIST_HEAD(&entry->children) ; 
    INIT_LIST_HEAD(&entry->siblings) ; 
    entry->name.len = 1 ; 
    entry->name.name = "/" ; 
    entry->entry_status = PATH_TREE_ENTRY_ACTIVE ; 
    mutex_init(&entry->entry_mutex) ;
}

static inline void CLEANUP_PATH_TREE_ENTRY(path_tree_entry *root) {

    path_tree_entry *entry = root;

    do {
        if (!list_is_head((&entry->children)->next, &entry->children)) {
            entry = container_of((&entry->children)->next, path_tree_entry, siblings) ;
        } else {
            if (entry != root) {
                path_tree_entry *toDelete = entry ;
                entry = entry->parent ;
                list_del_rcu(&toDelete->siblings) ;
                kfree(toDelete) ;
            }
        }
    } while (entry != root) ;
}

static inline void remove_path_tree_entry_by_entry(path_tree_entry *entry) {
    entry->entry_status = PATH_TREE_ENTRY_INACTIVE ;
    path_tree_entry *base = entry ;
    struct list_head remove_queue, *pos, *tmp;
    
    do {
        if (base == &ROOT) break ;

        if (list_empty(&base->children) && base->entry_status == PATH_TREE_ENTRY_INACTIVE) {
            path_tree_entry *toRemove = base ;
            base = base->parent ;
            list_del_rcu(&toRemove->siblings) ;
            list_add_tail(&toRemove->remove_list, &remove_queue) ;
        } else break ;

    } while (1) ;

    do {
        if (base == &ROOT) break ;
        unlock_path_tree_entry(base) ;
        base = base->parent ;
    } while (1) ;

    synchronize_rcu() ;

    pos = remove_queue.next ;
    do {
        tmp = pos ;
        pos = pos->next ;

        list_del(tmp) ;
        kfree(container_of(tmp, path_tree_entry, remove_list)->name.name) ;
        kfree(container_of(tmp, path_tree_entry, remove_list)) ;
    } while (!list_is_head(pos, &remove_queue)) ;

    return ;
}

int remove_path_tree_entry(char *fullPath) {
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT, *toRemove ;

    if (!down_read_trylock(&PT_LOCK)) return -EBUSY ;
    lock_path_tree_entry(&ROOT) ;

    do {
        path_tree_entry *entry ;
        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                lock_path_tree_entry(entry) ;
                base = entry ;
                goto incr_step;
            } else {
                goto err_step;
            }
        }

incr_step:
        pathPtr += slashlen(pathPtr) ;

        if (*pathPtr == '\0' && base->entry_status == PATH_TREE_ENTRY_ACTIVE) {
            toRemove = base ;
            break ;
        }
        else goto err_step ;
        pathPtr++ ;
        continue ;

err_step:
        unlock_all_entries(base) ;
        up_read(&PT_LOCK) ;
        return -ENOENT ;
    } while (1) ;


    remove_path_tree_entry_by_entry(toRemove) ;
    up_read(&PT_LOCK) ;
    return 0 ;
}

int insert_path_tree_entry(char *fullPath) {
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;

    if (!down_read_trylock(&PT_LOCK)) return -EBUSY ;
    lock_path_tree_entry(&ROOT) ;

    do {
        path_tree_entry *newEntry = NULL ;
        path_tree_entry *entry ;
        char *name ;
        int nameLen ;

        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                if (base->parent != base) {
                    unlock_path_tree_entry(base->parent) ;
                }
                lock_path_tree_entry(entry) ;
                base = entry ;
                goto incr_step;
            }
        }

        newEntry = kmalloc(sizeof(path_tree_entry), GFP_KERNEL) ;
        if (IS_ERR_OR_NULL(newEntry)) goto free_allocations ;
        nameLen = slashlen(pathPtr) ;
        name = kmalloc(nameLen, GFP_KERNEL) ;
        if (IS_ERR_OR_NULL(name)) {
            kfree(newEntry) ;
            goto free_allocations ;
        }
        memcpy(name, pathPtr, nameLen) ;

        INIT_LIST_HEAD(&newEntry->children) ;

        newEntry->entry_status = PATH_TREE_ENTRY_INACTIVE ;
        newEntry->name.name = name ;
        newEntry->name.len = nameLen ;
        newEntry->name.hash = full_name_hash((void *)(FULL_RANDOM), name, nameLen) ;

        mutex_init(&newEntry->entry_mutex) ;
        lock_path_tree_entry(newEntry) ;

        newEntry->parent = base ;
        list_add_rcu(&newEntry->siblings, &base->children) ;

        if (base->parent != base) unlock_path_tree_entry(base->parent) ;

        base = newEntry ;

incr_step:
        pathPtr += slashlen(pathPtr) ;

        if (*pathPtr == '\0') {
            if (newEntry) {
                newEntry->entry_status = PATH_TREE_ENTRY_ACTIVE ;
                up_read(&PT_LOCK) ;
                return 0 ;
            }
            base->entry_status = PATH_TREE_ENTRY_ACTIVE ;
            up_read(&PT_LOCK) ;
            return 0 ;
        }
        pathPtr++ ;

    } while (1) ;

free_allocations:
    unlock_path_tree_entry(base) ;
    if (base->parent != base) unlock_path_tree_entry(base->parent) ;
    remove_path_tree_entry(fullPath) ;

    up_read(&PT_LOCK) ;
    return -ENOMEM ;
}

bool path_tree_has(const char *fullPath) {
    const char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;

    rcu_read_lock() ;
    do {
        path_tree_entry *entry ;
        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                base = entry ;
                pathPtr += slashlen(pathPtr) ;
                if (*pathPtr == '\0' && base->entry_status == PATH_TREE_ENTRY_ACTIVE) {
                    rcu_read_unlock() ;
                    return true ;
                }
                pathPtr++ ;
                break ;
            } else {
                rcu_read_unlock() ;
                return false ;
            }
        }
    } while (1) ;
}

int path_tree_lock(void) {
    return down_write_trylock(&PT_LOCK) ? 0 : 1 ;
}

void path_tree_unlock(void) {
    up_write(&PT_LOCK) ;
}

void init_path_tree(void) {
    init_rwsem(&PT_LOCK) ;
    INIT_PATH_TREE_ENTRY(&ROOT) ;
    return ;
}

void cleanup_path_tree(void) {
    CLEANUP_PATH_TREE_ENTRY(&ROOT) ;
    return ;

}

static inline int keep_reading(char __user *buf, ssize_t len) {
    int remaining, parsedlen, toWrite, written, bufIdx = 0 ;

    parsedlen = handle.curr->name.len + handle.depth + 3 ;

    remaining = parsedlen - handle.state ;

    if (!remaining) {
        do {
            handle.state = 0 ;
            if (!list_empty(&handle.curr->children) && !handle.children_visited) {
                handle.depth++ ;
                handle.curr = container_of(handle.curr->children.next, path_tree_entry, siblings) ;
                break ;
            } else {
                if (!list_is_last(&handle.curr->siblings, &handle.curr->parent->children)) {
                    handle.curr = container_of(handle.curr->siblings.next, path_tree_entry, siblings) ;
                    handle.children_visited = false ;
                    break ;
                } else {
                    handle.depth-- ;
                    if (handle.depth == 0) {
                        return 0 ;
                    }
                    handle.curr = handle.curr->parent ;
                    handle.children_visited = true ;
                }
            }
        } while (1) ;
    }

    written = toWrite = min(remaining, len) ;

    for (; handle.state < handle.depth && toWrite > 0 ; handle.state++, toWrite--, bufIdx++) buf[bufIdx] = '\t' ;

    if ((handle.state - handle.depth) < handle.curr->name.len && toWrite > 0) {
        int writablepath = (handle.curr->name.len - (handle.state - handle.depth)) ;
        int internalToWrite = min(writablepath, toWrite) ;
        copy_to_user(buf + bufIdx, handle.curr->name.name + handle.state - handle.depth, internalToWrite) ;
        toWrite -= internalToWrite ;
        handle.state += internalToWrite ;
        bufIdx += internalToWrite ;
    }

    if (toWrite) {
        buf[bufIdx] = ' ' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    if (toWrite) {
        buf[bufIdx] = handle.curr->entry_status == PATH_TREE_ENTRY_ACTIVE ? 'x' : ' ' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    if (toWrite) {
        buf[bufIdx] = '\n' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    return written ;
}

ssize_t pt_file_handle_read(char __user *buf, ssize_t len) {
    int read = 0, cum = 0 ;

    if ((handle.curr == &ROOT && handle.children_visited)) return 0 ;

    do {
        read = keep_reading(buf + cum, len - cum) ;
        cum += read ;
    } while (cum < len || read == 0) ;

    return cum ;
}

void reset_pt_file_handle(void) {
    handle.curr = &ROOT ;
    handle.children_visited = false ;
    handle.depth = 0 ;
    handle.state = 0 ;
}