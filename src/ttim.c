#include <stdlib.h>

#include "ttim.h"

/* VALIDATIONS ******************************************************************* */
#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC && (!defined(TTIM_MALLOC) || !defined(TTIM_FREE))
#error For TTIM_MM_MODE_DYNAMIC ttim_config.h must define wrappers TTIM_MALLOC and TTIM_FREE
#endif

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC && (!defined(TTIM_COUNT))
#error For TTIM_MM_MODE_STATIC ttim_config.h must define TTIM_COUNT
#endif

#if TTIM_PERIODIC_TICK == 1
#ifndef TTIM_TIMEBASE_ELAPSED
#error TTIM_TIMEBASE_ELAPSED in ttim_config.h is not needed
#endif
#ifndef TTIM_RESOLUTION
#error For TTIM_PERIODIC_TICK==1 ttim_config.h must define TTIM_RESOLUTION that will be used to start the timer via TTIM_TIMEBASE_START macro.
#endif
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
ttim_list_t ttim_list;

#ifdef TTIM_TIMEBASE_TYPE
/* timebase object */
TTIM_TIMEBASE_TYPE time_base_obj;
#endif

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
ttim_t ttim_group[TTIM_COUNT];
#endif

/* PRIVATE DEFINES */
#define TTIM_INVALID_NEXT ((void *)(~((size_t)0)))

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
#define TTIM_GET_HND(PTR) ((char *)(PTR) - (char *)ttim_group) / sizeof(ttim_group[0])
#endif

#ifndef TTIM_STATIC
#define TTIM_STATIC static inline
#endif

#if TTIM_CALC_STATS == 1
uint32_t ttim_loops_on_remove_count = 0;
uint32_t ttim_remove_count = 0;
uint32_t ttim_loops_on_remining_count = 0;
uint32_t ttim_remining_count = 0;
uint32_t ttim_loops_on_start_count = 0;
uint32_t ttim_start_count = 0;
uint32_t ttim_loops_on_reinsert_count = 0;
uint32_t ttim_reinsert_count = 0;
uint32_t ttim_loops_on_update_count = 0;
uint32_t ttim_update_count = 0;
#endif

/* PRIVATE DEFAULT VALUES ======================================================================= */

/**
   @brief invalidate a node.
            NOT THREAD SAFE
            PRIVATE METHOD

   @param node
   @return TTIM_STATIC
    */
TTIM_STATIC void _ttim_node_invalidate( ttim_node_t *node )
{
    node->t = TTIM_INVALID_TIME;
    node->next = TTIM_INVALID_NEXT;
}

/**
   @brief returns if the node is valid
            NOT THREAD SAFE
            PRIVATE METHOD
   @param node
   @return true
   @return false
 */
TTIM_STATIC bool _ttim_node_is_valid( ttim_node_t *node )
{
    return node != TTIM_INVALID_NEXT;
}

/**
   @brief given a timer object pointer, it returns the handler
            NOT THREAD SAFE
            PRIVATE METHOD
   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC TTIM_HND_T _ttim_get_hnd( void *ptr )
{
#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    return TTIM_GET_HND( ptr );
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    TTIM_ASSERT( ptr != NULL );
    return ( ptr );
#endif
}

/**
   @brief returns if the time value is valid
            NOT THREAD SAFE
            PRIVATE METHOD
   @param time
   @return true
   @return false
 */
TTIM_STATIC bool _ttim_time_is_valid( TTIM_COUNT_T time )
{
    return time != TTIM_INVALID_TIME;
}

#if 0 // NOT USED FOR NOW
/**
   @brief returns if the list is empty
            NOT THREAD SAFE
            PRIVATE METHOD
   @param list
   @return true
   @return false
 */
TTIM_STATIC bool _ttim_list_is_empty( ttim_list_t *list )
{
    if ( _ttim_node_is_valid( list->next ) )
    {
        return false;
    }
    else
    {
        return true;
    }
}
#endif

/**
   @brief returns the tick count from the last call to _ttim_timebase_start
            NOT THREAD SAFE
            PRIVATE METHOD
   @return TTIM_COUNT_T
 */
TTIM_STATIC TTIM_COUNT_T _ttim_timebase_elapsed()
{
    /* this is a low level function that relies on the user configuiration */
    TTIM_COUNT_T rv;

#if TTIM_PERIODIC_TICK == 1
    /* for periodic ticks there is no remaining offline elapsed time, because there is one tick per count.
       Optimizing the library will simplify this call */
    rv = 0;
#else

#ifdef TTIM_TIMEBASE_TYPE
    rv = TTIM_TIMEBASE_ELAPSED( &time_base_obj );
#else
    rv = TTIM_TIMEBASE_ELAPSED();
#endif

#endif
    return rv;
}

