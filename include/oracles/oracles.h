#ifndef ORACLES_H
#define ORACLES_H

/**
 * Resolves and validates a user provider path:
 * 
 * First, it tries to solve the path as is, in case it exists.
 * In case not, if the path is absolute, it just uses it, otherwise it tries 
 * to resolve the cwd and appends the relative path to it, handling "."
 * and ".." elements as well as empty components
 *
 * @param path The path to resolve
 * @returns An allocated PATH_MAX size buf, containing the path, or ERR_PTR in case of errors
 */
char *pathname_oracle(const char *path) ;

#endif