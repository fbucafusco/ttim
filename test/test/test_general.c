#include <string.h>
#include "unity.h"
#include "ttim.h"
#include "ttim_abstraction.h"

/* handlers for testing static allocation */
enum
{
    TTIM_00,
    TTIM_01,
    TTIM_02,
    TTIM_03,
};

extern TTIM_TIMEBASE_TYPE   time_base_obj;
extern ttim_list_t  ttim_list;
extern const ttim_t ttim_group[];
char activations[4];
uint16_t print_idx;
int isr_dis;
TTIM_HND_T hnd_group[4];

TTIM_COUNT_T _ttim_remaining_time( TTIM_HND_T hnd  )  ;
bool _ttim_is_any_running();
bool _ttim_is_paused( TTIM_HND_T hnd );
bool _ttim_is_stopped( TTIM_HND_T hnd );
bool _ttim_node_is_valid( ttim_t* node );
bool _ttim_time_is_valid( TTIM_COUNT_T time );
TTIM_HND_T _ttim_get_hnd( void* ptr );

#if DEBUG_DURING_TESTING==1
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


void setUp()
{
    /* no timer has triggered */
    memset( activations, 0, sizeof( activations ) );

    ttim_init();

#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    /* TIMER hnd_group ARE INDEXES */
    hnd_group[0] = TTIM_00 ;
    hnd_group[1] = TTIM_01 ;
    hnd_group[2] = TTIM_02 ;
    hnd_group[3] = TTIM_03 ;
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    /* TIMER hnd_group ARE POINTERS, AND MUST BE CREATED IN RUNTIME */
    hnd_group[0] = ttim_ctor( 0 ) ;
    hnd_group[1] = ttim_ctor( 0 ) ;
    hnd_group[2] = ttim_ctor( 0 ) ;
    hnd_group[3] = ttim_ctor( 0 ) ;
#endif

    isr_dis = 0;
    print_idx=0;

    PRINTF( "\n" );
}

void tearDown()
{
#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    /* TIMER hnd_group ARE INDEXES */
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    /* TIMER hnd_group ARE POINTERS, AND MUST BE CREATED IN RUNTIME */
    ttim_dtor( hnd_group[0] ) ;
    ttim_dtor( hnd_group[1] ) ;
    ttim_dtor( hnd_group[2] ) ;
    ttim_dtor( hnd_group[3] ) ;
#endif
}

/**
@brief Simulates n ticks

@param n
*/
void simulate_ticks( TTIM_COUNT_T n )
{
    int i;

#if TTIM_PERIODIC_TICK==1
    for( i=0; i<n; i++ )
    {
        TEST_ASSERT_EQUAL( 0, isr_dis );
        ttim_update();
        TTIM_TIMEBASE_ADD_ELAPSED( &time_base_obj, 1 );
    }
#else
    for( TTIM_COUNT_T i=0; i<n; i++ )
    {
        timebase_add_elapsed( &time_base_obj, 1 );

        int32_t dif = timebase_timedout( &time_base_obj );

        if( dif==-1 )
        {
            //no timed out
        }
        else
        {
            //timed out
            ttim_update();
        }
    }
#endif
}

#if TTIM_CB_MODE==TTIM_CB_MODE_PARAM
void tim_callback( TTIM_HND_T hnd, void*param )
{
#if TTIM_MM_MODE==TTIM_MM_MODE_STATIC
    /* TIMER hnd_group ARE INDEXES */
    activations[hnd]++;
#elif TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
    /* TIMER hnd_group ARE POINTERS, AND MUST BE CREATED IN RUNTIME */

    for( int i =0; i<4; i++ )
        if( hnd==hnd_group[i] )
        {
            activations[i]++;
            return;
        }
#endif

}
#endif

void print_tim_list()
{
    ttim_t* node  = ( ttim_t* ) &ttim_list;
    TTIM_COUNT_T temp = node->next_delay;

    PRINTF( "[%02u] el: %02u tb: %02u {", print_idx,   timebase_get_elapsed( &time_base_obj ), node->next_delay );
    print_idx++;

    while( 1 )
    {
        node    = node->next;

        if( !_ttim_node_is_valid( node ) )
        {
            break;
        }

        TTIM_HND_T hnd  = _ttim_get_hnd( node );

        PRINTF( "%u (to: %2u d: %2d) ", hnd, temp, ( _ttim_time_is_valid( node->next_delay )?node->next_delay:-1 )   );


        temp   += node->next_delay ;

        if( _ttim_node_is_valid( node->next ) )
        {
            PRINTF( " -> " );
        }

    }

    PRINTF( "}\n" );
    fflush( stdout );
}

