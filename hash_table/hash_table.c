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

#include "include/sys_mirror/sys_mirror.h"
#include "include/hash_table/tree.h"
#include "include/hash_table/hash_table.h"
#include "include/utils/unlock.h"

#define table_from_policy(table, policy) \
do {\
    switch (policy) { \
        case POLICY_UID_ONLY: \
            table = &UID_TABLE ; \
            break ; \
        case POLICY_PROGRAM_ONLY: \
            table = &PROGRAM_HANDLES_TABLE ; \
            break ; \
        case POLICY_UID_AND_PROGRAM: \
            table = &PROGRAM_UID_HANDLES_TABLE ; \
            break ; \
        default: \
            table = NULL ; \
            break ; \
    } \
} while (0) \

static atomic_long_t ID_POOL = { .counter = 0 };

static struct hash_table UID_TABLE, PROGRAM_HANDLES_TABLE, PROGRAM_UID_HANDLES_TABLE ;

static bool list_contains(struct list_head *el, struct list_head *list) {
    struct list_head *pos ;
    list_for_each(pos, list) {
        if (pos == el) return true ;
    }
    return false ;
}

static inline int evaluate_hash(int uid, const inode_descriptor *descriptor) {
    int hash ;
    if (descriptor == NULL) return uid % MODULUS;
    hash = uid ^ (descriptor->device_id << 16) ^ ~descriptor->inode_number ;
    return hash % MODULUS ;
}

static void recalculate_program_based_policy(policy_with_table *table) {
    unsigned long syscalls[DATA_PER_LIMIT(unsigned long)] = {0};
    unsigned long tolerance = ULONG_MAX ;
    struct list_head *pos ;
    path_with_table *pathTable ;
    int i ;
    
    list_for_each(pos, &table->bound_paths) {
        pathTable = list_entry(pos, path_with_table, handle_list) ;
        tolerance = umin(tolerance, atomic_read(&pathTable->policy.tolerance)) ; 
        for(i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
            syscalls[i] |= atomic_long_read(&pathTable->policy.syscalls[i]) ;
        }
    }

    atomic_xchg(&table->policy.tolerance, tolerance) ;
    for (i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
        atomic_long_xchg(&table->policy.syscalls[i], syscalls[i]) ;
    }

    return ;
}

int init_hash_table(void) {
    init_path_tree() ;

    for (int i = 0; i < MODULUS; i++) {
        INIT_LIST_HEAD(&PROGRAM_UID_HANDLES_TABLE.records[i].overflow_list) ;
        init_rwsem(&PROGRAM_UID_HANDLES_TABLE.records[i].sem) ;
        INIT_LIST_HEAD(&UID_TABLE.records[i].overflow_list) ;
        init_rwsem(&UID_TABLE.records[i].sem) ;
        INIT_LIST_HEAD(&PROGRAM_HANDLES_TABLE.records[i].overflow_list) ;
        init_rwsem(&PROGRAM_HANDLES_TABLE.records[i].sem) ;
    }

    return init_ht_sys_mirror() ;
}

