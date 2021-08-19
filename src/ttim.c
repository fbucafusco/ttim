#include <stdlib.h>
#include "ttim.h"

/**
   @brief STATES OF EACH TIMER
    paused count                   remining_time           state
    0      TTIM_INVALID_TIME       TTIM_INVALID_TIME       init
    0      !=TTIM_INVALID_TIME     TTIM_INVALID_TIME       stopped
    0      !=TTIM_INVALID_TIME     !=TTIM_INVALID_TIME     running
    1      !=TTIM_INVALID_TIME     !=TTIM_INVALID_TIME     paused
    1      !=TTIM_INVALID_TIME     TTIM_INVALID_TIME       timedout
 */

/* PRIVATE OBJECTS */

/* entry point to the running list. */
ttim_list_t  ttim_list;

#ifdef TTIM_TIMEBASE_TYPE
/* timebase object */
TTIM_TIMEBASE_TYPE time_base_obj;
#endif

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
ttim_t ttim_group[TTIM_COUNT];
#endif

/* PRIVATE DEFINES */
#define TTIM_INVALID_NEXT    ((void*)(~((size_t)0 )))

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
#define TTIM_GET_HND(PTR)  ( ( char* ) (PTR) - ( char* )ttim_group )  / sizeof( ttim_group[0] )
#endif

#ifndef TTIM_STATIC
#define TTIM_STATIC static inline
#endif

/* PRIVATE DEFAULT VALUES ======================================================================= */

/**
   @brief invalidate a node.
            NOT THREAD SAFE
            PRIVATE METHOD

   @param node
   @return TTIM_STATIC
 */
TTIM_STATIC bool _ttim_node_invalidate( ttim_node_t* node )
{
    node->t     = TTIM_INVALID_TIME;
    node->next  = TTIM_INVALID_NEXT;
}


/**
   @brief returns if the node is valid
            NOT THREAD SAFE
            PRIVATE METHOD
   @param node
   @return true
   @return false
 */
TTIM_STATIC bool _ttim_node_is_valid( ttim_t* node )
{
    return node != TTIM_INVALID_NEXT;
}

/**
   @brief given a timer object pointer, it returns the handler

   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC TTIM_HND_T _ttim_get_hnd( void* ptr )
{
#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    return TTIM_GET_HND( ptr );
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    TTIM_ASSERT( ptr!=NULL );
    return  ( ptr );
#endif
}

#if 0
TTIM_COUNT_T _ttim_get_timeout( TTIM_HND_T hnd )
{
    return ttim_group[hnd].count ;
}
#endif

/**
   @brief returns if the time value is valid

   @param time
   @return true
   @return false
 */
TTIM_STATIC  bool _ttim_time_is_valid( TTIM_COUNT_T time )
{
    return time != TTIM_INVALID_TIME;
}


/**
   @brief returns if the list is empty

   @param list
   @return true
   @return false
 */
TTIM_STATIC bool _ttim_list_is_empty( ttim_list_t* list )
{
    if( _ttim_node_is_valid( list->next )   )
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
   @brief returns the tick count from the last call to _ttim_timebase_start

   @return TTIM_COUNT_T
 */
TTIM_STATIC TTIM_COUNT_T _ttim_timebase_elapsed()
{
    /* this is a low level function that relies on the user configuiration */
    TTIM_COUNT_T rv ;

    /* this fcn is called always when a timer is running so in every case, the timebase must be running.*/

#ifdef TTIM_TIMEBASE_TYPE
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) );
#else
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING() );
#endif


#if TTIM_PERIODIC_TICK==1
    /* for periodic ticks there is no remaining offline elapsed time, because there is one tick per count. */
    rv = 0;
#else
    rv = TTIM_TIMEBASE_ELAPSED( &time_base_obj );
#endif
    return rv;
}

/**
   @brief   Removes a timer from the list.
            NOT THREAD SAFE
   @param   hnd
   @return  TTIM_COUNT_T Remaining time that had left the removed node.
 */
