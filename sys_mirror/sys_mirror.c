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

static struct kobject *mirror_root, *policy_program_uid, *policy_program, *policy_uid, *path_program_uid, *path_program;

static ssize_t path_show_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    path_with_table *pt = container_of(attr, path_with_table, path_attribute) ;

    return sprintf(buf, "%s", pt->path.pathName) ;
}

static ssize_t path_show_with_uid(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

    path_with_table *pt = container_of(attr, path_with_table, desc_attribute) ;

    return sprintf(buf, "Supported syscalls: %lx %lx %lx %lx %lx %lx %lx %lx\nTolerance %d\nUid : %d\n",
        atomic_long_read(&pt->policy.syscalls[0]),
        atomic_long_read(&pt->policy.syscalls[1]),
        atomic_long_read(&pt->policy.syscalls[2]),
        atomic_long_read(&pt->policy.syscalls[3]),
        atomic_long_read(&pt->policy.syscalls[4]),
        atomic_long_read(&pt->policy.syscalls[5]),
        atomic_long_read(&pt->policy.syscalls[6]),
        atomic_long_read(&pt->policy.syscalls[7]),
        atomic_read(&pt->policy.tolerance),
        pt->policy.uid
    );
}

static ssize_t path_show_no_uid(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

    path_with_table *pt = container_of(attr, path_with_table, desc_attribute) ;

    return sprintf(buf, "Supported syscalls: %lx %lx %lx %lx %lx %lx %lx %lx\nTolerance %d\n",
        atomic_long_read(&pt->policy.syscalls[0]),
        atomic_long_read(&pt->policy.syscalls[1]),
        atomic_long_read(&pt->policy.syscalls[2]),
        atomic_long_read(&pt->policy.syscalls[3]),
        atomic_long_read(&pt->policy.syscalls[4]),
        atomic_long_read(&pt->policy.syscalls[5]),
        atomic_long_read(&pt->policy.syscalls[6]),
        atomic_long_read(&pt->policy.syscalls[7]),
        atomic_read(&pt->policy.tolerance)
    );
}