static int hash_table_insert_by_uid(throttleA_policy *policy) {
    struct hash_table *table = &UID_TABLE;
    int idx, sysFsRet ;
    struct list_head *list, *pos ;
    struct rw_semaphore *sem ;
    policy_with_table *pt ;
    char hit = 0 ;

    idx = evaluate_hash(policy->uid, NULL) ;
    list = &table->records[idx].overflow_list;
    sem = &table->records[idx].sem ;
    list_for_each_rcu(pos, list) {
        policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        if (uid_condition) {

            for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
                atomic_long_or(policy->syscalls[i], &table->policy.syscalls[i]) ;
            }

            if (policy->tolerance != 0) {
                atomic_xchg(&table->policy.tolerance, policy->tolerance) ;
            }

            hit = 1 ;
            break ;
        }
    }

    if (hit) return 0 ;

    down_write(sem) ;

    pt = kmalloc(sizeof(policy_with_table), GFP_KERNEL) ;
    if (pt == NULL) {
        up_write(sem) ;
        return 1 ;
    }
    pt->policy.policy = policy->policy ;
    for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
        atomic_long_set(&pt->policy.syscalls[i], policy->syscalls[i]) ;
    }
    atomic_set(&pt->policy.tolerance, policy->tolerance) ;
    pt->policy.uid = policy->uid ;

    atomic_long_set(&pt->throttle_counter, 0) ;
    atomic_set(&pt->isActive, 1) ;

    list_add_rcu(&pt->hash_head,list) ;

    sysFsRet = sys_mirror_policy_add(pt) ;
    if (sysFsRet) {
        list_del_rcu(&pt->hash_head) ;        
        kfree(pt) ;
        up_write(sem) ;
        return sysFsRet ;
    }

    up_write(sem) ;

    return 0 ;
}

path_with_table *hash_table_insert_desc(throttleA_policy *policy, path_decree *decree, struct list_head *unlock_stack) {
    struct list_head *list, *pos ;
    path_with_table *path_table, *retVal = NULL ;
    path_tree_entry *entry ;

    entry = get_path_tree_entry(decree->path_ptr, unlock_stack) ;
    list = &entry->pts ;
    list_for_each(pos, list) {
        path_with_table *table = list_entry(pos, path_with_table, overflow_list) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(table->path.pathName, decree->path_ptr) == 0 ;
        if (uid_condition && path_condition) {

            for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
                atomic_long_or(policy->syscalls[i], &table->policy.syscalls[i]) ;
            }

            if (policy->tolerance != 0) {
                atomic_xchg(&table->policy.tolerance, policy->tolerance) ;
            }

            retVal = table ;
            break ;
        }
    }

    if (retVal != NULL) {
        return retVal ;
    }

    path_table = kmalloc(sizeof(path_with_table), GFP_KERNEL) ;
    if (path_table == NULL) {
        return ERR_PTR(-ENOMEM) ;
    }
    memcpy(&path_table->path.pathName, decree->path_ptr, PATH_MAX) ;
    path_table->policy.policy = policy->policy ;
    for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
        atomic_long_set(&path_table->policy.syscalls[i], policy->syscalls[i]) ;
    }
    atomic_set(&path_table->policy.tolerance, policy->tolerance) ;
    path_table->policy.uid = policy->uid ;
    path_table->id = atomic_long_inc_return(&ID_POOL) ;

    if (sys_mirror_path_add(path_table)) {
        kfree(path_table) ;
        return ERR_PTR(-1) ;
    }

    list_add_rcu(&path_table->overflow_list,list) ;

    return path_table ;
}