TTIM_STATIC TTIM_COUNT_T _ttim_list_remove( ttim_list_t* list, TTIM_HND_T hnd )
{
    TTIM_COUNT_T rv;
    ttim_node_t* node ;
    ttim_node_t* node_bck;

    node        = list->next ;

    /* upcast */
    node_bck    = ( ttim_node_t* ) list;

    rv = list->t;

    while( 1 )
    {
        bool not_found = ( TTIM_INVALID_NEXT == node ) ;
        bool found;

        if(  true == not_found )
        {
            /* not found */
            rv = TTIM_INVALID_TIME;
            break;
        }

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
        found = ( &ttim_group[hnd] == node ) ;
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
        found = ( hnd == node );
#endif

        if( true == found )
        {
            /* found */
            node_bck->next  = node->next;
            node_bck->t    += node->t;

            _ttim_node_invalidate( ( ttim_node_t* ) node );

            break;
        }

        rv         += node->t;
        node_bck    = node;
        node        = node->next;
    }

    return rv;
}

/**
   @brief   returns if the timer has paused
            NOT THREAD SAFE
   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC bool _ttim_is_paused( TTIM_HND_T hnd )
{
    ttim_t* tim;

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( hnd!=NULL );
#endif

    return ( tim->remining_time != TTIM_INVALID_TIME   &&
             tim->count         != TTIM_INVALID_TIME   &&
             tim->paused        == 1   );
}

/**
   @brief   returns if the timer is stopped
            NOT THREAD SAFE
   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC bool _ttim_is_stopped( TTIM_HND_T hnd )
{
    ttim_t* tim;
#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( hnd!=NULL );
#endif

    return ( tim->remining_time == TTIM_INVALID_TIME   &&
             tim->count         != TTIM_INVALID_TIME   &&
             tim->paused        == 0   );
}

/**
   @brief Returns if the tier has timedout.
          NOT THREAD SAFE
   @param hnd
   @return bool
 */
TTIM_STATIC bool _ttim_is_timedout( TTIM_HND_T hnd )
{
    bool rv;
    ttim_t* tim;
#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim!=NULL );
#endif
    rv  = ( tim->remining_time  == TTIM_INVALID_TIME );
    rv  = rv && ( tim->count          != TTIM_INVALID_TIME );
    rv  = rv &&  ( tim->paused == 1 );
    return rv;
}

/**
   @brief returns if the timer is running
          NOT THREAD SAFE

   @param hnd
   @return bool
 */
TTIM_STATIC bool _ttim_is_running( TTIM_HND_T hnd )
{
    bool rv;
    ttim_t* tim;

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim!=NULL );
#endif

    rv  = ( tim->remining_time  != TTIM_INVALID_TIME );
    rv  = rv && ( tim->count          != TTIM_INVALID_TIME );
    rv  = rv &&  ( tim->paused == 0 );
    return rv;
    return ( tim->remining_time  != TTIM_INVALID_TIME   &&
             tim->count          != TTIM_INVALID_TIME   &&
             tim->paused == 0 );
}

/**
   @brief   calculates the remining time for a given timer, without taking care the elapsed timebase time.
            NOT THREAD SAFE

   @param hnd
   @return TTIM_COUNT_T   TTIM_INVALID_TIME if there are not timers started of ti the handler is not stated
                          other             if the timer related to the hnd has started
 */
TTIM_STATIC TTIM_COUNT_T _ttim_remaining_time( TTIM_HND_T hnd )
{
    TTIM_COUNT_T rv;
    TTIM_HND_T i;
    ttim_node_t* node = ttim_list.next ;
    ttim_node_t* tim;

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim!=NULL );
#endif

    rv = ttim_list.t;

    if( rv != TTIM_INVALID_TIME )
    {
        while( 1 )
        {
            if( TTIM_INVALID_NEXT == node )
            {
                /* not found */
                rv = TTIM_INVALID_TIME;
                break;
            }

            if( tim == node )
            {
                /* found */
                break;
            }

            rv += node->t;
            node = node->next;
        }
    }
    return rv;
}

/**
   @brief returns if there is any timer running

   @return bool true:   there is at least one timer running
                false:  there aren't any timer running
 */
