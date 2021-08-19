#ifndef TTIM_ABSTRACTION_H
#define TTIM_ABSTRACTION_H
#include <stdbool.h>

typedef struct
{
    int32_t timeout;
    int32_t elapsed;
    int32_t flags;
} mcu_timer_t;

void timebase_init( mcu_timer_t* hnd );
void timebase_start( mcu_timer_t* hnd, uint32_t time );
void timebase_add_elapsed( mcu_timer_t* hnd, uint32_t time );
bool timebase_is_stopped( mcu_timer_t* hnd );
bool timebase_is_running( mcu_timer_t* hnd );
bool timebase_is_timedout( mcu_timer_t* hnd );
int32_t timebase_timedout( mcu_timer_t* hnd );
uint32_t timebase_get_elapsed( mcu_timer_t* hnd );
void timebase_stop( mcu_timer_t* hnd  );
#endif
