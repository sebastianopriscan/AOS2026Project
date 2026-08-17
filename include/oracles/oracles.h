#ifndef ORACLES_H
#define ORACLES_H

#include "include/api/api.h"

typedef struct path_decree {
    bool path_found ;
    inode_descriptor descriptor ;
    char *path_ptr ;
    char pathname[PATH_MAX] ;
} path_decree ;

/**
 * Resolves a relative path into an absolute one
 * 
 * @param path The path to resolve
 * @returns A pointer to a struct path_decree containing the absolute path and eventual inode_descriptor, or ERR_PTR of an error
 */
path_decree *pathname_oracle(char *path);

#endif