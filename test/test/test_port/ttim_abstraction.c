#include "ttim.h"
#include "ttim_abstraction.h"
#include <stdio.h>

extern TTIM_TIMEBASE_TYPE time_base_obj;

#define TIMER_RUNNING_FLAG 1

void test_assert( bool condition, const char *fcn, int line )
{
    if ( condition )
    {
    }
    else
    {
        char buffer[500];
        sprintf( buffer, "Assertion failed in %s, line %d", fcn, line );
        TEST_ASSERT_TRUE_MESSAGE( condition, buffer );
    }
}

void test_crtical_start( const char *fcn, int line )
{
    extern int isr_dis;

    isr_dis++;
    /*printf("+1\n"); */

    if ( isr_dis >= 2 )
    {
        printf( "recursive critical section at %s %u", fcn, line );
        TEST_ASSERT( 0 );
    };

    fflush( stdout );
}

void test_crtical_end( void )
{
    extern int isr_dis;
    isr_dis--;
    /* printf("-1\n"); fflush(stdout);*/
}

void timebase_init( mcu_timer_t *hnd )
{
    hnd->timeout = TTIM_TIMEOUT_INVALID;
    hnd->elapsed = 0;
    hnd->flags = 0;
}

void timebase_start( mcu_timer_t *hnd, uint32_t time )
{
    TEST_ASSERT_MESSAGE( time > 0, "the time base is started with 0 ticks" );

    uint32_t temp = ( hnd )->timeout;
    if ( timebase_is_running( hnd ) )
    {
        if ( timebase_is_timedout( hnd ) )
        {
            uint32_t extra_elapsed = ( hnd )->elapsed - ( hnd )->timeout;

            if ( time >= ( extra_elapsed ) )
            {
                ( hnd )->timeout = ( time ) - ( extra_elapsed );
                ( hnd )->elapsed -= temp;
            }
            else
            {
                ( hnd )->timeout = ( time );
                ( hnd )->elapsed -= time;
            }
        }
        else
        {
            if ( ( hnd )->timeout >= ( ( hnd )->elapsed ) + time )
            {
                ( hnd )->elapsed = 0;
            }
            else
            {
            }

            ( hnd )->timeout = ( time );
        }
    }
    else
    {
        ( hnd )->timeout = ( time );
        ( hnd )->elapsed = 0;
    }

    ( hnd )->flags |= TIMER_RUNNING_FLAG;
}

bool timebase_is_running( mcu_timer_t *hnd )
{
    return ( ( hnd )->flags & TIMER_RUNNING_FLAG );
}

bool timebase_is_stopped( mcu_timer_t *hnd )
{
    return !timebase_is_running( hnd );
}

bool timebase_is_timedout( mcu_timer_t *hnd )
{
    return ( hnd->elapsed >= hnd->timeout );
}

/**
   @brief returns the difference between the elapsed timer and the setted timeout
          if not timedout, return -1
 */
int32_t timebase_timedout( mcu_timer_t *hnd )
{
    int32_t dif = hnd->elapsed - hnd->timeout;

    if ( dif >= 0 )
    {
        return ( dif );
    }
    else
    {
        return -1;
    }
}

void timebase_stop( mcu_timer_t *hnd )
{
    ( hnd )->timeout = TTIM_TIMEOUT_INVALID;
    hnd->elapsed = 0;
    ( hnd )->flags &= ~TIMER_RUNNING_FLAG;
}

/**
 * return the elapsed time since the last call to timebase_start
 */
uint32_t timebase_get_elapsed( mcu_timer_t *hnd )
{
    return hnd->elapsed;
}

uint32_t timebase_get_current_to( mcu_timer_t *hnd )
{
    return hnd->timeout;
}

uint32_t timebase_get_remaining( mcu_timer_t *hnd )
{
    TEST_ASSERT( ( hnd )->timeout >= hnd->elapsed );
    uint32_t rv = ( hnd )->timeout - hnd->elapsed;
    if ( rv == 0 )
    {
        /* just timedout */
    }
    return rv;
}

void timebase_reset_elapsed( mcu_timer_t *hnd )
{
    hnd->elapsed = 0;
}

void timebase_add_elapsed( mcu_timer_t *hnd, uint32_t time )
{
    hnd->elapsed += ( time );
}
