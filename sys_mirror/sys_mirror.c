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

static struct kobject *mirror_root, *policy_program_uid, *policy_program, *policy_uid;

static ssize_t var_show_with_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

    policy_with_table *pt = container_of(attr, policy_with_table, kobj_attribute) ;

    return sprintf(buf, "Is active: %d\nSupported syscalls: %lx %lx %lx %lx %lx %lx %lx %lx\nThrottle counter: %ld\nTolerance %d\nUid : %d\n",
        atomic_read(&pt->isActive), 
        atomic_long_read(&pt->policy.syscalls[0]),
        atomic_long_read(&pt->policy.syscalls[1]),
        atomic_long_read(&pt->policy.syscalls[2]),
        atomic_long_read(&pt->policy.syscalls[3]),
        atomic_long_read(&pt->policy.syscalls[4]),
        atomic_long_read(&pt->policy.syscalls[5]),
        atomic_long_read(&pt->policy.syscalls[6]),
        atomic_long_read(&pt->policy.syscalls[7]),
        atomic_long_read(&pt->throttle_counter), 
        atomic_read(&pt->policy.tolerance),
        pt->policy.uid
    );
}

static ssize_t var_show_no_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

    policy_with_table *pt = container_of(attr, policy_with_table, kobj_attribute) ;

    return sprintf(buf, "Is active: %d\nSupported syscalls: %lx %lx %lx %lx %lx %lx %lx %lx\nThrottle counter: %ld\nTolerance %d\n",
        atomic_read(&pt->isActive), 
        atomic_long_read(&pt->policy.syscalls[0]),
        atomic_long_read(&pt->policy.syscalls[1]),
        atomic_long_read(&pt->policy.syscalls[2]),
        atomic_long_read(&pt->policy.syscalls[3]),
        atomic_long_read(&pt->policy.syscalls[4]),
        atomic_long_read(&pt->policy.syscalls[5]),
        atomic_long_read(&pt->policy.syscalls[6]),
        atomic_long_read(&pt->policy.syscalls[7]),
        atomic_long_read(&pt->throttle_counter), 
        atomic_read(&pt->policy.tolerance)
    );
}

static int sys_mirror_add_by_path(policy_with_table *table) {
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;
    char *curStart = buf +1 ;
    char *cur = curStart ;
    int chunks = 0 ;
    int i, j ;
    struct kobject *root, *kobject, *child;
    int ret = 0; 
    root = kobject = table->policy.policy == POLICY_UID_AND_PROGRAM ? policy_program_uid : policy_program ;

    if (!buf) {
        return -1 ;
    }
    
    strcpy(buf, table->policy.path.pathName) ;

    do {
        if (*cur == '\0' || *cur == '/') chunks++ ;
        if (*cur == '/') *cur = '\0' ;
        cur++ ;
    } while(*cur != '\0') ;

    kobject_get(root) ;
    for (curStart = buf+1, i = 0; i < chunks -1; i++, curStart += strlen(curStart) +1) {
        struct kobject *found ;
        found = kset_find_obj(kobject->kset, curStart) ;

        if(!found) {
            found = kobject_create_and_add(curStart, kobject) ;
            if (!found) {
                ret = -1 ;
                goto free_kobjs ;
            }
        } else {
            kobject_get(found) ;
        }
        kobject = found ;
    }

    child = kobject_create_and_add(curStart, kobject) ;
    if (!child) {
        ret = -1 ;
        goto free_kobjs;
    }

    table->kobj_attribute.attr.mode = 0600 ;
    table->kobj_attribute.show = table->policy.policy == POLICY_UID_AND_PROGRAM ? var_show_with_path : var_show_no_path;
    table->kobj_attribute.store = NULL ;

    if (sysfs_create_file(child, &table->kobj_attribute.attr)) {
        kobject_put(child) ;
        ret = -1 ;
        goto free_kobjs;
    }

    kfree(buf) ;
    return 0 ;

free_kobjs:
    for (j = 0; j < i; j++) {
        struct kobject *object = kobject ;
        kobject = kobject->parent ;
        //sysfs_remove_dir(object) ;
        kobject_put(object) ;
    }
    kobject_put(root) ;

    kfree(buf) ;

    return ret ;
}