static ssize_t policy_show_with_inode_and_uid(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

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

static ssize_t policy_show_with_inode(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

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

static ssize_t policy_show_with_uid(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

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

int sys_mirror_path_add(path_with_table *table) {
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;
    int chunks = 0 ;
    int i, j ;
    struct kobject *root, *child, *desc, *path;
    int ret = 0; 
    root = table->policy.policy == POLICY_UID_AND_PROGRAM ? path_program_uid : path_program ;

    if (!buf) {
        return -1 ;
    }
    
    sprintf(buf, "%ld", table->id) ;

    child = kobject_create_and_add(buf, root) ;
    kfree(buf) ;

    if (!child) {
        return -1 ;
    }

    desc = kobject_create_and_add("desc", child) ;

    table->desc_attribute.attr.mode = 0600 ;
    table->desc_attribute.show = table->policy.policy == POLICY_UID_AND_PROGRAM ? path_show_with_uid : path_show_no_uid;
    table->desc_attribute.store = NULL ;

    if (sysfs_create_file(desc, &table->desc_attribute.attr)) {
        kobject_put(desc) ;
        kobject_put(child) ;
        return -1 ;
    }

    path = kobject_create_and_add("path", child) ;

    table->path_attribute.attr.mode = 0600 ;
    table->path_attribute.show = path_show_path;
    table->path_attribute.store = NULL ;

    if (sysfs_create_file(desc, &table->desc_attribute.attr)) {
        kobject_put(path) ;
        sysfs_remove_file(desc, &table->desc_attribute.attr) ;
        kobject_put(desc) ;
        kobject_put(child) ;
        return -1 ;
    }

    table->child = child ;
    table->name = path ;
    table->desc = desc ;
    return 0 ;
}

void sys_mirror_path_rm(path_with_table *table) {

    sysfs_remove_file(&table->desc, &table->desc_attribute.attr) ;
    sysfs_remove_file(&table->path, &table->path_attribute.attr) ;

    kobject_put(table->desc) ;
    kobject_put(table->name) ;
    kobject_put(table->desc) ;

    return ;
}

static int sys_mirror_add_by_inode(policy_with_table *table) {
    struct kobject *found, *root;
    char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL) ;

    if (!buf) {
        return -1 ;
    }
    sprintf(buf, "%d:%d", table->policy.inode.device_id, table->policy.inode.inode_number) ;

    root = table->policy.policy == POLICY_UID_AND_PROGRAM ? policy_program_uid : policy_program ;
    kobject_get(root) ;
    found = kset_find_obj(root->kset, buf) ;

    if(!found) {
        struct kobject *child, *dir = kobject_create_and_add(buf, policy_uid) ;
        struct list_head *pos ;

        if (!dir) {
            kfree(buf) ;
            kobject_put(root) ;
            return -1 ;
        }
        child = kobject_create_and_add("policy", dir) ;
        if (!child) {
            kfree(buf) ;
            kobject_put(dir) ;
            kobject_put(root) ;
        }

        table->kobj_attribute.attr.mode = 0600 ;
        table->kobj_attribute.show = table->policy.policy == POLICY_UID_AND_PROGRAM ? policy_show_with_inode_and_uid : policy_show_with_inode;
        table->kobj_attribute.store = NULL ;

        if (sysfs_create_file(child, &table->kobj_attribute.attr)) {
            kobject_put(dir) ;
            kobject_put(policy_uid) ;
            kfree(buf) ;
            return -1 ;
        }
        table->kobj = child ;
    }
    kfree(buf) ;

    return 0 ;
}

static void sys_mirror_rm_by_inode(policy_with_table *table) {
    struct kobject *parent = table->kobj->parent ;

    sysfs_remove_file(table->kobj, &table->kobj_attribute.attr) ;
    kobject_put(table->kobj) ;
    kobject_put(parent) ;
    kobject_put(table->policy.policy == POLICY_UID_AND_PROGRAM ? policy_program_uid : policy_program) ;

    return ;
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
        table->kobj_attribute.show = policy_show_with_uid;
        table->kobj_attribute.store = NULL ;

        if (sysfs_create_file(child, &table->kobj_attribute.attr)) {
            kobject_put(child) ;
            kobject_put(policy_uid) ;
            kfree(buf) ;
            return -1 ;
        }
        table->kobj = child ;
    }
    kfree(buf) ;

    return 0 ;
}

static void sys_mirror_rm_by_uid(policy_with_table *table) {
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

int sys_mirror_policy_add(policy_with_table *table) {
    return table->policy.policy == POLICY_UID_ONLY ? sys_mirror_add_by_uid(table) : sys_mirror_add_by_inode(table) ;
}

void sys_mirror_policy_rm(policy_with_table *table) {
    table->policy.policy == POLICY_UID_ONLY ? sys_mirror_rm_by_uid(table) : sys_mirror_rm_by_inode(table) ;
}

/**
 * Realizes a two way binding between a policy and
 * a path
 */
int bind_policy_to_path(policy_with_table *table, path_with_table *path) {
    char buf[32] ;

    sprintf(buf, "%lu", path->id) ;
    if (sysfs_create_link(table->kobj, path->name, buf)) {
        return -1 ;
    }

    if (sysfs_create_link(path->child, table->kobj, PATH_TABLE_SYMLINK_NAME)) {
        sysfs_remove_link(table->kobj, buf) ;
        return -1 ;
    }

    return 0 ;
}

/**
 * Unbinds a path-policy binding
 */
void unbind_path(path_with_table *path) {
    sysfs_remove_link(path->child, PATH_TABLE_SYMLINK_NAME) ;
    return ;
}

/**
 * Unbinds a policy-path binding
 */
void unbind_policy(policy_with_table *table, unsigned long id) {
    char buf[32] ;
    sprintf(buf, "%lu", id) ;

    sysfs_remove_link(table->kobj, buf) ;

    return ;
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

    path_program_uid = kobject_create_and_add("paths-program-uid", mirror_root) ;
    if (!path_program_uid) {
        kobject_put(policy_program) ;
        kobject_put(policy_program_uid) ;
        kobject_put(mirror_root) ;
        kobject_put(policy_uid) ;
        return PTR_ERR(path_program_uid) ;
    }

    path_program = kobject_create_and_add("paths-program", mirror_root) ;
    if (!path_program) {
        kobject_put(policy_program) ;
        kobject_put(policy_program_uid) ;
        kobject_put(mirror_root) ;
        kobject_put(policy_uid) ;
        kobject_put(path_program_uid) ;
        return PTR_ERR(path_program) ;
    }

    return 0 ;
}

void clean_ht_sys_mirror(void) {
    kobject_put(path_program) ;
    kobject_put(path_program_uid) ;
    kobject_put(policy_uid) ;
    kobject_put(policy_program) ;
    kobject_put(policy_program_uid) ;
    kobject_put(mirror_root) ;
}
