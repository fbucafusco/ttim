#ifndef TTIM_H
#define TTIM_H

#include <stdint.h>
#include <stdbool.h>

#include "ttim_config.h"

/* TODO : UNIFY INSERT*/
/* TODO: IMPROVE REINSERT*/
/* TODO: the 31 bit is number is nor portable. It depends on TTIM_COUNT_T */

/* CONSTANTS =========================================================================== */
#define TTIM_MM_MODE_STATIC                         1
#define TTIM_MM_MODE_DYNAMIC                        2

#define TTIM_CB_MODE_NONE                           1
#define TTIM_CB_MODE_SIMPLE                         2
#define TTIM_CB_MODE_PARAM                          3

/* DEFAULT VALUES ======================================================================= */
#ifndef TTIM_ASSERT
/**
   @brief assertion macro for teting and debugging purposes
 */
#define TTIM_ASSERT(A)
#endif

#ifndef TTIM_PERIODIC_TICK
#define TTIM_PERIODIC_TICK  0
#endif

#ifndef TTIM_ALLOW_PERIODIC_TIMERS
/* Defines if periodic timers are allowed
    0: all the created timers are one shot
    1: the user can choose to make a timer periodic
 */
#define TTIM_ALLOW_PERIODIC_TIMERS  1
#endif


#ifndef TTIM_MM_MODE
/* defines the memory management schemes use to create timers.
   TTIM_MM_MODE_STATIC : In this mode the number of timers is fixed to TTIM_COUNT, and all the objects
   (DEFAULT)             are allocated at compile time.
                         All the timers are initialized with ttim_init.
                         The handlers for each timer are integer numbers starting from 0 to TTIM_COUNT-1
   TTIM_MM_MODE_DYNAMIC (TBI):
                         In this mode dynamic memory is used for allocating timers at runtime.
                         ttim_init should be called for initialize the module, but there are no timers
                         allocated at that time.
                         User should use ttim_ctor for creating a new timer. The funcion
                         returns the handler.
    */
#define TTIM_MM_MODE    TTIM_MM_MODE_STATIC
#endif

#ifndef TTIM_CB_MODE
/*  defines the interface for implementing the callbacks when any event occurs
    TTIM_CB_MODE_NONE   : No callbacks are used. The user should poll the timer status.
    TTIM_CB_MODE_SIMPLE : The callback is called by passing just the timer id
    TTIM_CB_MODE_PARAM  : The callback is called by passing the timer id that generated the call, and
                          a user parameter.

    NOTE: the callbacks are called wherever context the user choose to execute ttim_update.
          Also the callbacks are called outside any critical secion and can call any ttim_xxx method but the ttmi_update one.
*/
#define TTIM_CB_MODE    TTIM_CB_MODE_PARAM
#endif

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
#ifndef TTIM_HND_T
/*
   Defines the datatype for each handle. In this mm mode, it is represented by an index.
   The range of the data type should be less than TTIM_COUNT.
*/
#define TTIM_HND_T              uint16_t
#endif
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
#define TTIM_HND_T              void*
#endif

#ifndef TTIM_COUNT_T
/**
   @brief Defines the range count fot the timers
 */
#define TTIM_COUNT_T        uint32_t
#endif

/* CONDITIONALS ===================================================================== */
#define TTIM_INVALID_TIME    ((TTIM_COUNT_T)(~((TTIM_COUNT_T)0 ))>>1) //the >>1 is because the count and remaining time is 31 wide. 


#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
#define TTIM_INVALID_HND     TTIM_COUNT
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
#define TTIM_INVALID_HND     NULL
#endif

/* DATA TYPES ======================================================================== */
#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
typedef void callback_t( TTIM_HND_T hnd );
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
typedef void callback_t( TTIM_HND_T hnd, void*param );
#endif

typedef struct ttim_node_t_
{
    struct ttim_node_t_*    next;
    TTIM_COUNT_T            t;      /* time diference with the next one */
} ttim_node_t;

typedef struct ttim_t_
{
    /* node that points to next active timer and has the time difference with it.*/
    ttim_node_t             node;

    TTIM_COUNT_T            remining_time:31;

#if TTIM_ALLOW_PERIODIC_TIMERS == 1
    TTIM_COUNT_T            periodic:1;
#endif

    TTIM_COUNT_T            count:31;
    TTIM_COUNT_T            paused:1;

#if TTIM_CB_MODE!=TTIM_CB_MODE_NONE
    void*                   timeout_callback;
#endif

#if TTIM_CB_MODE==TTIM_CB_MODE_PARAM
    void*                   timeout_param;
#endif

} ttim_t;

/* node that points to next active timer and has the time difference with it.*/
typedef ttim_node_t ttim_list_t;

/* FUNCTION PROTOTYPES *************************************************************** */
void ttim_init( void );
void ttim_start( TTIM_HND_T hnd );

#if TTIM_ALLOW_PERIODIC_TIMERS == 1
void ttim_set_periodic( TTIM_HND_T hnd );
#endif

#if TTIM_CB_MODE==TTIM_CB_MODE_SIMPLE
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb );
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb );
#elif TTIM_CB_MODE==TTIM_CB_MODE_PARAM
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb,void*param );
void ( TTIM_HND_T hnd, TTIM_COUNT_T time, void* cb,void*param );
#else
void ttim_set( TTIM_HND_T hnd, TTIM_COUNT_T time );
void ttim_set_n_start( TTIM_HND_T hnd, TTIM_COUNT_T time );
#endif
void ttim_reset_n_restart( TTIM_HND_T hnd );
void ttim_stop( TTIM_HND_T hnd );
void ttim_pause( TTIM_HND_T hnd );
void ttim_update( void );

TTIM_COUNT_T ttim_get_remining_time( TTIM_HND_T hnd );
bool ttim_is_stopped( TTIM_HND_T hnd );
bool ttim_is_timedout( TTIM_HND_T hnd );

#if TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
TTIM_HND_T ttim_ctor( TTIM_HND_T hnd );
TTIM_HND_T ttim_dtor( TTIM_HND_T hnd );
#endif

#if TTIM_CALC_STATS == 1
void ttim_print_stats();
#endif

#endif // TTIM_H

/* v1.05 */