static int sys_mirror_rm_by_path(policy_with_table *table) {
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;
    char *curStart = buf +1 ;
    char *cur = curStart ;
    int chunks = 0 ;
    int i ;
    struct kobject *root, *kobject;
    root = kobject = table->policy.policy == POLICY_UID_AND_PROGRAM ? policy_program_uid : policy_program ;

    if (!buf) {
        return -1 ;
    }
    
    strcpy(buf, table->policy.path.pathName) ;

    do {
        if (*cur == '\0' || *cur == '/') chunks++ ;
        if (*cur == '/') *cur = '\0' ;
        cur++ ;
    } while(*cur != '\0') ;

    for (curStart = buf+1, i = 0; i < chunks; i++, curStart += strlen(curStart) +1) {
        struct kobject *found ;
        found = kset_find_obj(kobject->kset, curStart) ;

        if(!found) {
            kfree(buf) ;
            return -1;
        }
        kobject = found ;
    }

    sysfs_remove_file(kobject, &table->kobj_attribute.attr) ;

    for (i = 0; i < chunks -1 ; i++) {
        struct kobject *toRemove = kobject ;
        kobject = kobject->parent ;
        kobject_put(toRemove) ;
        //sysfs_remove_dir(kobject) ;
    }

    kobject_put(kobject) ;
    kobject_put(root) ;

    kfree(buf) ;

    return 0 ;
}

static int sys_mirror_add_by_uid(policy_with_table *table) {
    struct kobject *found ;
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;

    if (!buf) {
        return -1 ;
    }
    sprintf(buf, "%d", table->policy.uid) ;

    kobject_get(policy_uid) ;
    found = kset_find_obj(policy_uid->kset, buf) ;

    if(!found) {
        struct kobject *child = kobject_create_and_add(buf, policy_uid) ;
        if (!child) {
            kfree(buf) ;
            kobject_put(policy_uid) ;
            return -1 ;
        }

        table->kobj_attribute.attr.mode = 0600 ;
        table->kobj_attribute.show = var_show_no_path;
        table->kobj_attribute.store = NULL ;

        if (sysfs_create_file(child, &table->kobj_attribute.attr)) {
            kobject_put(child) ;
            kobject_put(policy_uid) ;
            kfree(buf) ;
            return -1 ;
        }
    }
    kfree(buf) ;

    return 0 ;
}

static int sys_mirror_rm_by_uid(policy_with_table *table) {
    struct kobject *found ;
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;

    if (!buf) {
        return -1 ;
    }
    sprintf(buf, "%d", table->policy.uid) ;

    found = kset_find_obj(policy_uid->kset, buf) ;

    if(!found) {
        kfree(buf) ;
        return -1;
    }

    sysfs_remove_file(found, &table->kobj_attribute.attr) ;
    kobject_put(found) ;
    kobject_put(policy_uid) ;
    kfree(buf) ;

    return 0 ;
}

int sys_mirror_add(policy_with_table *table) {
    return table->policy.policy == POLICY_UID_ONLY ? sys_mirror_add_by_uid(table) : sys_mirror_add_by_path(table) ;
}

int sys_mirror_rm(policy_with_table *table) {
    return table->policy.policy == POLICY_UID_ONLY ? sys_mirror_rm_by_uid(table) : sys_mirror_rm_by_path(table) ;
}

int init_ht_sys_mirror(void) {

    mirror_root = kobject_create_and_add("mirror", &THIS_MODULE->mkobj.kobj) ;
    if (!mirror_root) {
        return PTR_ERR(mirror_root) ;
    }

    policy_program_uid = kobject_create_and_add("by-policy-program-uid", mirror_root) ;
    if (!policy_program_uid) {
        kobject_put(mirror_root) ;
        return PTR_ERR(policy_program_uid) ;
    }

    policy_program = kobject_create_and_add("by-policy-program", mirror_root) ;
    if (!policy_program) {
        kobject_put(policy_program_uid) ;
        kobject_put(mirror_root) ;
        return PTR_ERR(policy_program) ;
    }

    policy_uid = kobject_create_and_add("by-policy-uid", mirror_root) ;
    if (!policy_uid) {
        kobject_put(policy_program) ;
        kobject_put(policy_program_uid) ;
        kobject_put(mirror_root) ;
        return PTR_ERR(policy_uid) ;
    }

    return 0 ;
}

void clean_ht_sys_mirror(void) {
    kobject_put(policy_uid) ;
    kobject_put(policy_program) ;
    kobject_put(policy_program_uid) ;
    kobject_put(mirror_root) ;
}