unsigned long hash_table_remove_desc(throttleA_policy *policy, path_decree *decree, struct list_head *unlock_stack) ;
int hash_table_insert(throttleA_policy *policy, path_decree *decree) {
    struct hash_table *table;
    int idx, sysFsRet ;
    struct list_head *list, *pos, unlock_stack ;
    struct rw_semaphore *sem ;
    policy_with_table *policy_table ;
    path_with_table *path_table ;
    char hit = 0 ;

    table_from_policy(table, policy->policy) ;

    if (table == NULL) return -ENOKEY ;
    if (table == &UID_TABLE) return hash_table_insert_by_uid(policy) ;

    path_table = hash_table_insert_desc(policy, decree, &unlock_stack) ; 
    if (IS_ERR(path_table)) return PTR_ERR(path_table) ;

    if (decree->path_found) {
        idx = evaluate_hash(policy->uid, &decree->descriptor) ;
        list = &table->records[idx].overflow_list;
        sem = &table->records[idx].sem ;
        list_for_each_rcu(pos, list) {
            policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
            const int uid_condition = policy->uid == table->policy.uid ;
            const int inode_condition = decree->descriptor.device_id == table->policy.inode.device_id && decree->descriptor.inode_number == table->policy.inode.inode_number ;
            if (uid_condition && inode_condition) {
                if (!list_contains(&path_table->handle_list, &table->bound_paths)) {
                    list_add(&path_table->handle_list, &table->bound_paths) ;
                    atomic_inc(&table->refCount) ;
                    if (bind_policy_to_path(table, path_table)) {
                        list_del(&path_table->handle_list) ;
                        atomic_dec(&table->refCount) ;

                        perform_unlocking(&unlock_stack) ;
                        hash_table_remove_desc(policy, decree, &unlock_stack) ;
                        perform_unlocking(&unlock_stack) ;
                        return -1 ;
                    }
                }
                recalculate_program_based_policy(table) ;
                hit = 1 ;
                break ;
            }
        }

        if (hit) return 0 ;

        down_write(sem) ;

        policy_table = kmalloc(sizeof(policy_with_table), GFP_KERNEL) ;
        if (policy_table == NULL) {
            up_write(sem) ;
            perform_unlocking(&unlock_stack) ;
            hash_table_remove_desc(policy, decree, &unlock_stack) ;
            perform_unlocking(&unlock_stack) ;
            return 1 ;
        }
        policy_table->policy.inode.device_id = decree->descriptor.device_id ;
        policy_table->policy.inode.inode_number = decree->descriptor.inode_number ;
        policy_table->policy.policy = policy->policy ;
        for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
            atomic_long_set(&policy_table->policy.syscalls[i], policy->syscalls[i]) ;
        }
        atomic_set(&policy_table->policy.tolerance, policy->tolerance) ;
        policy_table->policy.uid = policy->uid ;

        atomic_long_set(&policy_table->throttle_counter, 0) ;
        atomic_set(&policy_table->isActive, 1) ;
        atomic_set(&policy_table->refCount, 1) ;

        sysFsRet = sys_mirror_policy_add(policy_table) ;
        if (sysFsRet) {
            kfree(policy_table) ;
            up_write(sem) ;
            perform_unlocking(&unlock_stack) ;
            hash_table_remove_desc(policy, decree, &unlock_stack) ;
            perform_unlocking(&unlock_stack) ;
            return sysFsRet ;
        }

        if (bind_policy_to_path(policy_table, path_table)) {
            list_del(&path_table->handle_list) ;
            perform_unlocking(&unlock_stack) ;
            hash_table_remove_desc(policy, decree, &unlock_stack) ;
            perform_unlocking(&unlock_stack) ;
            kfree(policy_table) ;
            up_write(sem) ;
            return -1 ;
        }

        list_add_rcu(&policy_table->hash_head,list) ;
        list_add(&path_table->handle_list, &policy_table->bound_paths) ;

        up_write(sem) ;
    }

    perform_unlocking(&unlock_stack) ;

    return 0 ;
}

static void hash_table_remove_by_uid(throttleA_policy *policy) {
    struct hash_table *table = &UID_TABLE;
    int idx ;
    struct list_head *list, *pos ;
    struct rw_semaphore *sem ;

    idx = evaluate_hash(policy->uid, NULL) ;
    list = &table->records[idx].overflow_list ;
    sem = &table->records[idx].sem ;

    down_write(sem) ;
    list_for_each_rcu(pos, list) {
        policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        if (uid_condition) {
            atomic_xchg(&table->isActive,0) ;
            sys_mirror_policy_rm(table) ;
            list_del_rcu(pos) ;
            up_write(sem) ;
            synchronize_rcu() ;
            kfree(table) ;
            break ;
        }
    }

    return ;
}

