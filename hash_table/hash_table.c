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

#include "include/hash_table/hash_table.h"

#define MODULUS 193

#define table_from_policy(table, policy) \
do {\
    switch (policy) { \
        case POLICY_UID_ONLY: \
            table = &UID_TABLE ; \
            break ; \
        case POLICY_PROGRAM_ONLY: \
            table = &PROGRAM_TABLE ; \
            break ; \
        case POLICY_UID_AND_PROGRAM: \
            table = &PROGRAM_UID_TABLE ; \
            break ; \
        default: \
            return -ENOKEY ; \
    } \
} while (0) \

struct hash_table_record {
    struct list_head overflow_list ;
    struct rw_semaphore sem ;
} ;

struct hash_table {
    struct hash_table_record records[MODULUS] ;
} ;

static struct hash_table UID_TABLE, PROGRAM_TABLE, PROGRAM_UID_TABLE ;

static inline int evaluate_hash(int uid, char *name) {
    int hash = uid ;
    const int len = strlen(name) ;
    for (int i = 0; i < len; i += sizeof(int)) {
        int number = 0;
        for (int j = 0; j < sizeof(int) ; j++) {
            number |= ((int) name[i+j] << (8*j)) ;
        }
        hash ^= number ;
    }

    return hash % MODULUS ;
}

void init_hash_table(void) {
    for (int i = 0; i < MODULUS; i++) {
        INIT_LIST_HEAD(&PROGRAM_UID_TABLE.records[i].overflow_list) ;
        init_rwsem(&PROGRAM_UID_TABLE.records[i].sem) ;
        INIT_LIST_HEAD(&UID_TABLE.records[i].overflow_list) ;
        init_rwsem(&UID_TABLE.records[i].sem) ;
        INIT_LIST_HEAD(&PROGRAM_TABLE.records[i].overflow_list) ;
        init_rwsem(&PROGRAM_TABLE.records[i].sem) ;
    }
}

int hash_table_insert(throttleA_policy *policy) {
    struct hash_table *table;
    table_from_policy(table, policy->policy) ;

    policy_with_table *pt = kmalloc(sizeof(policy_with_table), GFP_KERNEL) ;
    if (pt == NULL) {
        return 1 ;
    }
    memcpy(&pt->policy, policy, sizeof(throttleA_policy)) ;
    atomic_long_set(&pt->throttle_counter, 0) ;
    atomic_long_set(&pt->isActive, 1) ;

    int idx = evaluate_hash(policy->uid, policy->path.pathName) ;
    struct list_head *list = &table->records[idx].overflow_list;
    struct rw_semaphore *sem = &table->records[idx].sem ;
    down_write(sem) ;
    list_add_rcu(list, &pt->hash_head) ;
    list_add(list, &pt->hash_head) ;
    up_write(sem) ;

    return 0 ;
}

int hash_table_remove(throttleA_policy *policy) {
    struct hash_table *table;
    table_from_policy(table, policy->policy) ;

    int idx = evaluate_hash(policy->uid, policy->path.pathName) ;
    struct list_head *list = &table->records[idx].overflow_list ;
    struct rw_semaphore *sem = &table->records[idx].sem ;

    struct list_head *pos ;
    list_for_each(pos, list) {
        policy_with_table *table = list_entry(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(policy->path.pathName, table->policy.path.pathName) == 0 ;
        if (uid_condition && path_condition) {
            down_write(sem) ;
            atomic_xchg(&table->isActive,0) ;
            list_del_rcu(pos) ;
            up_write(sem) ;
            synchronize_rcu() ;
            kfree(table) ;
            break ;
        }
    }

    return 0 ;
}

static policy_with_table *hash_table_try_get(policy_kind policy, uid_t uid, const char *pathName) {
    struct hash_table *table;
    table_from_policy(table, policy) ;

    int idx = evaluate_hash(uid, pathName) ;
    struct list_head *list = &table->records[idx].overflow_list ;

    struct list_head *pos ;
    list_for_each_rcu(pos, list) {
        policy_with_table *table = list_entry_rcu(pos, policy_with_table, hash_head) ;
        const int uid_condition = uid == table->policy.uid ;
        const int path_condition = strcmp(pathName, table->policy.path.pathName) == 0 ;
        if (uid_condition && path_condition) {
            rcu_read_lock() ;
            return table ;
        }
    }

    return NULL ;
}

policy_with_table *hash_table_get(uid_t uid, const char *pathName) {
    policy_with_table *retVal = hash_table_try_get(POLICY_UID_AND_PROGRAM, uid, pathName) ;
    if (retVal) return retVal;

    retVal = hash_table_try_get(POLICY_PROGRAM_ONLY, uid, pathName) ;
    if (retVal) return retVal;

    return hash_table_try_get(POLICY_UID_ONLY, uid, pathName) ;
}

void hash_table_put(void) {
    rcu_read_unlock() ;
}

void hash_table_refresh(void) {
    for (int i = 0; i < MODULUS; i++) {
        struct list_head *pos ;
        list_for_each_rcu(pos, &PROGRAM_UID_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
        list_for_each_rcu(pos, &UID_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
        list_for_each_rcu(pos, &PROGRAM_TABLE.records[i].overflow_list) {
            atomic_long_xchg(&list_entry_rcu(pos, policy_with_table, hash_head)->throttle_counter, 0) ;
        }
    }
}
