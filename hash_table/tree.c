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

/**
 * Bumped by every insert/remove that gets past PT_LOCK, so a dumper that
 * lost the lock can tell if its handle may still point to live entries
 */
static atomic_long_t PT_GEN = ATOMIC_LONG_INIT(0) ;

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
        if (!list_empty(&entry->children)) {
            entry = container_of((&entry->children)->next, path_tree_entry, siblings) ;
        } else {
            if (entry != root) {
                path_tree_entry *toDelete = entry ;
                entry = entry->parent ;
                list_del_rcu(&toDelete->siblings) ;
                kfree(toDelete->name.name) ;
                kfree(toDelete) ;
            }
        }
    } while (entry != root || !list_empty(&root->children)) ;
}

static inline bool valid_path(const char *path) {
    if (path[0] != '/') return false ;
    for (; *path ; path++) if (path[0] == '/' && (path[1] == '/' || path[1] == '\0')) return false ;
    return true ;
}

static inline void remove_path_tree_entry_by_entry(path_tree_entry *entry) {
    path_tree_entry *base = entry ;
    struct list_head *pos, *tmp;
    LIST_HEAD(remove_queue) ;

    do {
        if (base == &ROOT) break ;

        if (list_empty(&base->children) && base->entry_status == PATH_TREE_ENTRY_INACTIVE) {
            path_tree_entry *toRemove = base ;
            base = base->parent ;
            list_del_rcu(&toRemove->siblings) ;
            list_add_tail(&toRemove->remove_list, &remove_queue) ;
            unlock_path_tree_entry(toRemove) ;
        } else break ;

    } while (1) ;

    do {
        bool isRoot = base == &ROOT ;
        unlock_path_tree_entry(base) ;
        if (isRoot) break ;
        base = base->parent ;
    } while (1) ;

    synchronize_rcu() ;

    list_for_each_safe(pos, tmp, &remove_queue) {
        kfree(container_of(pos, path_tree_entry, remove_list)->name.name) ;
        kfree(container_of(pos, path_tree_entry, remove_list)) ;
    }

    return ;
}

int remove_path_tree_entry(char *fullPath) {
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT, *toRemove ;

    if (!valid_path(fullPath)) return -EINVAL ;
    if (!down_read_trylock(&PT_LOCK)) return -EBUSY ;
    atomic_long_inc(&PT_GEN) ;
    lock_path_tree_entry(&ROOT) ;

    do {
        path_tree_entry *entry ;
        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                lock_path_tree_entry(entry) ;
                base = entry ;
                goto incr_step;
            }
        }
        goto err_step ;

incr_step:
        pathPtr += slashlen(pathPtr) ;

        if (*pathPtr == '\0') {
            if (base->entry_status != PATH_TREE_ENTRY_ACTIVE) goto err_step ;
            toRemove = base ;
            break ;
        }
        pathPtr++ ;
        continue ;

err_step:
        unlock_all_entries(base) ;
        up_read(&PT_LOCK) ;
        return -ENOENT ;
    } while (1) ;


    toRemove->entry_status = PATH_TREE_ENTRY_INACTIVE ;
    remove_path_tree_entry_by_entry(toRemove) ;
    up_read(&PT_LOCK) ;
    return 0 ;
}

static void rollback_path_tree_entry(char *fullPath) {
    char *pathPtr = fullPath +1 ;
    path_tree_entry *base = &ROOT ;

    lock_path_tree_entry(&ROOT) ;

    do {
        path_tree_entry *entry ;
        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                lock_path_tree_entry(entry) ;
                base = entry ;
                goto incr_step;
            }
        }
        break ;

incr_step:
        pathPtr += slashlen(pathPtr) ;

        if (*pathPtr == '\0') break ;
        pathPtr++ ;
    } while (1) ;

    remove_path_tree_entry_by_entry(base) ;
}

int insert_path_tree_entry(char *fullPath) {
    char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;

    if (!valid_path(fullPath)) return -EINVAL ;
    if (!down_read_trylock(&PT_LOCK)) return -EBUSY ;
    atomic_long_inc(&PT_GEN) ;
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
        name = kzalloc(nameLen +1, GFP_KERNEL) ;
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
                unlock_path_tree_entry(newEntry) ;
                unlock_path_tree_entry(newEntry->parent) ;
                up_read(&PT_LOCK) ;
                return 0 ;
            }
            base->entry_status = PATH_TREE_ENTRY_ACTIVE ;
            unlock_path_tree_entry(base) ;
            if (base != base->parent) unlock_path_tree_entry(base->parent) ;
            up_read(&PT_LOCK) ;
            return 0 ;
        }
        pathPtr++ ;

    } while (1) ;

