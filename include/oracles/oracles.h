#ifndef ORACLES_H
#define ORACLES_H

/**
 * Resolves a relative path into an absolute one
 * 
 * @param path The path to resolve
 * @returns A PATH_MAX size buf containing the absolute path, or ERR_PTR of an error otherwise
 */
char *pathname_oracle(char *path) ;

#endif