/**
   @brief Removes the first timer in the list.
            NOT THREAD SAFE
            PRIVATE METHOD
   @param list
   @return TTIM_STATIC
 */
TTIM_STATIC void _ttim_list_remove_first( ttim_list_t *list )
{
    ttim_node_t *node = list->entry.next;

    list->entry.next = node->next;
    list->entry.t = node->t;

    // list->element_count--;

    _ttim_node_invalidate( node );
}

/**
   @brief   Removes a timer from the list.
            NOT THREAD SAFE
            PRIVATE METHOD
   @param   hnd
   @return  TTIM_COUNT_T Remaining time that had left the removed node.
 */
TTIM_STATIC TTIM_COUNT_T _ttim_list_remove( ttim_list_t *list, TTIM_HND_T hnd )
{
    TTIM_COUNT_T rv;
    ttim_node_t *node;
    ttim_node_t *node_bck;

    node = list->entry.next;

    /* upcast */
    node_bck = ( ttim_node_t * )list;

    rv = list->entry.t;

#if TTIM_CALC_STATS == 1
    ttim_remove_count++;
#endif

    while ( 1 )
    {
#if TTIM_CALC_STATS == 1
        ttim_loops_on_remove_count++;
#endif
        bool not_found = ( TTIM_INVALID_NEXT == node );
        bool found;

        if ( true == not_found )
        {
            /* not found */
            rv = TTIM_INVALID_TIME;
            break;
        }

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
        found = ( ( ttim_node_t * )&ttim_group[hnd] == node );
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
        found = ( hnd == node );
#endif

        if ( true == found )
        {
            /* found */
            node_bck->next = node->next;
            node_bck->t += node->t;

            _ttim_node_invalidate( node );

            // list->element_count--;

            break;
        }

        rv += node->t;
        node_bck = node;
        node = node->next;
    }

    return rv;
}

#if 0 // NOT BEING USED FOR NOW
/**
   @brief   Removes a timer from the list.
            NOT THREAD SAFE
            PRIVATE METHOD
   @param   hnd
   @return  TTIM_COUNT_T Remaining time that had left the removed node.
 */
TTIM_STATIC TTIM_COUNT_T _ttim_list_insert( ttim_list_t *list, TTIM_HND_T hnd )
{
    TTIM_COUNT_T rv;
    ttim_node_t *node;
    ttim_node_t *node_bck;

    node = list->next;

    /* upcast */
    node_bck = ( ttim_node_t * )list;

    rv = list->t;

#if TTIM_CALC_STATS == 1
    ttim_remove_count++;
#endif

    while ( 1 )
    {
#if TTIM_CALC_STATS == 1
        ttim_loops_on_remove_count++;
#endif
        bool not_found = ( TTIM_INVALID_NEXT == node );
        bool found;

        if ( true == not_found )
        {
            /* not found */
            rv = TTIM_INVALID_TIME;
            break;
        }

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
        found = ( ( ttim_node_t * ) &ttim_group[hnd] == node );
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
        found = ( hnd == node );
#endif

        if ( true == found )
        {
            /* found */
            node_bck->next = node->next;
            node_bck->t += node->t;

            _ttim_node_invalidate( node );

            break;
        }

        rv += node->t;
        node_bck = node;
        node = node->next;
    }

    return rv;
}
#endif

/**
   @brief   returns if the timer has paused
            NOT THREAD SAFE
   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC bool _ttim_is_paused( TTIM_HND_T hnd )
{
    ttim_t *tim;
    bool rv = false;

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( hnd != NULL );
#endif

    rv = ( tim->remining_time != TTIM_INVALID_TIME );
    // rv = rv && (tim->count != TTIM_INVALID_TIME);    //this condition is irrelevant in the states truth table
    rv = rv && ( tim->paused == 1 );

    return rv;
}

/**
   @brief   returns if the timer is stopped
            NOT THREAD SAFE
   @param hnd
   @return TTIM_COUNT_T
 */
TTIM_STATIC bool _ttim_is_stopped( TTIM_HND_T hnd )
{
    ttim_t *tim;
    bool rv = false;

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( hnd != NULL );
#endif

    rv = ( tim->remining_time == TTIM_INVALID_TIME );
    rv = rv && ( tim->count != TTIM_INVALID_TIME );
    rv = rv && ( tim->paused == 0 );

    return rv;
}