TTIM_STATIC bool _ttim_is_any_running( )
{
    int rv;

    TTIM_CRITICAL_START();

    if( ttim_list.next == TTIM_INVALID_NEXT )
    {
        rv = false;
    }
    else
    {
        rv = true;
    }

    TTIM_CRITICAL_END();

    return rv;
}

/**
   @brief   starts the time base with a given timeout.
            if the timebase is running, the timeout is changed relative to the
            moment of the call of this method.

   @param time
 */
TTIM_STATIC void _ttim_timebase_start( TTIM_COUNT_T time )
{
    bool start = false;

#if TTIM_PERIODIC_TICK==1
    /* start the timebase */
    if( !TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) )
    {
        /* the time base is not running */
        start = true;
    }
#else
    /* For TTIM_PERIODIC_TICK==0 the timeout could be different, so it  must be changed */

    if( ttim_list.t >= time )
    {
        /* it only starts the timer, if there is less or equal to de actual timeout.
           If not, the current timeout still running because the new timeout is greater than
           the 1st delta t, that is related to the next timer in de running list.   */
        start = true;
    }
#endif

    if( start )
    {
#ifdef TTIM_TIMEBASE_TYPE
        TTIM_TIMEBASE_START( &time_base_obj, time );
#else
        TTIM_TIMEBASE_START( time );
#endif
    }

#if defined(TTIM_TIMEBASE_TYPE) && defined(TTIM_TIMEBASE_IS_RUNNING)
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) );
#endif
}

/**
   @brief   logically stops the timer
            NOT THREAD SAFE

   @param hnd
 */
TTIM_STATIC void _ttim_stop( TTIM_HND_T hnd )
{
    ttim_t* tim;

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    TTIM_ASSERT( tim!=NULL );
    tim = hnd;
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#endif

    tim->node.t        = TTIM_INVALID_TIME;
    tim->paused            = 0;
}

/**
   @brief Stops the timebase if there is no more timers running.

   @return true    there still are timers that need for the time base, the timebase didn't stop
   @return false   the timebase has stopped
 */
TTIM_STATIC bool _ttim_timebase_stop()
{
    bool any_timer_running = _ttim_is_any_running();

    if( !any_timer_running )
    {
        /* turn off he timebase */
#ifdef TTIM_TIMEBASE_TYPE
        TTIM_TIMEBASE_STOP( &time_base_obj );
#else
        TTIM_TIMEBASE_STOP();
#endif
    }
    else
    {
        _ttim_timebase_start( ttim_list.t );
    }

    return any_timer_running;
}

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
/**
   @brief individual timer destructor

   @param hnd   Timer handler
 */
TTIM_HND_T ttim_dtor( TTIM_HND_T hnd )
{
#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    TTIM_ASSERT( hnd    !=NULL );
    TTIM_FREE( hnd );
#endif
}
#endif

/**
   @brief individual timer constructor

   @param hnd   Timer handler
 */
TTIM_HND_T ttim_ctor( TTIM_HND_T hnd )
{
    ttim_t* timer_new;

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    hnd =  TTIM_MALLOC( sizeof( ttim_t ) );
    if( hnd != NULL )
    {
        timer_new = hnd;

#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        timer_new = &ttim_group[hnd];
#endif
        TTIM_CRITICAL_START();

        _ttim_node_invalidate( ( ttim_node_t* ) timer_new );

        timer_new->count            = TTIM_INVALID_TIME;
        timer_new->remining_time    = TTIM_INVALID_TIME;
        timer_new->paused           = 0;

#if TTIM_CB_MODE!=TTIM_CB_MODE_NONE
        timer_new->timeout_callback = NULL;
#endif

#if TTIM_CB_MODE==TTIM_CB_MODE_PARAM
        timer_new->timeout_param  = NULL;
#endif

        TTIM_CRITICAL_END();
    }
    else
    {
        hnd = TTIM_INVALID_HND;
    }
    return hnd;
}

/**
   @brief init the ttim module
 */
void ttim_init()
{
    TTIM_HND_T hnd;

    TTIM_CRITICAL_START();

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    for( hnd = 0 ; hnd < TTIM_COUNT ; hnd++ )
    {
        ttim_ctor( hnd ) ;
    }
#endif

    _ttim_node_invalidate( ( ttim_node_t* ) &ttim_list );

    TTIM_CRITICAL_END();

#ifdef TTIM_TIMEBASE_TYPE
    TTIM_TIMEBASE_INIT( &time_base_obj );
#else
    TTIM_TIMEBASE_INIT( time );
#endif
}

