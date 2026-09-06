#ifndef UTILS_UNLOCK_H
#define UTILS_UNLOCK_H

#include <linux/list.h>

/**
 * Struct that contains the unlocking info, to be embedded
 * in another struct containing the lock or a reference to it
 */
typedef struct _unlock_data {
    struct list_head stack ;
    void (*unlock)(struct _unlock_data *) ; 
} unlock_data ;

/**
 * Perform, in LIFO order, the unlocking of a series of locks
 * @param head : pointer to the list_head containing the stack
 */
void perform_unlocking(struct list_head *head) ;


#endif