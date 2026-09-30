#include "client.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#define MAP_ENTRY_BITS (sizeof(throttleA_syscall_map_type) * 8)

#define PRINT_USAGE() \
        fprintf(stderr, "Usage:\n" \
            "\tthrottlectl on|off\n" \
            "\tthrottlectl uid add|rm UID\n" \
            "\tthrottlectl path add|rm PATH\n" \
            "\tthrottlectl syscalls add|rm SYSCALLNUM1,SYSCALLNUM2,...\n" \
            "\tthrottlectl syscalls dump\n" \
            "\tthrottlectl stats\n" \
            "\tthrottlectl max MAX\n" \
        ) \

/**
 * Parses a non-negative decimal number, returns -1 on malformed input
 */
static inline int parseUnsigned(const char *value, unsigned long *out) {
    char *endptr ;

    if (*value == '\0' || *value == '-') return -1 ;

    errno = 0 ;
    *out = strtoul(value, &endptr, 10) ;
    if (*endptr != '\0' || errno == ERANGE) return -1 ;

    return 0 ;
}

/**
 * Fills the map with the comma separated syscall numbers in list
 */
static inline int parseSyscalls(const char *list, throttleA_syscall_map *map) {
    char *copy, *token ;
    unsigned long code ;
    int retVal = 0 ;

    copy = strdup(list) ;
    if (copy == NULL) {
        perror("Error copying syscalls list") ;
        return -1 ;
    }

    memset(map, 0, sizeof(throttleA_syscall_map)) ;

    token = strtok(copy, ",") ;
    if (token == NULL) {
        fprintf(stderr, "Error: empty syscall list\n") ;
        retVal = -1 ;
    }
    while (token != NULL) {
        if (parseUnsigned(token, &code) == -1 || code >= SYSCALL_LIMIT) {
            fprintf(stderr, "Error: invalid syscall number '%s' (allowed range 0-%d)\n", token, SYSCALL_LIMIT -1) ;
            retVal = -1 ;
            break ;
        }
        map->map[code / MAP_ENTRY_BITS] |= 1UL << (code % MAP_ENTRY_BITS) ;
        token = strtok(NULL, ",") ;
    }

    free(copy) ;
    return retVal ;
}

/**
 * Fills path with the absolute, symlink-free version of value.
 * If the file can't be resolved (e.g. it was deleted), an already
 * absolute path is accepted verbatim.
 */
static inline int parsePath(const char *value, throttleA_path *path) {
    memset(path, 0, sizeof(throttleA_path)) ;

    if (realpath(value, path->pathName) != NULL) return 0 ;

    if (value[0] == '/' && strlen(value) < PATH_MAX) {
        strcpy(path->pathName, value) ;
        return 0 ;
    }

    perror("Error resolving path") ;
    return -1 ;
}

/**
 * Issues the ioctl and reports failures, both from the syscall itself
 * and from the module's handlers (non-zero return values)
 */
static inline int doIoctl(int fd, unsigned int code, unsigned long arg) {
    int ret = ioctl(fd, code, arg) ;

    if (ret == -1) {
        perror("Error invoking throttler operation") ;
        return -1 ;
    }
    if (ret != 0) {
        fprintf(stderr, "Error: throttler operation returned %d\n", ret) ;
        return -1 ;
    }
    return 0 ;
}

static int handleUid(int fd, int argc, char **argv) {
    unsigned long uid ;

    if (argc != 4 || parseUnsigned(argv[3], &uid) == -1) {
        PRINT_USAGE() ;
        return -1 ;
    }

    // (uid_t) -1 is the kernel's INVALID_UID
    if (uid >= (uid_t) -1) {
        fprintf(stderr, "Error: invalid uid '%s' (allowed range 0-%u)\n", argv[3], (uid_t) -2) ;
        return -1 ;
    }

    if (strcmp(argv[2], "add") == 0) return doIoctl(fd, ADD_UID, uid) ;
    if (strcmp(argv[2], "rm") == 0) return doIoctl(fd, RM_UID, uid) ;

    PRINT_USAGE() ;
    return -1 ;
}

static int handlePath(int fd, int argc, char **argv) {
    throttleA_path *path ;
    unsigned int code ;
    int retVal ;

    if (argc != 4) {
        PRINT_USAGE() ;
        return -1 ;
    }

    if (strcmp(argv[2], "add") == 0) code = ADD_PATH ;
    else if (strcmp(argv[2], "rm") == 0) code = RM_PATH ;
    else {
        PRINT_USAGE() ;
        return -1 ;
    }

    path = malloc(sizeof(throttleA_path)) ;
    if (path == NULL) {
        perror("Error allocating path") ;
        return -1 ;
    }

    retVal = parsePath(argv[3], path) ;
    if (retVal == 0) {
        retVal = doIoctl(fd, code | sizeof(throttleA_path), (unsigned long) path) ;
    }

    free(path) ;
    return retVal ;
}

