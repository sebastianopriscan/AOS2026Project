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

enum policy_kind {
    POLICY_UID_ONLY = 0,
    POLICY_PROGRAM_ONLY = 1,
    POLICY_UID_AND_PROGRAM = 2
} ;
typedef enum policy_kind policy_kind ;

struct inode_descriptor {
    dev_t device_id ;
    unsigned long inode_number ;
} ;
typedef struct inode_descriptor inode_descriptor ;

/**
 * Policy for the throttler. Depending on the policy_kind field, it will
 * enable throttling for a determinate user-ID and/or program name.
 * 
 * The relation between a policy_kind and the actual policy is the following:
 * 
 * - POLICY_UID_ONLY : The policy applies by user-ID
 * - POLICY_PROGRAM_ONLY : The policy applies by program name
 * - POLICY_UID_AND_PROGRAM : The policy applies when the specified user-ID runs the
 *                            specified program
 * A copy of the struct is allocated when the user passes it as a param
 */
struct _throttleA_policy {
    policy_kind policy;
    unsigned int uid ;
    throttleA_path path;
    unsigned int tolerance ;
    TYPE_ARRAY_PER_LIMIT(unsigned long, syscalls);
} ;
typedef struct _throttleA_policy throttleA_policy ;


/******** Operation addPolicy: ********
    OPCODE    : 0b000
    OPMACRO   : ADD_POLICY
    OPARG     : struct throttleA_policy
    OPARGTYPE : IN
    ARGSIZE   : sizeof(struct throttleA_policy)

    Description : adds a policy to the throttling manager.
*/
#define ADD_POLICY 0x00000000


ssize_t throttleA_policy_add(throttleA_policy *) ;

/******** Operation removePolicy: ********
    OPCODE    : 0b001
    OPMACRO   : RM_POLICY
    OPARG     : struct throttleA_policy
    OPARGTYPE : IN
    ARGSIZE   : sizeof(struct throttleA_policy)

    Description : removes a given policy
*/
#define RM_POLICY 0x20000000

ssize_t throttleA_policy_rm(throttleA_policy *) ;

/******** Operation deletePolicy: ********
    OPCODE    : 0b100
    OPMACRO   : DELETE_POLICY
    OPARG     : struct throttleA_policy
    OPARGTYPE : IN
    ARGSIZE   : sizeof(struct throttleA_policy)

    Description : deletes the given syscalls from the given policy
*/
#define DELETE_POLICY 0x80000000

ssize_t throttleA_policy_delete(throttleA_policy *) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b010
    OPMACRO   : THROTTLER_SET_ENABLE
    OPARG     : char
    OPARGTYPE : IN
    ARGSIZE   : sizeof(char)

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_ENABLE 0x40000000

ssize_t set_throttler_on(void) ;

/******** Operation setThrottler: ********
    OPCODE    : 0b011
    OPMACRO   : THROTTLER_SET_DISABLE
    OPARG     : char
    OPARGTYPE : IN
    ARGSIZE   : sizeof(char)

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_DISABLE 0x60000000

ssize_t set_throttler_off(void) ;


/*********** Read related API **********/

/**
 * Returns an array containing all the set dumps
 */
throttleA_policy *dump_throttleA_status(void) ;


/*********** Lifecycle operations **********/

int setup_api(void) ;
void cleanup_api(void) ;

#endif