void test_TTIM_0a()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    TEST_ASSERT_FALSE( _ttim_time_is_valid ( ttim_get_remining_time( hnd_group[0] ) )     );

    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 17, NULL, NULL );
    ttim_set( hnd_group[2], 15, NULL, NULL );
    ttim_set( hnd_group[3], 33, NULL, NULL );

    TEST_ASSERT_FALSE( _ttim_time_is_valid ( ttim_get_remining_time( hnd_group[0] ) )     );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[3] );

    print_tim_list();   //0

    simulate_ticks( 6 );

    print_tim_list();   //1

    ttim_start( hnd_group[2] );

    print_tim_list();   //2

#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 10, _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 17, _ttim_remaining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 21, _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 33, _ttim_remaining_time( hnd_group[3] ) );
#endif

    /* hnd_group[2] restart with the same number of ticks the internal timer values should be the same as before  */

    ttim_set( hnd_group[2], 15, NULL, NULL );

    print_tim_list();   //3 - this printout should be equal than the nro 1

#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 10,  _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 17,  _ttim_remaining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 21,  _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 33,  _ttim_remaining_time( hnd_group[3] ) );
#endif

    TEST_ASSERT_EQUAL( 4,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 11,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 15,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 27,  ttim_get_remining_time( hnd_group[3] ) );
}

void test_TTIM_0()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    ttim_set(  hnd_group[0], 10, NULL, NULL );
    ttim_set(  hnd_group[1], 20, NULL, NULL );
    ttim_set(  hnd_group[2], 30, NULL, NULL );
    ttim_set(  hnd_group[3], 3, NULL, NULL );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    print_tim_list();   //0

    simulate_ticks( 5 );

    ttim_start( hnd_group[3] );

    print_tim_list();   //1

#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 5, _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 15, _ttim_remaining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 25, _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 3, _ttim_remaining_time( hnd_group[3] ) );
#endif

    /* hnd_group[1] restarts with a different time. The running list updates all its remining_to time values */
    ttim_set( hnd_group[1], 2, NULL, NULL );

    print_tim_list();   //2

#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 5, _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 2, _ttim_remaining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 25, _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 3, _ttim_remaining_time( hnd_group[3] ) );
#endif

    /* Add 1 tick, the running list keep intact all its remining_to time values */
    simulate_ticks( 1 );

    print_tim_list();   //3

#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 5,  _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 2,  _ttim_remaining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 25, _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 3,  _ttim_remaining_time( hnd_group[3] ) );
#endif

    TEST_ASSERT_EQUAL( 4,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 1,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 24, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 2,  ttim_get_remining_time( hnd_group[3] ) );

    /* Add 1 tick, hnd_group[1] will end and removed from the list.
       The running list updates all its remining_to time values */
    simulate_ticks( 1 );
#if TTIM_PERIODIC_TICK==0
    TEST_ASSERT_EQUAL( 3, _ttim_remaining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid ( _ttim_remaining_time( hnd_group[1] ) )     );
    TEST_ASSERT_EQUAL( 23, _ttim_remaining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 1, _ttim_remaining_time( hnd_group[3] ) );

    TEST_ASSERT_EQUAL( 3,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid ( _ttim_remaining_time( hnd_group[1] ) )     );
    TEST_ASSERT_EQUAL( 23, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 1,  ttim_get_remining_time( hnd_group[3] ) );
#endif

    TEST_ASSERT_EQUAL( 3,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 23, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 1,  ttim_get_remining_time( hnd_group[3] ) );

    print_tim_list(); //4
}


