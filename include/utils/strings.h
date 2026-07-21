#ifndef THROTTLEA_STRINGS_H
#define THROTTLEA_STRINGS_H

/**
 * Like strcmp, but stops at '/' instead of '\0'
 */
int slashcmp(char *s1, char *s2) ;

/**
 * Like strlen, but stops at both '/' and '\0'
 */
int slashlen(char *s) ;

#endif