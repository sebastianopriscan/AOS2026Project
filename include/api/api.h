#ifndef API_H
#define API_H

#include <linux/limits.h>

#include "include/stats/stats.h"

#define SYSCALL_LIMIT 500
#define DATA_PER_LIMIT(type) (SYSCALL_LIMIT + sizeof(type) * 8 -1) / (sizeof(type) * 8)
#define TYPE_ARRAY_PER_LIMIT(type, property) type property[DATA_PER_LIMIT(type)]

/*
ioctl op arg specification:

The op arg uses the three most significant bits to encode the operation kind,
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



/******** Operation addUid: ********
    OPCODE    : 0b0000
    OPMACRO   : ADD_UID
    OPARG     : unsigned long
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : adds a uid to be monitored by the throttling manager.
*/
#define ADD_UID 0x00000000

ssize_t throttleA_uid_add(unsigned long) ;

/******** Operation removeUid: ********
    OPCODE    : 0b0001
    OPMACRO   : RM_UID
    OPARG     : unsigned long
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : removes a uid being monitored by the throttling manager.
*/
#define RM_UID 0x10000000

ssize_t throttleA_uid_rm(unsigned long) ;

/******** Operation addPath: ********
    OPCODE    : 0b0010
    OPMACRO   : ADD_PATH
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)

    Description : adds a path to be monitored by the throttling manager.
*/
#define ADD_PATH 0x20000000

ssize_t throttleA_path_add(throttleA_path *) ;

/******** Operation rmPath: ********
    OPCODE    : 0b0011
    OPMACRO   : RM_PATH
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)

    Description : removes a path being monitored by the throttling manager.
*/
#define RM_PATH 0x30000000

ssize_t throttleA_path_rm(throttleA_path *) ;

/******** Operation addSyscalls: ********
    OPCODE    : 0b0100
    OPMACRO   : ADD_SYSCALLS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : adds some syscalls to be monitored by the throttling manager.
*/
#define ADD_SYSCALLS 0x40000000

ssize_t throttleA_syscalls_add(throttleA_syscall_map *) ;

/******** Operation rmSyscalls: ********
    OPCODE    : 0b0101
    OPMACRO   : RM_SYSCALLS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : removes some syscalls being monitored by the throttling manager.
*/
#define RM_SYSCALLS 0x50000000

ssize_t throttleA_syscalls_rm(throttleA_syscall_map *) ;

/******** Operation dumpSyscalls: ********
    OPCODE    : 0b0110
    OPMACRO   : DUMP_SYSCALLS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : OUT
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : Dumps the syscalls being monitored by the throttling manager
*/
#define DUMP_SYSCALLS 0x60000000

ssize_t throttleA_syscalls_dump(throttleA_syscall_map *) ;

/******** Operation dumpStats: ********
    OPCODE    : 0b0111
    OPMACRO   : DUMP_STATS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : OUT
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : Dumps the syscalls being monitored by the throttling manager
*/
#define DUMP_STATS 0x70000000

ssize_t throttleA_stats_dump(struct stats_register *) ;

/******** Operation resetMax: ********
    OPCODE    : 0b1000
    OPMACRO   : RESET_MAX
    OPARG     : unsigned long
    OPARGTYPE : IN
    ARGSIZE   : sizeof(unsigned long)

    Description : Dumps the syscalls being monitored by the throttling manager
*/
#define RESET_MAX 0x80000000

ssize_t throttleA_reset_max(unsigned long) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b1001
    OPMACRO   : THROTTLER_SET_ENABLE
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_ENABLE 0x90000000

ssize_t set_throttler_on(void) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b1010
    OPMACRO   : THROTTLER_SET_DISABLE
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_DISABLE 0xA0000000

ssize_t set_throttler_off(void) ;

/******** Operation dumpStatus: ********
    OPCODE    : 0b1011
    OPMACRO   : DUMP_STATUS
    OPARG     : void
    OPARGTYPE : OUT (through the return value)
    ARGSIZE   : 0

    Description : returns 1 if the throttler is on, 0 if it is off
*/
#define DUMP_STATUS 0xB0000000

ssize_t throttleA_status_dump(void) ;


/*********** Lifecycle operations **********/

int setup_api(void) ;
void cleanup_api(void) ;

#endif