free_allocations:
    unlock_path_tree_entry(base) ;
    if (base->parent != base) unlock_path_tree_entry(base->parent) ;
    rollback_path_tree_entry(fullPath) ;

    up_read(&PT_LOCK) ;
    return -ENOMEM ;
}

bool path_tree_has(const char *fullPath) {
    const char *pathPtr = fullPath +1;
    path_tree_entry *base = &ROOT ;

    rcu_read_lock() ;
    do {
        path_tree_entry *entry, *next = NULL ;
        list_for_each_entry_rcu(entry, &base->children, siblings) {
            if (check_entry_path(&entry->name, pathPtr)) {
                next = entry ;
                break ;
            }
        }

        if (!next) {
            rcu_read_unlock() ;
            return false ;
        }

        base = next ;
        pathPtr += slashlen(pathPtr) ;
        if (*pathPtr == '\0') {
            bool retval = base->entry_status == PATH_TREE_ENTRY_ACTIVE ;
            rcu_read_unlock() ;
            return retval ;
        }
        pathPtr++ ;
    } while (1) ;
}

int path_tree_lock(void) {
    return down_write_trylock(&PT_LOCK) ? 0 : 1 ;
}

void path_tree_unlock(void) {
    up_write(&PT_LOCK) ;
}

unsigned long path_tree_generation(void) {
    return atomic_long_read(&PT_GEN) ;
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

static inline int keep_reading(char *kern_buf, size_t len) {
    int remaining, parsedlen, toWrite, written, bufIdx = 0 ;

    parsedlen = handle.curr->name.len + handle.depth + 3 ;
    remaining = parsedlen - handle.state ;

    if (!remaining) {
        do {
            handle.state = 0 ;
            if (handle.curr == &ROOT && (handle.children_visited || list_empty(&ROOT.children))) {
                handle.children_visited = true ;
                return 0 ;
            }
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
                    handle.curr = handle.curr->parent ;
                    handle.children_visited = true ;
                }
            }
        } while (1) ;
    }

    parsedlen = handle.curr->name.len + handle.depth + 3 ;
    remaining = parsedlen - handle.state ;
    written = toWrite = umin(remaining, len) ;

    for (; handle.state < handle.depth && toWrite > 0 ; handle.state++, toWrite--, bufIdx++) kern_buf[bufIdx] = '\t' ;

    if ((handle.state - handle.depth) < handle.curr->name.len && toWrite > 0) {
        int writablepath = (handle.curr->name.len - (handle.state - handle.depth)) ;
        int internalToWrite = min(writablepath, toWrite) ;
        memcpy(kern_buf + bufIdx, handle.curr->name.name + handle.state - handle.depth, internalToWrite) ;
        toWrite -= internalToWrite ;
        handle.state += internalToWrite ;
        bufIdx += internalToWrite ;
    }

    if (toWrite && handle.state == handle.depth + handle.curr->name.len) {
        kern_buf[bufIdx] = ' ' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    if (toWrite && handle.state == handle.depth + handle.curr->name.len + 1) {
        kern_buf[bufIdx] = handle.curr->entry_status == PATH_TREE_ENTRY_ACTIVE ? 'x' : ' ' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    if (toWrite && handle.state == handle.depth + handle.curr->name.len + 2) {
        kern_buf[bufIdx] = '\n' ;
        toWrite-- ;
        bufIdx++ ;
        handle.state++ ;
    }

    return written ;
}

ssize_t pt_file_handle_read(char __user *buf, ssize_t len) {
    int read = 0, cum = 0, missing, toRead ;
    char *kern_buf ;

    if ((handle.curr == &ROOT && handle.children_visited)) return 0 ;

    kern_buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;
    if (IS_ERR_OR_NULL(kern_buf)) return -ENOMEM ;

    do {
        toRead = umin(PAGE_SIZE, len - cum) ;
        read = keep_reading(kern_buf, toRead) ;
        if (read == 0) {
            kfree(kern_buf) ;
            return cum ;
        }
        missing = copy_to_user(buf + cum, kern_buf, read) ;
        if (missing) {
            kfree(kern_buf) ;
            handle.state -= missing ;
            return (cum + read - missing) ? cum + read - missing : -EFAULT ;
        }
        cum += read ;
    } while (cum < len) ;

    kfree(kern_buf) ;
    return cum ;
}

void reset_pt_file_handle(void) {
    handle.curr = &ROOT ;
    handle.children_visited = false ;
    handle.depth = 0 ;
    handle.state = 0 ;
}