/**
   @brief           returns if the timer is stopped
   @param hnd       Timer handler
   @return bool     true/false
 */
bool ttim_is_stopped( TTIM_HND_T hnd )
{
    bool rv;
    TTIM_CRITICAL_START();
    rv = _ttim_is_stopped( hnd );
    TTIM_CRITICAL_END();
    return rv;
}

/**
   @brief           returns if the timer is timedout
   @param hnd       Timer handler
   @return bool     true/false
 */
TTIM_STATIC bool ttim_is_timedout( TTIM_HND_T hnd )
{
    bool rv;
    TTIM_CRITICAL_START();
    rv = _ttim_is_timedout( hnd );
    TTIM_CRITICAL_END();
    return rv;
}

/**
   @brief stop, configure and restarts

   @param hnd   Timer handler
   @param time  time for timeout the timer, once started.
   @param cb    pointer to a callback that will be called when timeout. Valid for TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
   @param param parameter that will be passed asargument to the callback, when the timers timesout. Valid for TTIM_CB_MODE==TTIM_CB_MODE_PARAM
 */
#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb )
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb,void*param )
#else
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time )
#endif
{
    ttim_stop( hnd );

#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
    ttim_set( hnd, time, cb );
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
    ttim_set( hnd, time, cb, param );
#else
    ttim_set( hnd, time );
#endif

    ttim_start( hnd );
}

/**
   @brief   Sets the timeout value. The timer do not start until ttim_start is called.
            If it was running, the timer starts with the new timeout value (from zero)
 */
#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb )
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb,void*param )
#else
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time )
#endif
{
    ttim_t* timer;

    if( time == TTIM_INVALID_TIME )
    {
        return;
    }

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    if( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();

        if( _ttim_is_running( hnd ) )
        {
            /* Si esta corriendo, frenamos, cambiamos, arrancamos. TODO: esta es la salida facil ! */
#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
            ttim_set_n_start( hnd, time, cb );
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
            ttim_set_n_start( hnd, time, cb, param );
#else
            ttim_set_n_start( hnd, time );
#endif
        }
        else
        {
            timer->count = time;

#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
            timer->timeout_callback = cb;
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
            timer->timeout_callback = cb;
            timer->timeout_param = param;
#else
#endif

        }

        TTIM_CRITICAL_END();
    }
}

/**
   @brief Starts the timer. If the timer was paused, it resumes the last count.

   @param hnd
 */
void ttim_start( TTIM_HND_T hnd )
{
    ttim_t* timer;

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    if( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        //printf(" on start timer %u to %u rem %u  \n",  hnd, timer->count , timer->remining_time   );

        TTIM_CRITICAL_START();

        if( _ttim_is_running( hnd ) )
        {
            /* already running : exit */
            TTIM_CRITICAL_END();
            return;
        }

        /* the timer was not setted, exit */
        if( timer->count == TTIM_INVALID_TIME )
        {
            TTIM_CRITICAL_END();
            return;
        }

        /* up cast the list to ttim_t structure */
        ttim_t* node  = ( ttim_t* ) &ttim_list;

        TTIM_COUNT_T count_to ;

        /* if the timer was paused or not */
        if( !_ttim_is_paused( hnd ) )
        {
            /* it was not paused */
            count_to = timer->count;
        }
        else
        {
            /* it was paused */
            count_to = timer->remining_time;
        }

        /* Evaluate if the running list is empty. If so, the ner timer is the next node */
        if( _ttim_list_is_empty( &ttim_list ) )
        {
            timer->node.next            = TTIM_INVALID_NEXT;
            timer->node.t               = TTIM_INVALID_TIME;

            timer->remining_time        = count_to ;

            ttim_list.next             = timer ;    //node->next =   = timer_new ;


            ttim_list.t       = count_to; // node->t            = count_to;
        }
        else
        {
            TTIM_COUNT_T temp = 0 ; //node->t; //campo ttim_list.t

            // Insert
            while( 1 )
            {
                if( node->node.next == TTIM_INVALID_NEXT )
                {
                    /* reach the last node in the list, insert at the end  */
                    break;
                }

                temp += node->node.t ;

                if( temp - _ttim_timebase_elapsed() >= count_to   )
                {
                    /* insert the new timer between node and node->next */
                    break;
                }

                node = node->node.next;
            }


            if( node->node.next == TTIM_INVALID_NEXT )
            {
                /* reach the last node in the list, insert at the end  */
                _ttim_node_invalidate( ( ttim_node_t* ) timer );

                node->node.next         = timer ;
                node->node.t            = count_to - ( temp - _ttim_timebase_elapsed() )  ;
            }
            else
            {
                timer->node.next        = node->node.next;
                node->node.next         = timer ;

                timer->node.t           = temp - count_to - _ttim_timebase_elapsed();

                if( node == ( ttim_t* )  &ttim_list )
                {
                    node->node.t        = count_to;    //there were nodes but the new one wil be de next
                }
                else
                {
                    node->node.t        -= timer->node.t;
                }
            }

            timer->remining_time    = count_to ;
        }

        timer->paused = 0;

        TTIM_CRITICAL_END();

        /* starts the time base, conditionally.
           This action is a system process that should not be within a critical section. */
        _ttim_timebase_start( ttim_list.t );
    }
}



