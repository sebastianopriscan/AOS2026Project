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
#include "include/utils/seeds.h"

struct hash_table_record {
    struct list_head overflow_list ;
    struct rw_semaphore sem ;
} ;

struct hash_table {
    struct hash_table_record records[MODULUS] ;
} ;

static struct hash_table UID_TABLE ;

typedef struct _uid_record {
    struct list_head overflow_list ;
    uid_t uid ;
} uid_record ;

static inline unsigned long evaluate_uid_hash(uid_t uid) {
    bool flipped = uid % 2 == 0 ;
    unsigned long first  = (flipped ? FIRST_HALF : SECOND_HALF) * uid ;
    unsigned long second = (flipped ? SECOND_HALF : FIRST_HALF) * uid ;
    unsigned long hash   = (first << 32) | ((unsigned long) 0xFFFFFFFF & SECOND_HALF) ;
    return hash % MODULUS ;
}

void init_hash_table(void) {
    for (int i = 0; i < MODULUS; i++) {
        INIT_LIST_HEAD(&UID_TABLE.records[i].overflow_list) ;
        init_rwsem(&UID_TABLE.records[i].sem) ;
    }

    return ;
}

int hash_table_insert_uid(uid_t uid) {
    struct hash_table_record *ht_record ;
    struct list_head *list ;
    struct rw_semaphore *sem ;
    uid_record *record ;

    ht_record = &(&UID_TABLE)->records[evaluate_uid_hash(uid)] ;
    list = &ht_record->overflow_list;
    sem = &ht_record->sem ;
    down_write(sem) ;
    list_for_each_entry_rcu(record, list, overflow_list, uid_record) {
        if (record->uid == uid) {
            up_write(sem) ;
            return -EEXIST ;
        }
    }

    record = kmalloc(sizeof(uid_record), GFP_KERNEL) ;
    if (record == NULL) {
        up_write(sem) ;
        return -ENOMEM ;
    }

    record->uid = uid ;
    list_add_rcu(&record->overflow_list, list) ;

    up_write(sem) ;

    return 0 ;
}

int hash_table_remove_uid(uid_t uid) {
    struct hash_table_record *ht_record ;
    struct list_head *list ;
    struct rw_semaphore *sem ;
    uid_record *record, *found = NULL;

    ht_record = &(&UID_TABLE)->records[evaluate_uid_hash(uid)] ;
    list = &ht_record->overflow_list;
    sem = &ht_record->sem ;
    down_write(sem) ;
    list_for_each_entry_rcu(record, list, overflow_list, uid_record) {
        if (record->uid == uid) {
            found = record ;
        }
    }

    if (found) {
        list_del_rcu(&found->overflow_list) ;
        synchronize_rcu() ;
        kfree(found) ;
        up_write(sem) ;
        return 0 ;
    }

    up_write(sem) ;
    return -ENOENT ;
}

static inline void clean_uid_ht_overflow_list(struct hash_table_record *record, struct list_head *freeList) {
    struct list_head *pos, *tmp ;
    down_write(&record->sem) ;
    pos = rcu_dereference(record->overflow_list.next) ;
    do {
        tmp = pos ;
        pos = rcu_dereference(pos->next) ;
        list_del_rcu(tmp) ;
        list_add(tmp, freeList) ;
    } while (!list_is_head(pos, &record->overflow_list)) ;
    up_write(&record->sem) ;
}

void cleanup_hash_table(void) {
    struct list_head *pos, *tmp, uid_free_list ;
    INIT_LIST_HEAD(&uid_free_list) ;

    clean_ht_sys_mirror() ;

    for (int i = 0; i < MODULUS; i++) {
        clean_uid_ht_overflow_list(&UID_TABLE.records[i], &uid_free_list) ;
        //clean_ht_overflow_list(&PROGRAM_TABLE.records[i], &free_list) ;
    }

    synchronize_rcu() ;

    pos = uid_free_list.next ;
    do {
        tmp = pos ;
        pos = pos->next ;

        list_del(tmp) ;
        kfree(container_of(tmp, uid_record, overflow_list)) ;
    } while (!list_is_head(pos, &uid_free_list)) ;
}

static bool hash_table_has(uid_t uid) {
    int idx ;
    struct list_head *list, *pos ;
    uid_record *record ;

    idx = evaluate_uid_hash(uid) ;
    list = &(&UID_TABLE)->records[idx].overflow_list ;

    rcu_read_lock() ;

    if (uid >= 0) {
        list_for_each_entry_rcu(record, list, overflow_list, uid_record) {
            if (uid == record->uid) {
                true ;
            }
        }
    }

    rcu_read_unlock() ;

    return NULL ;
}