/**
   @brief Returns if the tier has timedout.
          NOT THREAD SAFE
   @param hnd
   @return bool
 */
TTIM_STATIC bool _ttim_is_timedout( TTIM_HND_T hnd )
{
    bool rv = false;
    ttim_t *tim;
#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim != NULL );
#endif

    rv = ( tim->remining_time == TTIM_INVALID_TIME );
    rv = rv && ( tim->count != TTIM_INVALID_TIME );
    rv = rv && ( tim->paused == 1 );

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
    ttim_t *tim;

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    tim = &ttim_group[hnd];
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim != NULL );
#endif

    rv = ( tim->remining_time != TTIM_INVALID_TIME );
    // rv = rv && ( tim->count != TTIM_INVALID_TIME );      //this condition is irrelevant in the states truth table
    rv = rv && ( tim->paused == 0 );
    return rv;
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
    ttim_node_t *node = ttim_list.entry.next;
    ttim_node_t *tim;

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    tim = ( ttim_node_t * )&ttim_group[hnd];
#elif TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    tim = hnd;
    TTIM_ASSERT( tim != NULL );
#endif

    rv = ttim_list.entry.t;

    if ( rv != TTIM_INVALID_TIME )
    {
#if TTIM_CALC_STATS == 1
        ttim_remining_count++;
#endif
        while ( 1 )
        {
#if TTIM_CALC_STATS == 1
            ttim_loops_on_remining_count++;
#endif

            if ( TTIM_INVALID_NEXT == node )
            {
                /* not found */
                rv = TTIM_INVALID_TIME;
                break;
            }

            if ( tim == node )
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
            PRIVATE FUNCTION
            NOT THREAD SAFE

   @return bool true:   there is at least one timer running
                false:  there aren't any timer running
 */
TTIM_STATIC bool _ttim_is_any_running()
{
    int rv;

    // TTIM_CRITICAL_START();

    if ( ttim_list.entry.next == TTIM_INVALID_NEXT )
    {
        rv = false;
    }
    else
    {
        rv = true;
    }

    // TTIM_CRITICAL_END();

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

#if TTIM_PERIODIC_TICK == 1
    /* for this mode, the value of the tick is fixed */
    time = TTIM_RESOLUTION;
#endif

    if ( time == 0 )
    {
        /* no need to start the time base */
        return;
    }

#ifdef TTIM_TIMEBASE_TYPE
    TTIM_TIMEBASE_START( &time_base_obj, time );
#else
    TTIM_TIMEBASE_START( time );
#endif

#if defined(TTIM_TIMEBASE_TYPE) && defined(TTIM_TIMEBASE_IS_RUNNING)
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) );
#endif
}

/**
   @brief Stops the timebase
   @return
 */
TTIM_STATIC void _ttim_timebase_stop()
{
    /* turn off he timebase */
#ifdef TTIM_TIMEBASE_TYPE
    TTIM_TIMEBASE_STOP( &time_base_obj );
#else
    TTIM_TIMEBASE_STOP();
#endif
}

/**
   @brief Stops the timebase if there is no more timers running.
          If there are  running timers, it starts the timebase again

   @return true    there still are timers that need for the time base, the timebase didn't stop
   @return false   the timebase has stopped
 */
TTIM_STATIC bool _ttim_timebase_stop_or_restart( bool any_timer_running )
{
    if ( !any_timer_running )
    {
        _ttim_timebase_stop();
    }
    else
    {
        /* the list still have timers to run, the timebase should be reconfigured */
        _ttim_timebase_start( ttim_list.entry.t );
    }

    return any_timer_running;
}

ttim_node_t *_ttim_find_by_time( ttim_node_t *list_node, TTIM_COUNT_T count_to, TTIM_COUNT_T *acum_ )
{
    // Insert
    while ( 1 )
    {
        if ( list_node->next == TTIM_INVALID_NEXT )
        {
            /* reach the last node in the list, insert at the end  */
            break;
        }

        *acum_ += list_node->t;

        if ( *acum_ > count_to )
        {
            /* insert the new timer between node and node->next */
            break;
        }

        /* jump to the next */
        list_node = list_node->next;
    }

    return list_node;
}


/**
   @brief Inserts a timer with an final count, into the provided list.

   @param list
   @param timer
   @param total
 */
