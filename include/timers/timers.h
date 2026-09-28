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
 * the next one second window or, in case the sleep happens during
 * a refresh window, when the refresh is over.
 * 
 * @returns 0 in case the throttler did not run during a refresh,
 *          1 otherwise
 */
void throttle(void) ;

#endif