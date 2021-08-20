
#include "ttim.h"
#include "ttim_abstraction.h"

extern TTIM_TIMEBASE_TYPE time_base_obj;

#define TIMER_RUNNING_FLAG  1

void timebase_init( mcu_timer_t* hnd )
{
    hnd->timeout=TTIM_TIMEOUT_INVALID;
    hnd->elapsed = 0;

    hnd->flags = 0;
}

void timebase_start( mcu_timer_t* hnd, uint32_t time )
{
    uint32_t temp = ( hnd )->timeout;
    if( timebase_is_running( hnd ) )
    {
        if( timebase_is_timedout( hnd ) )
        {
            uint32_t extra_elapsed = ( hnd )->elapsed - ( hnd )->timeout;

            if( time >= ( extra_elapsed ) )
            {
                ( hnd )->timeout     = ( time ) - ( extra_elapsed ) ;
                ( hnd )->elapsed    -= temp;
            }
            else
            {
                ( hnd )->timeout     = ( time );
                ( hnd )->elapsed    -= time;
            }
        }
        else
        {
            if(  ( hnd )->timeout >= ( ( hnd )->elapsed ) + time )
            {
                ( hnd )->elapsed = 0;
            }
            else
            {

            }

            ( hnd )->timeout =    ( time )  ;
        }
    }
    else
    {
        ( hnd )->timeout = ( time ) ;
        ( hnd )->elapsed = 0;
    }

    ( hnd )->flags |=TIMER_RUNNING_FLAG;
}

bool timebase_is_running( mcu_timer_t* hnd )
{
    return ( ( hnd )->flags & TIMER_RUNNING_FLAG );
}

bool timebase_is_stopped( mcu_timer_t* hnd )
{
    return !timebase_is_running( hnd );
}

bool timebase_is_timedout( mcu_timer_t* hnd )
{
    return  ( hnd->elapsed >= hnd->timeout );
}

/**
   @brief returns the difference between the elapsed timer and the setted timeout
          if not timedout, return -1
 */
int32_t timebase_timedout( mcu_timer_t* hnd )
{
    int32_t dif = hnd->elapsed - hnd->timeout;

    if( dif>=0 )
    {
        return  ( dif  );
    }
    else
    {
        return -1;
    }
}


void timebase_stop( mcu_timer_t* hnd  )
{
    ( hnd )->timeout   = TTIM_TIMEOUT_INVALID;
    hnd->elapsed = 0;
    ( hnd )->flags &= ~TIMER_RUNNING_FLAG;
}

/**
 * return the elapsed time since the last call to timebase_start
 */
uint32_t timebase_get_elapsed( mcu_timer_t* hnd )
{
    return hnd->elapsed;
}

uint32_t timebase_get_current_to( mcu_timer_t* hnd )
{
    return hnd->timeout;
}

uint32_t timebase_get_remaining( mcu_timer_t* hnd )
{
    TEST_ASSERT( ( hnd )->timeout >= hnd->elapsed );
    uint32_t rv = ( hnd )->timeout - hnd->elapsed;
    if( rv ==0 )
    {
        /* just timedout */

    }
    return rv;
}


void timebase_reset_elapsed( mcu_timer_t* hnd )
{
    hnd->elapsed       = 0 ;
}

void timebase_add_elapsed( mcu_timer_t* hnd, uint32_t time )
{
    hnd->elapsed        += ( time );
}
