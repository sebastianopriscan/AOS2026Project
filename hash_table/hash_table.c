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

struct hash_table {
    struct list_head overflow_lists[MODULUS] ;
} ;

struct hash_table TABLE ;

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
        INIT_LIST_HEAD(&TABLE.overflow_lists[MODULUS]) ;
    }
}

int hash_table_insert(throttleA_policy *policy) {

    policy_with_table *pt = kmalloc(sizeof(policy_with_table), GFP_KERNEL) ;
    if (pt == NULL) {
        return 1 ;
    }
    memcpy(&pt->policy, policy, sizeof(throttleA_policy)) ;

    int idx = evaluate_hash(policy->uid, policy->path.pathName) ;
    struct list_head *list = &TABLE.overflow_lists[idx];
    list_add(list, &pt->hash_head) ;
    return 0 ;
}

int hash_table_remove(throttleA_policy *policy) {
    int idx = evaluate_hash(policy->uid, policy->path.pathName) ;
    struct list_head *list = &TABLE.overflow_lists[idx] ;

    struct list_head *pos ;
    list_for_each(pos, list) {
        policy_with_table *table = list_entry(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(policy->path.pathName, table->policy.path.pathName) == 0 ;
        if (uid_condition && path_condition) {
            list_del(pos) ;
            kfree(table) ;
            break ;
        }
    }

    return 0 ;
}

policy_with_table *hash_table_get(throttleA_policy *policy) {
    int idx = evaluate_hash(policy->uid, policy->path.pathName) ;
    struct list_head *list = &TABLE.overflow_lists[idx] ;

    struct list_head *pos ;
    list_for_each(pos, list) {
        policy_with_table *table = list_entry(pos, policy_with_table, hash_head) ;
        const int uid_condition = policy->uid == table->policy.uid ;
        const int path_condition = strcmp(policy->path.pathName, table->policy.path.pathName) == 0 ;
        if (uid_condition && path_condition) {
            return table ;
        }
    }

    return NULL ;
}