#ifndef API_H
#define API_H

#include <linux/limits.h>

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
    OPCODE    : 0b000
    OPMACRO   : ADD_UID
    OPARG     : unsigned long
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : adds a uid to be monitored by the throttling manager.
*/
#define ADD_UID 0x00000000


ssize_t throttleA_uid_add(unsigned long) ;

/******** Operation removeUid: ********
    OPCODE    : 0b001
    OPMACRO   : RM_UID
    OPARG     : unsigned long
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : removes a uid being monitored by the throttling manager.
*/
#define RM_UID 0x20000000

ssize_t throttleA_uid_rm(unsigned long) ;

/******** Operation addPath: ********
    OPCODE    : 0b010
    OPMACRO   : ADD_PATH
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)

    Description : adds a path to be monitored by the throttling manager.
*/
#define ADD_PATH 0x40000000

ssize_t throttleA_path_add(throttleA_path *) ;

/******** Operation rmPath: ********
    OPCODE    : 0b011
    OPMACRO   : RM_PATH
    OPARG     : throttleA_path *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_path)

    Description : removes a path being monitored by the throttling manager.
*/
#define RM_PATH 0x60000000

ssize_t throttleA_path_rm(throttleA_path *) ;

/******** Operation addSyscalls: ********
    OPCODE    : 0b100
    OPMACRO   : ADD_SYSCALLS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : adds some syscalls to be monitored by the throttling manager.
*/
#define ADD_SYSCALLS 0x80000000

ssize_t throttleA_syscalls_add(throttleA_syscall_map *) ;

/******** Operation rmSyscalls: ********
    OPCODE    : 0b101
    OPMACRO   : RM_SYSCALLS
    OPARG     : throttleA_syscall_map *
    OPARGTYPE : IN
    ARGSIZE   : sizeof(throttleA_syscall_map)

    Description : removes some syscalls being monitored by the throttling manager.
*/
#define RM_SYSCALLS 0xA0000000

ssize_t throttleA_syscalls_rm(throttleA_syscall_map *) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b110
    OPMACRO   : THROTTLER_SET_ENABLE
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_ENABLE 0xC0000000

ssize_t set_throttler_on(void) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b111
    OPMACRO   : THROTTLER_SET_DISABLE
    OPARG     : void
    OPARGTYPE : IN
    ARGSIZE   : 0

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_DISABLE 0xE0000000

ssize_t set_throttler_off(void) ;


/*********** Lifecycle operations **********/

int setup_api(void) ;
void cleanup_api(void) ;

#endif