void _ttim_list_ttim_insert( ttim_list_t *list, ttim_t *timer, TTIM_COUNT_T total )
{
    ttim_node_t *node = &list->entry;
    // TTIM_COUNT_T total = timer->count;

    uint32_t acum_elapsed = 0 - _ttim_timebase_elapsed();

    // printf("total = %d elap %d\n", total, acum_elapsed);

    TTIM_ASSERT( total != TTIM_INVALID_TIME );

    node = _ttim_find_by_time( node, total, &acum_elapsed );

    if ( node->next == TTIM_INVALID_NEXT )
    {
        /* Reach the last node in the list, insert at the end.
           This case also handles when the list is empty  */
        timer->node.t = TTIM_INVALID_TIME;
        node->t = total - acum_elapsed;
    }
    else
    {
        /* insert in the middle, or at the beggining */
        timer->node.t = acum_elapsed - total;

        if ( node == &ttim_list )
        {
            node->t = total; // there were nodes but the new one will be de next
        }
        else
        {
            node->t -= timer->node.t;
        }
    }

    timer->node.next = node->next; // if invalid is ok, if not, it is ok also
    timer->remining_time = total;
    timer->paused = 0;

    node->next = ( ttim_node_t * )timer;
}

/**
   @brief Stop, Reset and Start a timer keeping its prvious caracteristics (count, callback, param)
          EQUIV STOP RESTART
          The hnd is not validated because is an internal method
          NOT THREAD SAFE
          PRIVATE
   @param hnd
 */
void _ttim_reinsert( TTIM_HND_T hnd )
{
    ttim_t *timer;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    timer = &ttim_group[hnd];
#endif

    /* remove the timer from te timer list */
    _ttim_list_remove( &ttim_list, hnd ); // if hnd is the first node, the removal is O(1)  _ttim_list_remove_first

    // Set ************************

#if TTIM_CALC_STATS == 1
    ttim_reinsert_count++;
#endif

    _ttim_list_ttim_insert( &ttim_list, timer, timer->count );

    /* Starts the time base, conditionally.
       This action is a system process that should not be within a critical section. */
    _ttim_timebase_start( ttim_list.entry.t );
}

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
/**
   @brief individual timer destructor

   @param hnd   Timer handler
 */
TTIM_HND_T ttim_dtor( TTIM_HND_T hnd )
{
#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    TTIM_ASSERT( hnd != NULL );
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
    ttim_t *timer_new;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    hnd = TTIM_MALLOC( sizeof( ttim_t ) );
    if ( hnd != NULL )
    {
        timer_new = hnd;

#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer_new = &ttim_group[hnd];
#endif
        TTIM_CRITICAL_START();

        _ttim_node_invalidate( ( ttim_node_t * )timer_new );

        timer_new->count = TTIM_INVALID_TIME;
        timer_new->remining_time = TTIM_INVALID_TIME;
        timer_new->paused = 0;

#if TTIM_CB_MODE != TTIM_CB_MODE_NONE
        timer_new->timeout_callback = NULL;
#endif

#if TTIM_CB_MODE == TTIM_CB_MODE_PARAM
        timer_new->timeout_param = NULL;
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

#if TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    for ( hnd = 0; hnd < TTIM_COUNT; hnd++ )
    {
        ttim_ctor( hnd );
    }
#endif

    TTIM_CRITICAL_START();

    _ttim_node_invalidate( ( ttim_node_t * )&ttim_list );

    // ttim_list.element_count = 0;

    TTIM_CRITICAL_END();

#ifdef TTIM_TIMEBASE_TYPE
    TTIM_TIMEBASE_INIT( &time_base_obj );
#else
    TTIM_TIMEBASE_INIT();
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
bool ttim_is_timedout( TTIM_HND_T hnd )
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
#if TTIM_CB_MODE == TTIM_CB_MODE_SIMPLE
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time, void *cb )
#elif TTIM_CB_MODE == TTIM_CB_MODE_PARAM
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time, void *cb, void *param )
#else
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time )
#endif
{
    ttim_stop( hnd );

#if TTIM_CB_MODE == TTIM_CB_MODE_SIMPLE
    ttim_set( hnd, time, cb );
#elif TTIM_CB_MODE == TTIM_CB_MODE_PARAM
    ttim_set( hnd, time, cb, param );
#else
    ttim_set( hnd, time );
#endif

    ttim_start( hnd );
}

#if TTIM_ALLOW_PERIODIC_TIMERS == 1

