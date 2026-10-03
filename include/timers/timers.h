#ifndef TIMERS_H
#define TIMERS_H

/**
 * This function that sets up a one second periodic timer managing
 * refreshing of the throttle counters and telemetry
 */
void setup_timers(void) ;

/**
 * This function cancels the refreshing timer and releases every
 * ticket taken so far, waking up all the throttled tasks
 */
void cleanup_timers(void) ;

unsigned long take_ticket(void) ;

bool ticket_served(unsigned long ticket) ;

/**
 * This function allows a task to put itself to sleep until its ticket
 * is served by one of the next one second windows.
 * 
 * @returns 0 once the ticket is served, -ERESTARTSYS if a fatal
 *          signal is pending
 */
int throttle(unsigned long ticket) ;

#endif