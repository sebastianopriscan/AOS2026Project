#ifndef THROTTLER_STATUS_H
#define THROTTLER_STATUS_H

typedef enum {
    ON,
    OFF
} THROTTLER_STATUS;

void setup_throttler_status(void) ;
void cleanup_throttler_status(void) ;

void set_throttler_status_on(void) ;
void set_throttler_status_off(void) ;

THROTTLER_STATUS get_throttler_status() ;

#endif