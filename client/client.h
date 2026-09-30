#ifndef CLIENT_H
#define CLIENT_H

#include <linux/limits.h>

/*
 * User-space mirror of include/api/api.h: keep the two in sync.
 */

#define API_DEVICE_PATH "/dev/throttleA-api"

#define SYSCALL_LIMIT 500
#define DATA_PER_LIMIT(type) (SYSCALL_LIMIT + sizeof(type) * 8 -1) / (sizeof(type) * 8)
#define TYPE_ARRAY_PER_LIMIT(type, property) type property[DATA_PER_LIMIT(type)]

/*
ioctl op arg specification:

The op arg uses the four most significant bits to encode the operation kind,
for each operation it will be specified if it's in and/or out only here,
and the parameters'length will be passed in the remaining bytes
*/

/**
 * Describes an operating system's file's path
 */
struct _throttleA_path {
    char pathName[PATH_MAX] ;
} ;
typedef struct _throttleA_path throttleA_path ;

typedef unsigned long throttleA_syscall_map_type ;
/**
 * Bitmap of the x86_64 syscalls, bit at index i
 * corresponds to the ith's syscall code
 */
struct _throttleA_syscall_map {
    TYPE_ARRAY_PER_LIMIT(throttleA_syscall_map_type, map) ;
} ;
typedef struct _throttleA_syscall_map throttleA_syscall_map ;

#define STATS_PAGE_SIZE 4096

/**
 * Mirror of the kernel's struct stats_register (include/stats/stats.h).
 * The kernel-only types are replaced by same-sized user-space ones:
 * atomic_long_t -> long, spinlock_t -> unsigned int (4 bytes when the
 * kernel is built without spinlock debugging), PAGE_SIZE -> 4096 (x86_64).
 */
struct throttleA_stats_register {
    long tolerance ;
    unsigned int reg_lock ;
    unsigned long MAX ;

    unsigned long peak_blocked ;
    unsigned long sum_blocked ;
    unsigned long num_blocked ;

    unsigned long peak_delay ;
    unsigned long peak_uid ;
    unsigned char peak_name[2*STATS_PAGE_SIZE] ;
} ;

/******** Operation addUid: ********
    OPCODE    : 0b0000
    OPARG     : unsigned long (passed by value)
    OPARGTYPE : IN
    ARGSIZE   : 0
*/
#define ADD_UID 0x00000000

/******** Operation removeUid: ********
    OPCODE    : 0b0001
    OPARG     : unsigned long (passed by value)
    OPARGTYPE : IN
    ARGSIZE   : 0
*/
#define RM_UID 0x10000000

/******** Operation addPath: ********
    OPCODE    : 0b0010
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)
*/
#define ADD_PATH 0x20000000

/******** Operation rmPath: ********
    OPCODE    : 0b0011
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)
*/
#define RM_PATH 0x30000000

/******** Operation addSyscalls: ********
    OPCODE    : 0b0100
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)
*/
#define ADD_SYSCALLS 0x40000000

/******** Operation rmSyscalls: ********
    OPCODE    : 0b0101
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)
*/
#define RM_SYSCALLS 0x50000000

/******** Operation dumpSyscalls: ********
    OPCODE    : 0b0110
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : OUT
    ARGSIZE   : sizeof(throttleA_syscall_map)
*/
#define DUMP_SYSCALLS 0x60000000

/******** Operation dumpStats: ********
    OPCODE    : 0b0111
    OPARG     : struct throttleA_stats_register *
    OPARGTYPE : OUT
    ARGSIZE   : sizeof(struct throttleA_stats_register)
*/
#define DUMP_STATS 0x70000000

/******** Operation resetMax: ********
    OPCODE    : 0b1000
    OPARG     : unsigned long (passed by value)
    OPARGTYPE : IN
    ARGSIZE   : 0
*/
#define RESET_MAX 0x80000000

/******** Operation setThrottler: ********
    OPCODE    : 0b1001
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0
*/
#define THROTTLER_SET_ENABLE 0x90000000

/******** Operation setThrottler: ********
    OPCODE    : 0b1010
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0
*/
#define THROTTLER_SET_DISABLE 0xA0000000

#endif