/**
   @brief Resets the count and stops.
   @param hnd
 */
void ttim_stop( TTIM_HND_T hnd )
{
    ttim_t* timer;

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    if( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();

        /* remove the timer from te timer list */
        _ttim_list_remove( &ttim_list, hnd );

        timer->remining_time = TTIM_INVALID_TIME;
        timer->paused        = 0;
        //_ttim_stop(hnd);

        TTIM_CRITICAL_END();

        /* Any system process should be done outside a critical section.  */
        _ttim_timebase_stop();
    }
}

/**
   @brief Pauses the timer keeping the internal count intact.
   @param hnd
 */
void ttim_pause( TTIM_HND_T hnd )
{
    ttim_t* timer;

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    if( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_COUNT_T res;

        TTIM_CRITICAL_START();

        /* removes the node */
        res = _ttim_list_remove( &ttim_list, hnd );

        if( res != TTIM_INVALID_TIME )
        {
            /* the node existed but was removed, so, update the t information */
            timer->remining_time     = res - _ttim_timebase_elapsed() ;
            //printf(" on pause timer %u to %u rem %u\n",  hnd, res, timer->remining_time);
            //this line is removed because is done within _ttim_list_remove
            //  timer->t        = TTIM_INVALID_TIME;
        }

        timer->paused = 1;

        TTIM_CRITICAL_END();

        /* Any system process should be done outside a critical section.  */

        _ttim_timebase_stop();

        /*
            return ( tim->remining_time != TTIM_INVALID_TIME   &&
                     tim->count         == TTIM_INVALID_TIME   &&
                     tim->paused        == 1   ); */
    }
}

/**
   @brief reset the internal count and start de timer again
   @param hnd
 */
