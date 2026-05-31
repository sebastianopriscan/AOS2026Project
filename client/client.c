#include "client.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#define PRINT_USAGE() \ 
        fprintf(stderr, "Usage:\n" \
            "\tthrottlectl set_on/set_off\n" \
            "\tthrottlectl add SYSCALLNUM1,SYSCALLNUM2,...\n" \
                "\t\tOptions (at least one needed)\n" \
                    "\t\t\t--path PATH\n" \
                    "\t\t\t--uid USER\n\t" \
            "\tthrottlectl rm\n" \
                "\t\tOptions (delete based on the same properties of an added policy)\n" \
                    "\t\t\t--path PATH\n" \
                    "\t\t\t--uid USER\n\t" \
        ) \

#define PRINT_ADD_USAGE() \ 
        fprintf(stderr, "Usage:\n" \
            "\tthrottlectl add SYSCALLNUM1,SYSCALLNUM2,...\n" \
                "\t\tOptions (at least one needed)\n" \
                    "\t\t\t--path PATH\n" \
                    "\t\t\t--uid USER\n\t" \
        ) \

#define PRINT_RM_USAGE() \ 
        fprintf(stderr, "Usage:\n" \
            "\tthrottlectl rm\n" \
                "\t\tOptions (delete based on the same properties of an added policy)\n" \
                    "\t\t\t--path PATH\n" \
                    "\t\t\t--uid USER\n\t" \
        ) \

static inline int processOption(const char *option, const char *value, throttleA_policy * policy) {

    if (strcmp(option, "--path") == 0) {
        if (policy->policy == POLICY_PROGRAM_ONLY || POLICY_UID_AND_PROGRAM) {
            fprintf(stderr, "Error: path specified multiple times\n") ;
            return -1 ;
        }
        int i = 0 ;
        do {
            if (strlen(value) > PATH_MAX) {
                fprintf(stderr, "Error: specified path too long.\n") ;
                return -1 ;
            }
            policy->path.pathName[i] = value[i] ;
            i++ ;
        } while (value[i] != '\0') ;
    } else if (strcmp(option, "--uid")) {
        if (policy->policy == POLICY_UID_ONLY || POLICY_UID_AND_PROGRAM) {
            fprintf(stderr, "Error: uid specified multiple times\n") ;
            return -1 ;
        }
        char *endptr ;
        long converted = strtol(value, &endptr, 10) ;
        if (*value == '\0' || *endptr != '\0') {
            PRINT_ADD_USAGE() ;
            return -1 ;
        }
        policy->uid = (unsigned int) converted;
    } else {
        PRINT_ADD_USAGE() ;
        return -1 ;
    }

    return 0 ;
}

int main(int argc, char **argv) {

    const int throttlerFd = open("/dev/throttleA-api", O_RDONLY) ;
    char *syscalls_copy ;
    throttleA_policy *policy ;
    int retVal = 0 ;

    if (throttlerFd == -1) {
        perror("Error opening device file: ") ;
        return -1 ;
    }

    if (argc < 2) {
        PRINT_USAGE() ;
        retVal = -1 ;
        goto close_throttler_fd ;
    }

    if (strcmp(argv[1], "set_on") == 0) {
        ioctl(throttlerFd, THROTTLER_SET_ENABLE) ;
    } else if (strcmp(argv[1], "set_off") == 0) {
        ioctl(throttlerFd, THROTTLER_SET_DISABLE) ;
    } else if (strcmp(argv[1], "add") == 0) {
        if (argc < 4 || argc % 2 == 0) {
            PRINT_ADD_USAGE() ;
            retVal = -1 ;
            goto close_throttler_fd ;
        }
        int syscalls_count = 1 ;
        int idx = 0 ;
        while (argv[2][idx] != '\0') {
            if (argv[2][idx] == ';') {
                syscalls_count++ ;
            }
            idx++ ;
        } 
        const int len = idx ;
        syscalls_copy = malloc(len +1) ;
        if (syscalls_copy == NULL) {
            perror("Error copying syscalls options: ") ;
            retVal = -1 ;
            goto close_throttler_fd ;
        }
        throttleA_policy *policy = malloc(sizeof(throttleA_policy)) ;
        memset(policy, 0, sizeof(throttleA_policy)) ;
        unsigned int *syscall_codes = &policy->syscalls ;
        if (syscall_codes == NULL) {
            perror("Error copying syscalls codes: ") ;
            retVal = -1 ;
            free(syscalls_copy) ;
        }

        char *token = strtok(syscalls_copy, ",") ;
        do {
            char *endptr ;
            long converted = strtol(token, &endptr, 10) ;
            if (*token == '\0' || *endptr != '\0') {
                PRINT_ADD_USAGE() ;
                retVal = -1 ;
                free(syscalls_copy) ;
                goto free_policy ;
            }

            syscall_codes[converted / (sizeof(unsigned long) * 8)] |= 1UL << (converted % (sizeof(unsigned long) * 8)) ;

            token = strtok(NULL, ",") ;
        } while (token != NULL) ;

        free(syscalls_copy) ;

        policy->policy = POLICY_NONE ;
        for (int j = 3; j < argc ; j += 2) {
            if (processOption(argv[j], argv[j+1], policy) == -1) {
                retVal = -1 ;
                goto free_policy ;
            }
        }
        if (policy == POLICY_NONE) {
            retVal = -1 ;
            goto free_policy ;
        }
    } else if (strcmp(argv[1], "rm") == 0) {
        if (argc < 3 || argc % 2 == 1) {
            PRINT_RM_USAGE() ;
            retVal = -1 ;
            goto close_throttler_fd ;
        }
    }

free_policy:
    free(policy) ;


close_throttler_fd :
    fclose(throttlerFd) ;

    return retVal ;
}
