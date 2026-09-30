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

/**
 * Object that manages the read state for the UID hash table
 */
struct ht_file_handle {
    uid_record *curr ;
    int state ;

    struct list_head *head ;
    int ht_idx ;

    bool eof ;
} ;
static struct ht_file_handle handle ;


static struct rw_semaphore HT_RWLOCK ;

static inline unsigned long evaluate_uid_hash(uid_t uid) {
    bool flipped = uid % 2 == 0 ;
    unsigned long first  = (flipped ? FIRST_HALF : SECOND_HALF) * uid ;
    unsigned long second = (flipped ? SECOND_HALF : FIRST_HALF) * uid ;
    unsigned long hash   = (first << 32) | ((unsigned long) 0xFFFFFFFF & SECOND_HALF) ;
    return hash % MODULUS ;
}

int hash_table_insert_uid(uid_t uid) {
    struct hash_table_record *ht_record ;
    struct list_head *list ;
    struct rw_semaphore *sem ;
    uid_record *record ;

    if (!down_read_trylock(&HT_RWLOCK)) return -EBUSY ;

    ht_record = &(&UID_TABLE)->records[evaluate_uid_hash(uid)] ;
    list = &ht_record->overflow_list;
    sem = &ht_record->sem ;
    down_write(sem) ;
    list_for_each_entry_rcu(record, list, overflow_list, uid_record) {
        if (record->uid == uid) {
            up_write(sem) ;
            up_read(&HT_RWLOCK) ;
            return -EEXIST ;
        }
    }

    record = kmalloc(sizeof(uid_record), GFP_KERNEL) ;
    if (record == NULL) {
        up_write(sem) ;
        up_read(&HT_RWLOCK) ;
        return -ENOMEM ;
    }

    record->uid = uid ;
    list_add_rcu(&record->overflow_list, list) ;

    up_write(sem) ;

    up_read(&HT_RWLOCK) ;
    return 0 ;
}

int hash_table_remove_uid(uid_t uid) {
    struct hash_table_record *ht_record ;
    struct list_head *list ;
    struct rw_semaphore *sem ;
    uid_record *record, *found = NULL;

    if (!down_read_trylock(&HT_RWLOCK)) return -EBUSY ;

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
        up_read(&HT_RWLOCK) ;
        return 0 ;
    }

    up_write(sem) ;
    up_read(&HT_RWLOCK) ;
    return -ENOENT ;
}

int hash_table_lock(void) {
    return down_write_trylock(&HT_RWLOCK) ? 0 : 1 ;
}

void hash_table_unlock(void) {
    up_write(&HT_RWLOCK) ;
}

void init_hash_table(void) {
    init_rwsem(&HT_RWLOCK) ;
    for (int i = 0; i < MODULUS; i++) {
        INIT_LIST_HEAD(&UID_TABLE.records[i].overflow_list) ;
        init_rwsem(&UID_TABLE.records[i].sem) ;
    }

    return ;
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

bool hash_table_has(uid_t uid) {
    int idx ;
    struct list_head *list ;
    uid_record *record ;
    bool found = false ;

    idx = evaluate_uid_hash(uid) ;
    list = &(&UID_TABLE)->records[idx].overflow_list ;

    rcu_read_lock() ;

    list_for_each_entry_rcu(record, list, overflow_list) {
        if (uid == record->uid) {
            found = true ;
            break ;
        }
    }

    rcu_read_unlock() ;

    return found ;
}


static inline int keep_reading(char __user *buf, ssize_t len) {

    if (handle.curr == NULL) return 0 ;

    char parsed[32] ;
    int remaining, parsedlen ;

    sprintf(parsed, "%d\n", handle.curr->uid) ;
    parsedlen = strlen(parsed) ;

    remaining = parsedlen - handle.state ;

    if (!remaining) {
        handle.state = 0 ;
        if (list_is_head(&handle.curr->overflow_list, handle.head)) {
            do {
                handle.ht_idx++ ;
                if (handle.ht_idx >= MODULUS) {
                    handle.eof = true ;
                    return 0 ;
                }

                handle.head = &UID_TABLE.records[handle.ht_idx].overflow_list ;
                if (list_empty(handle.head)) continue ;
                else {
                    handle.curr = container_of(handle.head->next, uid_record, overflow_list) ;
                    break ;
                }
            } while (1) ;
        }
    }

    int toWrite = min(remaining, len) ;
    copy_to_user(buf, parsed + handle.state, toWrite) ;

    handle.state += toWrite ;
    return toWrite ;
}

ssize_t ht_file_handle_read(char __user *buf, ssize_t len) {
    int read = 0, cum = 0 ;

    if (handle.eof) return 0 ;

    do {
        read = keep_reading(buf + cum, len - cum) ;
        cum += read ;
    } while (cum < len || read == 0) ;

    return cum ;
}

void reset_ht_file_handle(void) {
    handle.ht_idx = -1 ;
    do {
        handle.ht_idx++ ;
        if (handle.ht_idx >= MODULUS) {
            handle.head = NULL ;
            handle.curr = NULL ;
            break ;
        } ;

        handle.head = &UID_TABLE.records[handle.ht_idx].overflow_list ;
        if (list_empty(handle.head)) continue ;
        else {
            handle.curr = container_of(handle.head->next, uid_record, overflow_list) ;
            break ;
        }
    } while (1) ;

    handle.state = 0 ;
    handle.eof = false ;
}