void ttim_set_periodic( TTIM_HND_T hnd )
{
    ttim_t *timer;
    //  periodic = 1;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();
        timer->periodic = 1;
        TTIM_CRITICAL_END();
    }
}
#endif

/**
   @brief   Sets the timeout value. The timer do not start until ttim_start is called.
            If it was running, the timer starts with the new timeout value (from zero)
 */
#if TTIM_CB_MODE == TTIM_CB_MODE_SIMPLE
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void *cb )
#elif TTIM_CB_MODE == TTIM_CB_MODE_PARAM
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void *cb, void *param )
#else
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time )
#endif
{
    ttim_t *timer;

    if ( time == TTIM_INVALID_TIME )
    {
        return;
    }

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();

        timer->count = time;

#if TTIM_CB_MODE == TTIM_CB_MODE_SIMPLE
        timer->timeout_callback = cb;
#elif TTIM_CB_MODE == TTIM_CB_MODE_PARAM
        timer->timeout_callback = cb;
        timer->timeout_param = param;
#else
#endif

        if ( _ttim_is_running( hnd ) )
        {
            /* if it's running, reinsert the timer in the list with the new params, and start running */
            _ttim_reinsert( hnd );
        }

        TTIM_CRITICAL_END();
    }
}

/**
   @brief   Starts the timer. If the timer was paused, it resumes the last count.
            If it is running, do nothing

   @param hnd
 */
void ttim_start( TTIM_HND_T hnd )
{
    ttim_t *timer;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();

        if ( _ttim_is_running( hnd ) )
        {
            /* already running : exit */
            TTIM_CRITICAL_END();
            return;
        }

        /* the timer was not setted, exit */
        if ( timer->count == TTIM_INVALID_TIME )
        {
            TTIM_CRITICAL_END();
            return;
        }

        /* up cast the list to ttim_t structure */
        ttim_node_t *tim = &ttim_list.entry;

        TTIM_COUNT_T count_to;
        // uint32_t acum_elapsed = 0;

        /* if the timer was paused or not */
        if ( !_ttim_is_paused( hnd ) )
        {
            /* it was not paused */
            count_to = timer->count;
        }
        else
        {
            /* it was paused */
            count_to = timer->remining_time;
        }

        TTIM_COUNT_T acum = 0;

#if TTIM_CALC_STATS == 1
        ttim_start_count++;
#endif

        _ttim_list_ttim_insert( &ttim_list, timer, count_to );

        TTIM_CRITICAL_END();

        /* Starts the time base, conditionally.
           This action is a system process that should not be within a critical section. */
        _ttim_timebase_start( ttim_list.entry.t );
    }
}

/**
   @brief Resets the count and stops.
   @param hnd
 */
void ttim_stop( TTIM_HND_T hnd )
{
    ttim_t *timer;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_CRITICAL_START();

        /* remove the timer from te timer list */
        _ttim_list_remove( &ttim_list, hnd );

        timer->remining_time = TTIM_INVALID_TIME;
        timer->paused = 0;

        bool running = _ttim_is_any_running();

        TTIM_CRITICAL_END();

        /* Any system process should be done outside a critical section.  */
        _ttim_timebase_stop_or_restart( running );
    }
}

/**
   @brief Pauses the t  imer keeping the internal count intact.
   @param hnd
 */
void ttim_pause( TTIM_HND_T hnd )
{
    ttim_t *timer;

#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
        timer = hnd;
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        timer = &ttim_group[hnd];
#endif

        TTIM_COUNT_T res;

        TTIM_CRITICAL_START();

        /* removes the node */
        res = _ttim_list_remove( &ttim_list, hnd );

        if ( res != TTIM_INVALID_TIME )
        {
            /* the node existed but was removed, so, update the t information */
            timer->remining_time = res - _ttim_timebase_elapsed();
        }

        timer->paused = 1;

        bool running = _ttim_is_any_running();

        TTIM_CRITICAL_END();

        /* Any system process should be done outside a critical section. */
        _ttim_timebase_stop_or_restart( running );
    }
}

/**
   @brief reset the internal count and start de timer again
   @param hnd
 */