unsigned long hash_table_remove_desc(throttleA_policy *policy, path_decree *decree, struct list_head *unlock_stack) {
    int retval;
    path_tree_entry *entry ;
    struct list_head *list, *pos ;
    path_with_table *toRemove = NULL ;

    entry = get_path_tree_entry(decree->path_ptr, unlock_stack) ;
    list = &entry->pts ;
    list_for_each(pos, list) {
        path_with_table *table = list_entry(pos, path_with_table, overflow_list) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(table->path.pathName, decree->path_ptr) == 0 ;
        if (uid_condition && path_condition) {
            toRemove = table ;
            break ;
        }
    }
    if (toRemove) {
        list_del(&toRemove->handle_list) ;
        list_del(&toRemove->overflow_list) ;
        sysfs_remove_link(toRemove->child, PATH_TABLE_SYMLINK_NAME) ;
        sys_mirror_path_rm(toRemove) ;
        retval = toRemove->id ;
        kfree(toRemove) ;
        remove_path_tree_entry(decree->path_ptr) ;
        return retval ;
    }
    return 0 ;
}

int hash_table_remove(throttleA_policy *policy, path_decree *decree) {
    struct hash_table *table;
    int idx ;
    unsigned long id ;
    struct list_head *list, *pos, unlock_stack ;
    struct rw_semaphore *sem ;

    table_from_policy(table, policy->policy) ;

    if (table == NULL) return -ENOKEY ;
    if (table == &UID_TABLE) {
        hash_table_remove_by_uid(policy) ;
        return -1 ;
    }

    id = hash_table_remove_desc(policy, decree, &unlock_stack) ;

    if (decree->path_found) {
        idx = evaluate_hash(policy->uid, &decree->descriptor) ;
        list = &table->records[idx].overflow_list ;
        sem = &table->records[idx].sem ;

        down_write(sem) ;
        list_for_each_rcu(pos, list) {
            policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
            const int uid_condition = policy->uid == table->policy.uid ;
            const int inode_condition = decree->descriptor.device_id == table->policy.inode.device_id && decree->descriptor.inode_number == table->policy.inode.inode_number ;
            if (uid_condition && inode_condition) {
                int refcount = atomic_read(&table->refCount) ;
                if (id) unbind_policy(table, id) ;

                if (refcount > 1) {
                    atomic_dec(&table->refCount) ;
                    recalculate_program_based_policy(table) ;
                    break ;
                }

                atomic_xchg(&table->isActive,0) ;
                sys_mirror_policy_rm(table) ;
                list_del_rcu(pos) ;
                up_write(sem) ;
                synchronize_rcu() ;
                kfree(table) ;
                break ;
            }
        }
    }

    perform_unlocking(&unlock_stack) ;

    return 0;
}

int hash_table_delete_by_uid(throttleA_policy *policy) {
    struct hash_table *table = &UID_TABLE;
    int idx ;
    struct list_head *list, *pos ;

    idx = evaluate_hash(policy->uid, NULL) ;
    list = &table->records[idx].overflow_list ;

    list_for_each_rcu(pos, list) {
        policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        if (uid_condition) {
            for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
                atomic_long_andnot(policy->syscalls[i], &table->policy.syscalls[i]) ;
            }
            break ;
        }
    }

    return 0 ;
}

void hash_table_delete_desc(throttleA_policy *policy, path_decree *decree) {
    struct list_head *list, *pos , unlock_stack;
    path_tree_entry *entry ;

    entry = get_path_tree_entry(decree->path_ptr, &unlock_stack);
    list = &entry->pts;
    list_for_each(pos, list) {
        path_with_table *table = list_entry(pos, path_with_table, overflow_list) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(table->path.pathName, decree->path_ptr) == 0 ;
        if (uid_condition && path_condition) {
            for(int i = 0; i < DATA_PER_LIMIT(unsigned long); i++) {
                atomic_long_andnot(policy->syscalls[i], &table->policy.syscalls[i]) ;
            }
            break ;
        }
    }
    perform_unlocking(&unlock_stack) ;
    return ;
}

