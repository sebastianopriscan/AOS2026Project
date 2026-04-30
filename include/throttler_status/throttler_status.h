#ifndef THROTTLER_STATUS_H
#define THROTTLER_STATUS_H

typedef enum {
    ON,
    OFF
} THROTTLER_STATUS;

typedef enum {
    THROTTLER_LOCK_READ,
    THROTTLER_LOCK_WRITE
} THROTTLER_LOCK ;

void setup_throttler_status(void) ;
void cleanup_throttler_status(void) ;

int set_throttler_status_on(void) ;
void set_throttler_status_off(void) ;

THROTTLER_STATUS get_throttler_status(void) ;

void up_throttler_status(THROTTLER_LOCK lockKind) ;
THROTTLER_STATUS down_throttler_status(THROTTLER_LOCK lockKind) ;

#endif