void test_TTIM_1()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) ); /* timebase is turned off */

    ttim_set( hnd_group[0], 10, tim_callback, NULL );
    ttim_set( hnd_group[1], 5, tim_callback, NULL );
    ttim_set( hnd_group[2], 40, tim_callback, NULL );
    ttim_set( hnd_group[3], 20, tim_callback, NULL );

    TEST_ASSERT_TRUE( ttim_is_stopped( hnd_group[0] ) );

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) ); /* timebase is turned off */

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_EQUAL( 0, activations[0] );
    TEST_ASSERT_EQUAL( 0, activations[1] );
    TEST_ASSERT_EQUAL( 0, activations[2] );
    TEST_ASSERT_EQUAL( 0, activations[3] );

    TEST_ASSERT_FALSE( _ttim_is_any_running() );

    TEST_ASSERT_FALSE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );

    ttim_start( hnd_group[0] );

    TEST_ASSERT_FALSE( ttim_is_stopped( hnd_group[0] ) );
    TEST_ASSERT_TRUE( ttim_is_timedout( hnd_group[1] ) );

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  ); //timebase is turned on

    ttim_start( hnd_group[1] );

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    ttim_start( hnd_group[2] );

    TEST_ASSERT_EQUAL( 0, isr_dis );;
    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  ); //timebase is turned on
    TEST_ASSERT_TRUE( _ttim_is_any_running() );

    /* hnd_group[1] must timeout*/
    simulate_ticks( 6 );

    TEST_ASSERT_FALSE( ttim_is_stopped( hnd_group[0] )  );
    TEST_ASSERT_TRUE(  ttim_is_timedout( hnd_group[1] ) );

    TEST_ASSERT_EQUAL( 0,  activations[0] );
    TEST_ASSERT_EQUAL( 1,  activations[1] );
    TEST_ASSERT_EQUAL( 0,  activations[2] );
    TEST_ASSERT_EQUAL( 0,  activations[3] );

    /* 6 ticks,  hnd_group[0] must timeout */
    simulate_ticks( 6 );

    TEST_ASSERT_EQUAL( 1, activations[0]  );
    TEST_ASSERT_EQUAL( 1, activations[1]  );
    TEST_ASSERT_EQUAL( 0, activations[2]  );
    TEST_ASSERT_EQUAL( 0, activations[3]  );

    /* 28 ticks,  hnd_group[2] must timeout */
    simulate_ticks( 28 );

    TEST_ASSERT_EQUAL( 1, activations[0]  );
    TEST_ASSERT_EQUAL( 1, activations[1]  );
    TEST_ASSERT_EQUAL( 1, activations[2]  );
    TEST_ASSERT_EQUAL( 0, activations[3]  );

    TEST_ASSERT_FALSE( _ttim_is_any_running() );

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) );

    /* starts   hnd_group[1] */
    ttim_start( hnd_group[1] );

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_TRUE( _ttim_is_any_running() );
}


/**
   @brief  tries _ttim_remaining_time
           low level method
 */
void test_TTIM_2()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );




    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 5, NULL, NULL );
    ttim_set( hnd_group[2], 40, NULL, NULL );
    ttim_set( hnd_group[3], 20, NULL, NULL );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( _ttim_remaining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( _ttim_remaining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( _ttim_remaining_time( hnd_group[2] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( _ttim_remaining_time( hnd_group[3] ) ) );


    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] ) ;
    ttim_start( hnd_group[2] );
    TEST_ASSERT_EQUAL( 0, isr_dis );;

    print_tim_list(); //0

    /* 2 ticks , no timeouts */
    simulate_ticks( 2 );

    print_tim_list(); //1

    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 3, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 38, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* 6 ticks,   hnd_group[1] must timeout */
    simulate_ticks( 6 );

    print_tim_list(); //2

    TEST_ASSERT_EQUAL( 2, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 32, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* change  hnd_group[1] */
    ttim_set( hnd_group[1], 6, NULL, NULL );
    TEST_ASSERT_EQUAL( 0, isr_dis );

    ttim_start( hnd_group[1] );
    TEST_ASSERT_EQUAL( 0, isr_dis );;

    print_tim_list(); //3

    /* 6 ticks, hnd_group[0] and  hnd_group[1] must timeout */
    simulate_ticks( 6 );

    print_tim_list(); //4

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 26,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );
}

/**
   @brief tries ttim_stop

 */
