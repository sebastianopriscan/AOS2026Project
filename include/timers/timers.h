#ifndef TIMERS_H
#define TIMERS_H

/**
 * This function that sets up a one second periodic timer managing
 * refreshing of the throttle counters and telemetry
 */
void setup_timers(void) ;

/**
 * This function cancels the refreshing timer
 */
void cleanup_timers(void) ;

/**
 * This function allows a task to put itself to sleep until
 * the next one second window
 */
void throttle(void) ;

#endif