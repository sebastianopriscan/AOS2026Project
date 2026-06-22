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

char *pathname_oracle(char *path) {

    char *buf, *cursor, *path_cursor ;
    struct path base_path ;

    buf = kzalloc(PAGE_SIZE, GFP_KERNEL) ;
    if (!buf) {
        return ERR_PTR(-ENOMEM) ;
    }

    if (path[0] == '/') {
        strncpy(buf, path, PATH_MAX) ;
        return buf;
    } else {
        kern_path(".", 0, &base_path) ;
        d_path(&base_path, buf, PATH_MAX) ;
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
                    kfree(buf) ;
                    return ERR_PTR(-E2BIG) ; 
                }
                memcpy(cursor, path_cursor, len) ;
                *(cursor + len) = '\0' ;
                break ;
            } else {
                const int len = next_cursor - path_cursor ;
                if ((cursor - buf) + len >= PATH_MAX -1) {
                    kfree(buf) ;
                    return ERR_PTR(-E2BIG) ;
                }
                memcpy(cursor, path_cursor, len) ;
                path_cursor = next_cursor +1;
                cursor += len ;
                *cursor = '\0' ;
            }
        }
    }

    return buf ;
}