void test_TTIM_4()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );




    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 5, NULL, NULL );
    ttim_set( hnd_group[2], 40, NULL, NULL );
    ttim_set( hnd_group[3], 20, NULL, NULL );

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) );  //timebase is turned off

    /* stops hnd_group[1] */
    ttim_stop(  hnd_group[1] )  ;

    TEST_ASSERT_EQUAL( 0, isr_dis );

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) );  //timebase is turned off


    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    ttim_start( hnd_group[0] );

    print_tim_list();   //0

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

    /* stops hnd_group[0] */
    ttim_stop(  hnd_group[0] )  ;

    print_tim_list();  //1

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    print_tim_list();   //2

    /* stops hnd_group[0] */
    ttim_stop( hnd_group[0] ) ;

    print_tim_list();   //3

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    /* 6 ticks,  hnd_group[1] must timeout */
    simulate_ticks( 6 );

    print_tim_list();   //4

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 34,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    TEST_ASSERT_EQUAL( 0, isr_dis );;

    print_tim_list();   //5

    /* 3 ticks */
    simulate_ticks( 3 );

    print_tim_list();   //6   hasta aca correcto

    TEST_ASSERT_EQUAL( 7,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 2,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 31,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* stops  hnd_group[1] */
    ttim_stop(  hnd_group[1] )  ;

    print_tim_list();   //7

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_EQUAL( 7,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 31,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* stops  hnd_group[0] */
    ttim_stop(  hnd_group[0] )  ;

    print_tim_list();   //8

    TEST_ASSERT_EQUAL( 0, isr_dis );;

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 31, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );


    /* 30 ticks  */
    simulate_ticks( 30 );

    print_tim_list();   //9

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 1,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );


    TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  ); //timebase is turned on
}

/**
   @brief tries ttim_pause -> ttim_start
 */
void test_TTIM_5()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    ttim_set( hnd_group[0], 10, NULL, NULL  );
    ttim_set( hnd_group[1], 5, NULL, NULL  );
    ttim_set( hnd_group[2], 40, NULL, NULL  );
    ttim_set( hnd_group[3], 20, NULL, NULL  );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    print_tim_list();   //0

    /* 6 ticks , hnd_group[1] must timeout */
    simulate_ticks( 6 );

    print_tim_list();   //1

    ttim_pause( hnd_group[0] );

    print_tim_list();   //2

    /* 6 ticks , hnd_group[1] must timeout */
    simulate_ticks( 10 );

    print_tim_list();   //3

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 24, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* resumes timer hnd_group[0] */
    ttim_start( hnd_group[0] );

    print_tim_list();   //4

    TEST_ASSERT_EQUAL( 4, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 24, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );
}


/**
   @brief tries ttim_get_remining_time y a combo of Pause, Stop & Start

 */
void test_TTIM_6()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 5, NULL, NULL );
    ttim_set( hnd_group[2], 40, NULL, NULL );
    ttim_set( hnd_group[3], 20, NULL, NULL );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    print_tim_list();   //0

    /*  2  ticks */
    simulate_ticks( 2 );

    print_tim_list();   //1

    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 3, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 38, ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /*  2  ticks, hnd_group[0] and hnd_group[1] must timeout */
    simulate_ticks( 10 );

    print_tim_list();   //2

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 28,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );


    /* pause and stop and start hnd_group[2]    */
    ttim_pause( hnd_group[2] );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );

    print_tim_list();   //3

    ttim_stop( hnd_group[2] );
    ttim_start( hnd_group[2] );
    ttim_start( hnd_group[3] );

    print_tim_list();   //4

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 40,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_EQUAL( 20,  ttim_get_remining_time( hnd_group[3] ) );

    /*  21 ticks,   hnd_group[3] must timeout */
    simulate_ticks( 21 );

    print_tim_list();   //5

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 19,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

    /* pause and  resumes hnd_group[2]  and start hnd_group[3]  */
    ttim_pause( hnd_group[2] );
    ttim_start( hnd_group[2] );
    ttim_start( hnd_group[3] );

    /*  19 ticks,   hnd_group[2] must timeout */
    simulate_ticks( 19 );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );
    TEST_ASSERT_EQUAL( 1,  ttim_get_remining_time( hnd_group[3] ) );

    /* start hnd_group[2] */
    ttim_start( hnd_group[2] );
    TEST_ASSERT_EQUAL( 40,  ttim_get_remining_time( hnd_group[2] ) );
}





/**
   @brief tries two calls to  ttim_start

 */