void ttim_reset_n_restart( TTIM_HND_T hnd )
{
#if TTIM_MM_MODE == TTIM_MM_MODE_DYNAMIC
    if ( hnd != TTIM_INVALID_HND )
    {
#elif TTIM_MM_MODE == TTIM_MM_MODE_STATIC
    if ( hnd < TTIM_COUNT )
    {
        hnd = hnd;
#endif
        TTIM_CRITICAL_START();

        _ttim_reinsert( hnd );

        TTIM_CRITICAL_END();
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

    if ( TTIM_INVALID_TIME != rv )
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
#if TTIM_PERIODIC_TICK == 1
#else
    /* the time base is stopped */
    _ttim_timebase_stop();
#endif

    TTIM_CRITICAL_START();

#if (TTIM_PERIODIC_TICK == 1)
    /* The tick is always running, so maybe it is ipmortant to handle som race condition en terms  of hardware flags  */
    if ( TTIM_INVALID_NEXT == ttim_list.entry.next )
    {
        TTIM_CRITICAL_END();
        return;
    }
#else
    /* In this mode the running list should be at least with one element. */
    TTIM_ASSERT( TTIM_INVALID_NEXT != ttim_list.entry.next );
#endif

#if TTIM_PERIODIC_TICK == 1
    ttim_list.entry.t -= 1; /* decrease 1 time unit   */
#else
    ttim_list.entry.t = 0;

    /* the time base is stopped */
    // _ttim_timebase_stop();
#endif

    bool any_timer_running = true;

    /* up cast the next to a node structure */
    TTIM_HND_T hnd;
    ttim_t *tim;

#if TTIM_CALC_STATS == 1
    ttim_update_count++;
#endif

#if TTIM_PERIODIC_TICK == 1
    if ( ttim_list.entry.t > 0 )
    {
        /* no timer has to be processed */
        TTIM_CRITICAL_END();
        fflush( stdout );
    }
    else
#endif
    {
        /* remove all the timers that had timeout and execute their callbacks */
        do
        {
#if TTIM_CALC_STATS == 1
            ttim_loops_on_update_count++;
#endif
            /* upcast the first node on the list */
            tim = ( ttim_t * )ttim_list.entry.next;

            /* Get the handler for that node */
            hnd = _ttim_get_hnd( tim );

            /* Get the callback */
            callback_t *which_callback = ( callback_t * )tim->timeout_callback;
            void *which_param = tim->timeout_param;

#if TTIM_ALLOW_PERIODIC_TIMERS == 1
            /* if the timer is periodic, then restart it */
            if ( tim->periodic )
            {
                /* Restart the timer */
                _ttim_reinsert( hnd );
            }
            else
#endif
            {
                /* Removes the first node on de list */
                _ttim_list_remove_first( &ttim_list );

                /* Set the removed node to timedout state  */
                tim->paused = 1;
                tim->remining_time = TTIM_INVALID_TIME;
            }

            any_timer_running = _ttim_is_any_running();

            TTIM_CRITICAL_END();

            /* Execute the callback */
            if ( NULL != which_callback )
            {
#if TTIM_CB_MODE == TTIM_CB_MODE_SIMPLE
                which_callback( hnd );
#endif

#if TTIM_CB_MODE == TTIM_CB_MODE_PARAM
                which_callback( hnd, which_param );
#endif
            }

            if ( 0 == ttim_list.entry.t )
            {
                TTIM_CRITICAL_START();
            }
        }
        while ( ttim_list.entry.t == 0 );
    }

#if defined(TTIM_TIMEBASE_IS_RUNNING) && (TTIM_PERIODIC_TICK == 1)
#ifdef TTIM_TIMEBASE_TYPE
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj ) );
#else
    TTIM_ASSERT( TTIM_TIMEBASE_IS_RUNNING() );
#endif
#endif

    //    TTIM_CRITICAL_END(); //not needed, end of the section is done before calling the callback

    /* Any system process should be done outside a critical section.  */

    /* stops the timebase */

    if ( !any_timer_running )
    {
    }
    else
    {
        _ttim_timebase_start( ttim_list.entry.t );
    }
}

#if TTIM_CALC_STATS == 1
void ttim_print_stats()
{
    printf( "ttim remove: %2.1f\n", ( float )ttim_loops_on_remove_count / ( float )ttim_remove_count );
    printf( "ttim reming: %2.1f\n", ( float )ttim_loops_on_remining_count / ( float )ttim_remining_count );
    printf( "ttim start: %2.1f\n", ( float )ttim_loops_on_start_count / ( float )ttim_start_count );
    printf( "ttim reinsert: %2.1f\n", ( float )ttim_loops_on_reinsert_count / ( float )ttim_reinsert_count );
    printf( "ttim update: %2.1f\n", ( float )ttim_loops_on_update_count / ( float )ttim_update_count );
}
#endif



/* v1.05 */
