#include "include/utils/strings.h"

int slashcmp(const char *s1, const char *s2) {
    int cur = 0 ;
    do {
        if (s1[cur] == '/' && s2[cur] == '/') return 0 ;
        if (s1[cur] == '/' && s2[cur] != '/') return -1 ;
        if (s1[cur] != '/' && s2[cur] == '/') return 1 ;
        if (s1[cur] != s2[cur]) return -1 ;
        cur ++ ;
    } while (1) ;
}

int slashlen(const char *s) {
    int cur = 0 ;
    do {
        if (s[cur] == '/' || s[cur] == '\0') return cur ;
        cur++ ;
    } while(1) ;
}