void test_TTIM_7()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );
    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 17, NULL, NULL );
    ttim_set( hnd_group[2], 33, NULL, NULL );
    ttim_set( hnd_group[3], 57, NULL, NULL );

    ttim_start( hnd_group[0] );

    print_tim_list();   //0

    ttim_start( hnd_group[0] );

    print_tim_list();   //1

    ttim_start( hnd_group[1] );

    print_tim_list();   //2

    ttim_start( hnd_group[1] );

    print_tim_list();   //3

    ttim_start( hnd_group[2] );

    print_tim_list();   //4

    ttim_start( hnd_group[2] );

    print_tim_list();   //5

    /* tickeo 2, ninguno dio timeout */
    simulate_ticks( 2 );

    TEST_ASSERT_EQUAL( 8,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 15,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 31,  ttim_get_remining_time( hnd_group[2] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[3] ) ) );

}

/**
   @brief tries internal behaviors
 */
void test_TTIM_8()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );




    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[1], 100, NULL, NULL );
    ttim_set( hnd_group[2], 1000, NULL, NULL );

    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    ttim_set( hnd_group[1], 99, NULL, NULL );

    TEST_ASSERT_EQUAL( 10,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 99,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 1000,  ttim_get_remining_time( hnd_group[2] ) );


}

/**
   @brief tries ttim_set_n_start
 */
void test_TTIM_9()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );




    ttim_set( hnd_group[0], 10, NULL, NULL );
    ttim_set( hnd_group[2], 1000, NULL, NULL );


    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );

    ttim_set_n_start( hnd_group[1], 80, NULL, NULL );

    print_tim_list();   //0

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_EQUAL( 80,  ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[2] ) ) );


    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    print_tim_list();   //1

    TEST_ASSERT_EQUAL( 10, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 80, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 1000, ttim_get_remining_time( hnd_group[2] ) );


    simulate_ticks( 2 );

    print_tim_list();   //2

    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 78, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 998, ttim_get_remining_time( hnd_group[2] ) );



    ttim_set_n_start( hnd_group[1], 50, NULL, NULL );

    print_tim_list();   //3

    ttim_set_n_start( hnd_group[2], 500, NULL, NULL );

    print_tim_list();   //4

    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 50, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 500, ttim_get_remining_time( hnd_group[2] ) );

    simulate_ticks( 2 );

    TEST_ASSERT_EQUAL( 6,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 48, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 498,  ttim_get_remining_time( hnd_group[2] ) );


    print_tim_list();   //5

    ttim_set_n_start( hnd_group[0], 10, NULL, NULL );

    simulate_ticks( 2 );

    TEST_ASSERT_EQUAL( 8,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 46, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 496,  ttim_get_remining_time( hnd_group[2] ) );


    ttim_stop( hnd_group[0] );

    print_tim_list();   //6

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_EQUAL( 46, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 496, ttim_get_remining_time( hnd_group[2] ) );

    ttim_set_n_start( hnd_group[0], 10, NULL, NULL );

    simulate_ticks( 2 );

    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 44, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 494, ttim_get_remining_time( hnd_group[2] ) );


    ttim_stop( hnd_group[2] );

    ttim_set_n_start( hnd_group[2], 30, NULL, NULL );

    simulate_ticks( 2 );

    TEST_ASSERT_EQUAL( 6,  ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 42, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 28,  ttim_get_remining_time( hnd_group[2] ) );


    simulate_ticks( 6 );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_EQUAL( 36, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 22, ttim_get_remining_time( hnd_group[2] ) );


    ttim_start( hnd_group[1] );

    simulate_ticks( 2 );

    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[0] ) ) );
    TEST_ASSERT_EQUAL( 34, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 20, ttim_get_remining_time( hnd_group[2] ) );

}


/**
   @brief extra tries

 */
void test_TTIM_10()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );




    ttim_set( hnd_group[0], 10, tim_callback, NULL );
    ttim_set( hnd_group[1], 5, tim_callback, NULL );
    ttim_set( hnd_group[2], 40, tim_callback, NULL );
    ttim_set( hnd_group[3], 20, tim_callback, NULL );

    {
        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) );      //timebase is turned off

        /* starts  */
        ttim_start( hnd_group[0] );
        ttim_start( hnd_group[1] );
        ttim_start( hnd_group[2] );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

        /*  6 ,   hnd_group[1] must timeout */
        simulate_ticks( 6 );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

        /* stops hnd_group[1] */
        ttim_stop( hnd_group[1] );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

        /*  */
        TEST_ASSERT_EQUAL( 1, activations[1] );

        /* 4 ticks ,   hnd_group[0] must timeout   */
        simulate_ticks( 4 );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

        /* starts hnd_group[1] */
        ttim_start( hnd_group[1] );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on

        /*   */
        TEST_ASSERT_EQUAL( 1, activations[0] );
        TEST_ASSERT_EQUAL( 1, activations[1] );

        /*  30 , ticks all must timeout  */
        simulate_ticks( 30 );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_STOPPED( &time_base_obj ) );      //timebase is turned off
    }

    {
        /* turns on 3 */
        ttim_start( hnd_group[0] ); //10
        ttim_start( hnd_group[1] ); //5
        ttim_start( hnd_group[2] ); //40

        simulate_ticks( 6 );

        TEST_ASSERT_TRUE( TTIM_TIMEBASE_IS_RUNNING( &time_base_obj )  );     //timebase is turned on
    }
}


