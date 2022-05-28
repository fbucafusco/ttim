#ifndef TTIM_CONFIG_H
#define TTIM_CONFIG_H

#include "unity.h"
#include "ttim_abstraction.h"

#define DEBUG_DURING_TESTING      0
#define TTIM_CALC_STATS      1
#define TTIM_STATIC

/* GENERAL =============================================================================================================================== */
#if DEBUG_DURING_TESTING==1
#define TTIM_ASSERT(A)                  TEST_ASSERT_TRUE_MESSAGE( A , "TTIM INTERNAL ERROR" )
#endif

#define TTIM_CB_MODE                    TTIM_CB_MODE_PARAM

#ifdef TEST_STATIC
#define TTIM_MM_MODE                    TTIM_MM_MODE_STATIC
#define TTIM_COUNT                      10
#endif

#ifdef TEST_DYNAMIC
#define TTIM_MM_MODE                    TTIM_MM_MODE_DYNAMIC
#define TTIM_MALLOC(SIZE)               malloc(SIZE)
#define TTIM_FREE(PTR)                  free(PTR)
#endif

/* Critical section */
#define TTIM_CRITICAL_START()        extern int isr_dis; isr_dis++; /*printf("+1\n"); */ if( isr_dis >= 2)  { printf("recursive critical section at %s %u", __FUNCTION__, __LINE__); TEST_ASSERT( 0 );  } ; fflush(stdout);
#define TTIM_CRITICAL_END()          isr_dis--; /* printf("-1\n"); fflush(stdout);*/

/* TIMEBASE LL CONFIGURATION FOR TESTING ================================================================================================ */
#define TTIM_TIMEBASE_TYPE                          mcu_timer_t
#define TTIM_TIMEOUT_INVALID                        -1
#define TTIM_TIMEBASE_INIT(TIMER_HND)               timebase_init( TIMER_HND )
#define TTIM_TIMEBASE_START(TIMER_HND,TIME)         timebase_start( TIMER_HND , TIME );

#define TTIM_TIMEBASE_IS_RUNNING(TIMER_HND)         timebase_is_running(TIMER_HND)
#define TTIM_TIMEBASE_IS_STOPPED(TIMER_HND)         timebase_is_stopped(TIMER_HND)

#define TTIM_TIMEBASE_STOP(TIMER_HND)               timebase_stop(TIMER_HND)

#define TTIM_TIMEBASE_ELAPSED(TIMER_HND)            timebase_get_elapsed(TIMER_HND)
#define TTIM_TIMEBASE_REMAINING(TIMER_HND)          timebase_get_remaining(TIMER_HND)

/* JUST FOR TESTING */
#define TTIM_TIMEBASE_ADD_ELAPSED(TIMER_HND , TIME) timebase_add_elapsed(TIMER_HND , TIME )
#define TTIM_TIMEBASE_TIMEDOUT(TIMER_HND)           timebase_is_timedout(TIMER_HND)

#endif // TTIM_CONFIG_H