int hash_table_delete(throttleA_policy *policy, path_decree *decree) {
    struct hash_table *table;
    int idx ;
    struct list_head *list, *pos ;

    table_from_policy(table, policy->policy) ;

    if (table == NULL) return -ENOKEY ;
    if (table == &UID_TABLE) return hash_table_delete_by_uid(policy) ;

    hash_table_delete_desc(policy, decree) ;

    if (decree->path_found) {
        idx = evaluate_hash(policy->uid, &decree->descriptor) ;
        list = &table->records[idx].overflow_list ;

        list_for_each_rcu(pos, list) {
            policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
            const int uid_condition = policy->uid == table->policy.uid ;
            const int inode_condition = decree->descriptor.device_id == table->policy.inode.device_id && decree->descriptor.inode_number == table->policy.inode.inode_number ;
            if (uid_condition && inode_condition) {
                recalculate_program_based_policy(table) ;
            }
        }
    }

    return 0 ;
}

static inline void clean_ht_overflow_list(struct hash_table_record *record, struct list_head *freeList) {
    struct list_head *pos, *tmp ;
    down_write(&record->sem) ;
    pos = rcu_dereference(record->overflow_list.next) ;
    do {
        policy_with_table *table ;
        tmp = pos ;
        pos = rcu_dereference(pos->next) ;

        table = list_entry_rcu(tmp, policy_with_table, hash_head) ;
        atomic_xchg(&table->isActive,0) ;
        list_del_rcu(tmp) ;
        list_add(tmp, freeList) ;
    } while (!list_is_head(pos, &record->overflow_list)) ;
    up_write(&record->sem) ;
}

void cleanup_hash_table(void) {
    struct list_head *pos, *tmp, free_list ;
    INIT_LIST_HEAD(&free_list) ;

    clean_ht_sys_mirror() ;

    for (int i = 0; i < MODULUS; i++) {
        clean_ht_overflow_list(&UID_TABLE.records[i], &free_list) ;
        clean_ht_overflow_list(&PROGRAM_HANDLES_TABLE.records[i], &free_list) ;
        clean_ht_overflow_list(&PROGRAM_UID_HANDLES_TABLE.records[i], &free_list) ;
    }

    synchronize_rcu() ;

    pos = free_list.next ;
    do {
        tmp = pos ;
        pos = pos->next ;

        list_del(tmp) ;
        kfree(tmp) ;
    } while (!list_is_head(pos, &free_list)) ;

    cleanup_path_tree() ;
}

typedef void (*binding_action_t)(policy_with_table *, path_with_table *) ;

void undo_binding(policy_with_table *policy_table, path_with_table *path_table) {
    list_del(&path_table->handle_list) ;
    path_table->bound = false ;
    atomic_dec(&policy_table->refCount) ;
    if (atomic_read(&policy_table->refCount) == 0) {
        atomic_xchg(&policy_table->isActive,0) ;
        sys_mirror_policy_rm(policy_table) ;
        list_del_rcu(&policy_table->hash_head) ;
        synchronize_rcu() ;
        kfree(policy_table) ;
    } else {
        recalculate_program_based_policy(policy_table) ;
    }
}

static policy_with_table *hash_table_try_get(policy_kind policy, uid_t uid, const inode_descriptor *desc) {
    struct hash_table *table;
    int idx ;
    struct list_head *list, *pos ;

    table_from_policy(table, policy) ;
    if (table == NULL) return NULL ;

    idx = evaluate_hash(uid, desc) ;
    list = &table->records[idx].overflow_list ;

    rcu_read_lock() ;
    list_for_each_rcu(pos, list) {
        policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
        const int uid_condition = uid == table->policy.uid ;
        const int inode_condition = desc->device_id == table->policy.inode.device_id && desc->inode_number == table->policy.inode.inode_number ;
        if (uid_condition && inode_condition) {
            return table ;
        }
    }

    rcu_read_unlock() ;
    return NULL ;
}