/**
   @brief equal timeouts and usage of ttim_reset_n_restart

 */
void test_TTIM_11()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    ttim_set( hnd_group[0], 10, tim_callback, NULL );
    ttim_set( hnd_group[2], 40, tim_callback, NULL );

    /* starts  */
    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    TEST_ASSERT_EQUAL( 10, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) ) );
    TEST_ASSERT_EQUAL( 40, ttim_get_remining_time( hnd_group[2] ) );

    simulate_ticks( 6 );

    ttim_set( hnd_group[1], 5, tim_callback, NULL );
    ttim_start( hnd_group[1] );


    TEST_ASSERT_EQUAL( 4, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 5, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 34, ttim_get_remining_time( hnd_group[2] ) );

    ttim_set( hnd_group[0], 10, tim_callback, NULL );
    ttim_set( hnd_group[2], 10, tim_callback, NULL );

    simulate_ticks( 2 );


    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 3, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 8, ttim_get_remining_time( hnd_group[2] ) );

    ttim_reset_n_restart( hnd_group[2] );
    ttim_set( hnd_group[0], 200, tim_callback, NULL );

    simulate_ticks( 4 );

    TEST_ASSERT_EQUAL( 196, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_FALSE( _ttim_time_is_valid( ttim_get_remining_time( hnd_group[1] ) )   );
    TEST_ASSERT_EQUAL( 6, ttim_get_remining_time( hnd_group[2] ) );

}



/**
   @brief tries ttim_is_timedout and some state machine transition
 */
void test_TTIM_12()
{
    PRINTF( "%s ------------------------------- \n\n", __FUNCTION__ );

    ttim_set( hnd_group[0], 40, NULL, NULL );
    ttim_set( hnd_group[1], 21, NULL, NULL );
    ttim_set( hnd_group[2], 13, NULL, NULL );

    /* starts  */
    ttim_start( hnd_group[0] );
    ttim_start( hnd_group[1] );
    ttim_start( hnd_group[2] );

    simulate_ticks( 15 );

    TEST_ASSERT_FALSE( ttim_is_timedout( hnd_group[0] ) );
    TEST_ASSERT_FALSE( ttim_is_timedout( hnd_group[1] ) );
    TEST_ASSERT_TRUE(  ttim_is_timedout( hnd_group[2] )  );

    ttim_start( hnd_group[2] );

    TEST_ASSERT_EQUAL( 25, ttim_get_remining_time( hnd_group[0] ) );
    TEST_ASSERT_EQUAL( 6, ttim_get_remining_time( hnd_group[1] ) );
    TEST_ASSERT_EQUAL( 13, ttim_get_remining_time( hnd_group[2] ) );

    TEST_ASSERT_FALSE( ttim_is_timedout( hnd_group[0] ) );
    TEST_ASSERT_FALSE( ttim_is_timedout( hnd_group[1] ) );
    TEST_ASSERT_FALSE(  ttim_is_timedout( hnd_group[2] )  );
}

#if 0
/**
   @brief Main test function

   @return int
 */
int main()
{
    UNITY_BEGIN();

    RUN_TEST( test_TTIM_12 );

    RUN_TEST( test_TTIM_0a );
    RUN_TEST( test_TTIM_0  );
    RUN_TEST( test_TTIM_1  );
    RUN_TEST( test_TTIM_2  );
    RUN_TEST( test_TTIM_4  );
    RUN_TEST( test_TTIM_5  );
    RUN_TEST( test_TTIM_6  );
    RUN_TEST( test_TTIM_7  );
    RUN_TEST( test_TTIM_8  );
    RUN_TEST( test_TTIM_9  );
    RUN_TEST( test_TTIM_10 );

    return UNITY_END();
}
#endif