void ttim_reset_n_restart( TTIM_HND_T hnd )
{
#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    if( hnd != TTIM_INVALID_HND )
    {
#elif TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    if( hnd < TTIM_COUNT )
    {
        hnd = hnd;
#endif
        /* improve.
           This implementation involvesd 2 whiles,
           so the time copexity is O(2 N).
           I think that could be done in O(N) */
        ttim_stop( hnd );
        ttim_start( hnd );
    }
}

/**
   @brief   Returns the absolute t time for a given timer.

   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_COUNT_T ttim_get_remining_time( TTIM_HND_T hnd )
{
    TTIM_CRITICAL_START();

    TTIM_COUNT_T rv = _ttim_remaining_time( hnd );

    if( TTIM_INVALID_TIME != rv )
    {
        rv = rv - _ttim_timebase_elapsed();
    }

    TTIM_CRITICAL_END();
    return rv;
}

/**
   @brief Function that have to be called within the timebase isr handler.
            For TTIM_PERIODIC_TICK==1 should be called in the periodic tick handler, and will
            increment the parial count in 1.
            For TTIM_PERIODIC_TICK==0 should be called from the one shot timer handler, and will
            timeout the sooner active timer that is in the running list.
 */
void ttim_update()
{
    TTIM_CRITICAL_START();

#if( TTIM_PERIODIC_TICK==1 )
    /* is there any timer running? */
    if( TTIM_INVALID_NEXT == ttim_list.next )
    {
        TTIM_CRITICAL_END();
        return;
    }
#else
    /* in this mode the running list should be at least with one element. */
    TTIM_ASSERT( TTIM_INVALID_NEXT != ttim_list.next );
#endif

    /* up cast the next to a node structure */
    ttim_t* node = ttim_list.next;

#if TTIM_PERIODIC_TICK==1
    ttim_list.t -= 1; /* decrease 1 time unit   */
#else
    ttim_list.t = 0;
#endif

    while( ( 0 == ttim_list.t )  && ( TTIM_INVALID_NEXT != ttim_list.next ) )
    {
        node = ttim_list.next;

        /* some timer did timedout  */
        TTIM_HND_T hnd   = _ttim_get_hnd( ttim_list.next );

        /* execute the callback */
        callback_t* timeout_callback = ( callback_t* ) node->timeout_callback;

        if( NULL != timeout_callback )
        {
#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
            timeout_callback( hnd );
#endif

#if TTIM_CB_MODE==TTIM_CB_MODE_PARAM
            timeout_callback( hnd, node->timeout_param );
#endif
        }

        ttim_t* tim;

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
        tim = &ttim_group[hnd];
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
        tim = hnd;
        TTIM_ASSERT( tim!=NULL );
#endif

        TTIM_ASSERT( tim==node );

        /* update the node jumping the current one, that is invalid */
        ttim_list.next  = node->node.next;
        ttim_list.t     = node->node.t;

        /* the timer turns off. Set to timedout state  */
        _ttim_node_invalidate( ( ttim_node_t* ) tim );

        tim->paused         = 1;
        tim->remining_time  = TTIM_INVALID_TIME;

        TTIM_ASSERT( ! _ttim_is_running( hnd ) );
    }

#if defined(TTIM_TIMEBASE_TYPE) && defined(TTIM_TIMEBASE_IS_RUNNING)
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) );
#endif

    TTIM_CRITICAL_END();

    /* Any system process should be done outside a critical section.  */

    /* stops the timebase */
    bool any_timer_running  = _ttim_timebase_stop();

    if( !any_timer_running )
    {
    }
    else
    {
#if TTIM_PERIODIC_TICK==1
        _ttim_timebase_start( TTIM_RESOLUTION );
#else
        _ttim_timebase_start( ttim_list.t );
#endif
    }
}

/* VALIDATIONS ******************************************************************* */
#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC && ( !defined(TTIM_MALLOC) || !defined(TTIM_FREE) )
#error For TTIM_MM_MODE_DYNAMIC ttim_config.h must define wrappers TTIM_MALLOC and TTIM_FREE
#endif

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC && ( !defined(TTIM_COUNT)   )
#error For TTIM_MM_MODE_STATIC ttim_config.h must define TTIM_COUNT
#endif

#if TTIM_PERIODIC_TICK==1 && ( !defined(TTIM_RESOLUTION)   )
#error For TTIM_PERIODIC_TICK==1 ttim_config.h must define TTIM_RESOLUTION that will be used to start the timer via TTIM_TIMEBASE_START macro.
#endif

#if  !defined(TTIM_TIMEBASE_TYPE)
#error ttim_config.h must define TTIM_TIMEBASE_TYPE
#endif

#ifndef TTIM_TIMEBASE_IS_RUNNING
#error ttim_config.h must define wrappers for TTIM_TIMEBASE_IS_RUNNING
#endif

#ifndef TTIM_TIMEBASE_STOP
#error ttim_config.h must define wrappers for TTIM_TIMEBASE_STOP
#endif

#ifndef TTIM_TIMEBASE_START
#error ttim_config.h must define wrappers for TTIM_TIMEBASE_START
#endif


/* v1.04 */
