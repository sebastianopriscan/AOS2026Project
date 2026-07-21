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
#include <linux/namei.h>

#include "include/oracles/oracles.h"

static inline char *moveFwdTo(char *cursor, char symbol, int maxLen) {
    for (int i = 0; i < maxLen; i++) {
        if (cursor[i] == symbol) return cursor +i ; 
    }
    return cursor ;
}

static inline char *moveBackTo(char *cursor, char symbol, int maxLen) {
    for (int i = 0; i < maxLen; i++) {
        if (cursor[-i] == symbol) return cursor -i ; 
    }
    return cursor ;
}

path_decree *pathname_oracle(char *path) {

    path_decree *decree ;
    char *buf, *cursor, *path_cursor ;
    struct path base_path, abs_path ;
    struct inode *inode_solved ;

    decree = kzalloc(sizeof(path_decree), GFP_KERNEL) ;
    if (!decree) {
        return ERR_PTR(-ENOMEM) ;
    }
    buf = decree->pathname ;
    decree->path_ptr = decree->pathname ;

    if (path[0] == '/') {
        strncpy(buf, path, PATH_MAX) ;
    } else {
        int kern_path_ret ;
        kern_path_ret = kern_path(".", 0, &base_path) ;
        if (kern_path_ret) {
            kfree(decree) ;
            return ERR_PTR(kern_path_ret) ;
        }
        buf = d_path(&base_path, buf, PATH_MAX) ;
        if (IS_ERR(buf)) {
            path_put(&base_path) ;
            kfree(decree) ;
            return buf ;
        }
        decree->path_ptr = buf ;
        path_put(&base_path) ;
    }

    cursor = buf + strlen(buf) ;
    path_cursor = path[0] == '/' ? path : path +1 ;

    while (1) {
        if (
            (strlen(path) - (path_cursor - path) >= 1) &&
            (
                (*path_cursor == '.' && *(path_cursor+1) == '/') ||
                (*path_cursor == '/')
            )
        ) {
            path_cursor = moveFwdTo(path_cursor, '/', PATH_MAX -1 - (path_cursor - path)) +1 ;
        }
        else if ((strlen(path) - (path_cursor - path) >= 2) && *path_cursor == '.' && *(path_cursor +1) == '.' && *(path_cursor +2) == '/') {
            cursor = moveBackTo(cursor, '/', cursor - buf) ;
            *cursor = '\0' ;
            path_cursor = moveFwdTo(path_cursor, '/', PATH_MAX - (path_cursor - path)) +1 ;
        } else {
            char *next_cursor = moveFwdTo(path_cursor, '/', PATH_MAX - (path_cursor -path)) ;
            *cursor = '/' ;
            cursor ++ ;
            if (next_cursor == path_cursor) {
                const int len = strlen(path_cursor) ;
                if ((cursor - buf) + len >= PATH_MAX -1) {
                    kfree(decree) ;
                    return ERR_PTR(-E2BIG) ; 
                }
                memcpy(cursor, path_cursor, len) ;
                *(cursor + len) = '\0' ;
                break ;
            } else {
                const int len = next_cursor - path_cursor ;
                if ((cursor - buf) + len >= PATH_MAX -1) {
                    kfree(decree) ;
                    return ERR_PTR(-E2BIG) ;
                }
                memcpy(cursor, path_cursor, len) ;
                path_cursor = next_cursor +1;
                cursor += len ;
                *cursor = '\0' ;
            }
        }
    }

    kern_path(buf, 0, &abs_path) ;

    dget(&abs_path.dentry) ;

    inode_solved = d_inode(&abs_path.dentry) ;
    if (abs_path.dentry->d_inode == NULL) {
        dput(&abs_path.dentry) ;
        path_put(&abs_path) ;
        decree->path_found = false ;
        return decree ;
    }

    inode_lock_shared(abs_path.dentry->d_inode) ;
    down_read(&abs_path.dentry->d_inode->i_sb->s_umount) ;
    decree->descriptor.device_id = &abs_path.dentry->d_inode->i_sb->s_dev ;
    decree->descriptor.inode_number = &abs_path.dentry->d_inode->i_ino ;
    up_read(&abs_path.dentry->d_inode->i_sb->s_umount) ;
    inode_unlock_shared(&abs_path.dentry->d_inode) ;
    dput(&abs_path.dentry) ;

    decree->path_found = true ;

    return decree ;
}