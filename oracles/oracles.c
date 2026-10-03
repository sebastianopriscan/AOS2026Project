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
#include "include/utils/strings.h"

static bool conformant_path(const char *path) {
    if (path[0] != '/' || path[1] == '\0') return false ;

    while (*path) {
        int len ;
        path++ ;
        len = slashlen(path) ;
        if (len == 0 || len > NAME_MAX) return false ;
        if (path[0] == '.' && (len == 1 || (len == 2 && path[1] == '.'))) return false ;
        path += len ;
    }
    return true ;
}

static int path_to_buf(const struct path *path, char *buf) {
    char *name ;

    // d_path would append " (deleted)" to it
    if (d_unlinked(path->dentry)) return -ENOENT ;

    name = d_path(path, buf, PATH_MAX) ;
    if (IS_ERR(name)) return PTR_ERR(name) ;

    // d_path builds the string at the end of buf
    memmove(buf, name, strlen(name) +1) ;
    return 0 ;
}

static int cwd_to_buf(char *buf) {
    struct path cwd ;
    int err = kern_path(".", 0, &cwd) ;

    if (err) return err ;
    err = path_to_buf(&cwd, buf) ;
    path_put(&cwd) ;
    return err ;
}

static int append_path(char *buf, const char *path) {
    int len = strlen(buf) ;

    // The root is the only path ending with '/', the components bring their own
    if (len == 1) len = 0 ;

    while (*path) {
        int clen ;

        if (*path == '/') {
            path ++ ;
            continue ;
        }

        clen = slashlen(path) ;
        if (clen == 1 && path[0] == '.') {
            // Same directory
        } else if (clen == 2 && path[0] == '.' && path[1] == '.') {
            while (len > 0 && buf[len -1] != '/') len -- ;
            if (len > 0) len -- ;
        } else {
            if (len + 1 + clen >= PATH_MAX) return -ENAMETOOLONG ;
            buf[len ++] = '/' ;
            memcpy(buf + len, path, clen) ;
            len += clen ;
        }
        path += clen ;
    }

    buf[len] = '\0' ;
    return 0 ;
}

char *pathname_oracle(const char *path) {

    struct path resolved ;
    char *buf ;
    int err ;

    // kern_path would read past the end of an unterminated path
    if (strnlen(path, PATH_MAX) == PATH_MAX) return ERR_PTR(-ENAMETOOLONG) ;
    if (path[0] == '\0') return ERR_PTR(-EINVAL) ;

    buf = kzalloc(PATH_MAX, GFP_KERNEL) ;
    if (!buf) {
        return ERR_PTR(-ENOMEM) ;
    }

    // No LOOKUP_FOLLOW: if path is a symlink, it is the name that gets resolved, not its target
    if (kern_path(path, 0, &resolved) == 0) {
        err = path_to_buf(&resolved, buf) ;
        path_put(&resolved) ;
    } else {
        // Path doesn't exist, build it by hand, relative ones start from the cwd
        err = path[0] == '/' ? 0 : cwd_to_buf(buf) ;
        if (!err) err = append_path(buf, path) ;
    }

    if (!err && !conformant_path(buf)) err = -EINVAL ;

    if (err) {
        kfree(buf) ;
        return ERR_PTR(err) ;
    }

    return buf ;
}
