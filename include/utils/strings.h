#ifndef THROTTLEA_STRINGS_H
#define THROTTLEA_STRINGS_H

/**
 * Like strcmp, but stops at '/' instead of '\0'
 */
int slashcmp(const char *s1, const char *s2) ;

/**
 * Like strlen, but stops at both '/' and '\0'
 */
int slashlen(const char *s) ;

#endif