static int handleSyscalls(int fd, int argc, char **argv) {
    throttleA_syscall_map map ;
    unsigned int code ;

    if (argc == 3 && strcmp(argv[2], "dump") == 0) {
        memset(&map, 0, sizeof(throttleA_syscall_map)) ;
        if (doIoctl(fd, DUMP_SYSCALLS | sizeof(throttleA_syscall_map), (unsigned long) &map) == -1) return -1 ;

        for (unsigned long i = 0 ; i < SYSCALL_LIMIT ; i++) {
            if (map.map[i / MAP_ENTRY_BITS] & (1UL << (i % MAP_ENTRY_BITS))) {
                printf("%lu\n", i) ;
            }
        }
        return 0 ;
    }

    if (argc != 4) {
        PRINT_USAGE() ;
        return -1 ;
    }

    if (strcmp(argv[2], "add") == 0) code = ADD_SYSCALLS ;
    else if (strcmp(argv[2], "rm") == 0) code = RM_SYSCALLS ;
    else {
        PRINT_USAGE() ;
        return -1 ;
    }

    if (parseSyscalls(argv[3], &map) == -1) return -1 ;

    return doIoctl(fd, code | sizeof(throttleA_syscall_map), (unsigned long) &map) ;
}

static int handleStats(int fd, int argc) {
    struct throttleA_stats_register *reg ;
    int retVal ;

    if (argc != 2) {
        PRINT_USAGE() ;
        return -1 ;
    }

    reg = calloc(1, sizeof(struct throttleA_stats_register)) ;
    if (reg == NULL) {
        perror("Error allocating stats buffer") ;
        return -1 ;
    }

    retVal = doIoctl(fd, DUMP_STATS | sizeof(struct throttleA_stats_register), (unsigned long) reg) ;
    if (retVal == 0) {
        reg->peak_name[sizeof(reg->peak_name) -1] = '\0' ;

        printf("MAX               : %lu\n", reg->MAX) ;
        printf("Current window    : %ld\n", reg->tolerance) ;
        printf("Peak blocked      : %lu\n", reg->peak_blocked) ;
        if (reg->num_blocked != 0) {
            printf("Average blocked   : %.2f\n", (double) reg->sum_blocked / reg->num_blocked) ;
        } else {
            printf("Average blocked   : 0\n") ;
        }
        printf("Peak delay        : %lu\n", reg->peak_delay) ;
        printf("Peak delay uid    : %lu\n", reg->peak_uid) ;
        printf("Peak delay program: %s\n", reg->peak_name) ;
    }

    free(reg) ;
    return retVal ;
}

static int handleMax(int fd, int argc, char **argv) {
    unsigned long max ;

    if (argc != 3 || parseUnsigned(argv[2], &max) == -1) {
        PRINT_USAGE() ;
        return -1 ;
    }

    return doIoctl(fd, RESET_MAX, max) ;
}

int main(int argc, char **argv) {

    int throttlerFd ;
    int retVal ;

    if (argc < 2) {
        PRINT_USAGE() ;
        return -1 ;
    }

    throttlerFd = open(API_DEVICE_PATH, O_RDONLY) ;
    if (throttlerFd == -1) {
        perror("Error opening device file " API_DEVICE_PATH) ;
        return -1 ;
    }

    if (strcmp(argv[1], "on") == 0 && argc == 2) {
        retVal = doIoctl(throttlerFd, THROTTLER_SET_ENABLE, 0) ;
    } else if (strcmp(argv[1], "off") == 0 && argc == 2) {
        retVal = doIoctl(throttlerFd, THROTTLER_SET_DISABLE, 0) ;
    } else if (strcmp(argv[1], "uid") == 0 && argc >= 3) {
        retVal = handleUid(throttlerFd, argc, argv) ;
    } else if (strcmp(argv[1], "path") == 0 && argc >= 3) {
        retVal = handlePath(throttlerFd, argc, argv) ;
    } else if (strcmp(argv[1], "syscalls") == 0 && argc >= 3) {
        retVal = handleSyscalls(throttlerFd, argc, argv) ;
    } else if (strcmp(argv[1], "stats") == 0) {
        retVal = handleStats(throttlerFd, argc) ;
    } else if (strcmp(argv[1], "max") == 0) {
        retVal = handleMax(throttlerFd, argc, argv) ;
    } else {
        PRINT_USAGE() ;
        retVal = -1 ;
    }

    close(throttlerFd) ;

    return retVal ;
}
