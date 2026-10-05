#include "client.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Keeps the uids or the paths dump file open, and reads it one byte per key press,
 * to test the dump lock. Press enter to read a byte, ctrl-D to close the file.
 */
int main(int argc, char **argv) {
    const char *devPath ;
    char byte ;
    int fd ;
    ssize_t n ;

    if (argc != 2 || (strcmp(argv[1], "uids") != 0 && strcmp(argv[1], "paths") != 0)) {
        fprintf(stderr, "Usage:\n\tdumphold uids|paths\n") ;
        return -1 ;
    }

    devPath = strcmp(argv[1], "uids") == 0 ? DUMP_UIDS_PATH : DUMP_PATHS_PATH ;
    fd = open(devPath, O_RDONLY) ;
    if (fd == -1) {
        perror("Error opening dump file") ;
        return -1 ;
    }
    printf("Opened %s, press enter to read a byte, ctrl-D to close\n", devPath) ;

    while (getchar() != EOF) {
        n = read(fd, &byte, 1) ;
        if (n == -1) {
            perror("Error reading dump file") ;
            break ;
        }
        if (n == 0) {
            printf("\n[end of dump]\n") ;
            continue ;
        }
        putchar(byte) ;
        fflush(stdout) ;
    }

    close(fd) ;
    return 0 ;
}
