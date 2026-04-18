#ifndef API_H
#define API_H

#include <linux/limits.h>

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

typedef unsigned int policy_kind ;

#define POLICY_UID_ONLY ((policy_kind) 0) 
#define POLICY_PROGRAM_ONLY ((policy_kind) 1) 
#define POLICY_UID_AND_PROGRAM ((policy_kind) 2) 

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
    unsigned int *syscalls;
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

/******** Operation setThrottler: ********
    OPCODE    : 0b010
    OPMACRO   : THROTTLER_SET_ENABLE
    OPARG     : char
    OPARGTYPE : IN
    ARGSIZE   : sizeof(char)

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_ENABLE 0x40000000

ssize_t set_throttler_on() ;

/******** Operation setThrottler: ********
    OPCODE    : 0b011
    OPMACRO   : THROTTLER_SET_DISABLE
    OPARG     : char
    OPARGTYPE : IN
    ARGSIZE   : sizeof(char)

    Description : sets the throttler's on/off state
*/
#define THROTTLER_SET_DISABLE 0x60000000

ssize_t set_throttler_off() ;


/*********** Read related API **********/

/**
 * Returns an array containing all the set dumps
 */
throttleA_policy *dump_throttleA_status() ;


/*********** Lifecycle operations **********/

int setup_api(void) ;
void cleanup_api(void) ;

#endif