static policy_with_table *hash_table_get(policy_kind policy, uid_t uid, const inode_descriptor *desc) {
    policy_with_table *policy_table = hash_table_try_get(policy, uid, desc) ;
    if (!policy_table) {
        struct hash_table * table ;
        int idx , sysFsRet;
        struct list_head *list ;
        struct rw_semaphore *sem ;

        table_from_policy(table, policy) ;
        if (table == NULL) return ERR_PTR(-ENOKEY) ;

        idx = evaluate_hash(uid, desc) ;
        list = &table->records[idx].overflow_list;
        sem = &table->records[idx].sem ;
        down_write(sem) ;

        policy_table = kmalloc(sizeof(policy_with_table), GFP_KERNEL) ;
        if (policy_table == NULL) {
            up_write(sem) ;
            return ERR_PTR(1) ;
        }
        policy_table->policy.inode.device_id = desc->device_id ;
        policy_table->policy.inode.inode_number = desc->inode_number ;
        policy_table->policy.policy = policy ;
        policy_table->policy.uid = uid ;

        atomic_long_set(&policy_table->throttle_counter, 0) ;
        atomic_set(&policy_table->isActive, 1) ;
        atomic_set(&policy_table->refCount, 1) ;

        sysFsRet = sys_mirror_policy_add(policy_table) ;
        if (sysFsRet) {
            kfree(policy_table) ;
            up_write(sem) ;
            return ERR_PTR(sysFsRet) ;
        }

        list_add_rcu(&policy_table->hash_head,list) ;

        up_write(sem) ;
    }

    return policy_table ;
}

policy_with_table *hash_table_try_get_all(uid_t uid, const inode_descriptor *desc) {
    policy_with_table *retVal = hash_table_try_get(POLICY_UID_AND_PROGRAM, uid, desc) ;
    if (retVal) return retVal;

    retVal = hash_table_try_get(POLICY_PROGRAM_ONLY, uid, desc) ;
    if (retVal) return retVal;

    return hash_table_try_get(POLICY_UID_ONLY, uid, desc) ;
}

int hash_table_bind_inode(struct list_head *pts, struct inode_descriptor *desc) {

    path_with_table *pt ;

    list_for_each_entry(pt, pts, overflow_list) {
        policy_with_table *policy_table = hash_table_get(pt->policy.policy, pt->policy.uid, desc) ;
        if (bind_policy_to_path(policy_table, pt)) {
            list_del(&pt->handle_list) ;
            kfree(policy_table) ;
            return -1 ;
        }

        list_add(&pt->handle_list, &policy_table->bound_paths) ;
        atomic_inc(&policy_table->refCount) ;
        recalculate_program_based_policy(policy_table) ;
    }

    return 0 ;
}

int hash_table_unbind_inode(struct list_head *pts, struct inode_descriptor *desc) {

    path_with_table *pt ;

    list_for_each_entry(pt, pts, overflow_list) {
        policy_with_table *policy_table = hash_table_try_get(pt->policy.policy, pt->policy.uid, desc) ;
        if (policy_table) {
            unbind_policy(policy_table, pt->id) ;
            unbind_path(pt) ;
            list_del(&pt->handle_list) ;
            atomic_dec(&policy_table->refCount) ;
            if (atomic_read(&policy_table->refCount) == 0) {
                atomic_xchg(&policy_table->isActive,0) ;
                sys_mirror_policy_rm(policy_table) ;
                list_del_rcu(&policy_table->hash_head) ;
                //up_write(sem) ;
                synchronize_rcu() ;
                kfree(policy_table) ;
            } else {
                atomic_dec(&policy_table->refCount) ;
                recalculate_program_based_policy(policy_table) ;
            }
        }
    }

    return 0 ;
}

void hash_table_put(void) {
    rcu_read_unlock() ;
}

void hash_table_refresh(void) {
    for (int i = 0; i < MODULUS; i++) {
        struct list_head *pos ;
        list_for_each_rcu(pos, &PROGRAM_UID_HANDLES_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
        list_for_each_rcu(pos, &UID_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
        list_for_each_rcu(pos, &PROGRAM_HANDLES_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
    }
}
