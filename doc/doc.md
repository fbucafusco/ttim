## **Introduction**

This library provides interfaces for creating software timers for bare-metal projects.

To manage active timers, the library maintains a list where the first element is the timer with the closest timeout value. This design allows each tick interrupt to be processed in O(1) time complexity, regardless of the number of running timers.

### **Timer handlers**

Each timer has an associated handler that must be used when operating with the API. Refer to TTIM_MM_MODE for additional details.

### **Timeout events and callbacks**

For each timer timeout event, the user can choose how to interface with the application:

* Polling Architecture:
  If you opt for a polling-based design, use the ttim_is_timedout function within your main superloop to check for timeout events.

* Callback-Based Architecture:
  If you prefer to use callbacks, provide the callback functions (and any associated parameters) via the ttim_set or ttim_set_n_start functions. Note that these callbacks are executed in the ISR context (more generally where the ttim_update() funcion is called). Although callbacks can invoke any method in this library, they must not call ttim_update, which is reserved for use within each timer ISR.

## **State machine**
Each timer has the following state machine. <br>

<img src="./image/ttim_sm.svg">

## **API**

- **ttim_init**: Module initialization function.
- **ttim_ctor**: Object constructor. Only valid for configuration TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC
- **ttim_dtor**: Object desstructor. Only valid for configuration TTIM_MM_MODE==TTIM_MM_MODE_DYNAMIC

- **ttim_set**: Establish the time for a given timer. Also it configures its callback and parameter. The timer does not start, and the user needs to call ttim_start().
- **ttim_set_n_start**: Establish the time for a given timer and starts. Also it configures its callback and parameter.
- **ttim_start**: Starts the timer. If paused, it resumes the countdown from where it was left when ttim_pause was called.
- **ttim_stop**: Stops the timer.
- **ttim_pause**: Pauses the timer.
- **ttim_reset_n_restart**: Resets the countdown, an restarts the timer.
- **ttim_is_timedout**: Returns if the timer is timedout.
- **ttim_is_stopped**: Returns if the timer is stopped.
- **ttim_get_remining_time**: Returns the remaining time for a started timer.
- **ttim_update**: Funcion that should be called within the hardware timer isr.

## **Library configuration**

### General
The configuration is made by including in the project the file **ttim_config.h**.

* **TTIM_CB_MODE:** Callback Mode. It termines the mechanism that the module will use to communicate with the application.
    * **TTIM_CB_MODE_NONE:** No callbacks are used. The user should poll the timer status via *ttim_is_timedout*.

    * **TTIM_CB_MODE_SIMPLE:** The callback is called by passing just the timer id using this function prototype:
        `void <name>( TTIM_HND_T hnd );`

    * **TTIM_CB_MODE_PARAM:** The callback is called by passing the timer id that generated the call, and a user parameter using this function prototype:
        `void <name>( TTIM_HND_T hnd, void*param );`
        Using this option the user the lifetime of the pointed object must remain intact until the callback is called. 

* **TTIM_MM_MODE:** Memory Allocation Mode. It establish the memory allocation scheme for each timer.
    * **TTIM_MM_MODE_STATIC:** (DEFAULT)  In this mode the number of timers is fixed to TTIM_COUNT, and all the objects are allocated at compile time. All the timers are initialized with *ttim_init*.  The handlers for each timer are integer numbers starting from 0 to TTIM_COUNT-1.

   * **TTIM_MM_MODE_DYNAMIC:** In this mode dynamic memory is used for allocating timers at runtime. *ttim_init* should be called for initialize the module, but won't create any timers. The user should use ttim_ctor for creating a new timer that will return its handler.

* **TTIM_PERIODIC_TICK:** (DEFAULT = 0) It determines the algorithm for incrementing the partial timer counts. The user should call ttim_update() within a timer ISR. If 1, the isr will be fired in a regular basis.
If 0, the isr will be fired aperdiodically, based on the created timer dynamics.

### Cmake projects

For cmake projects you need to add the path to the root ttim forlder to your main ```CMakeLists.txt``` file.
Also, you will need to specify a ```TTIM_CONFIG_PATH``` symbol before, so ttim knows there to find the configuration for it.

You have 3 options for this:

1- Before calling ```cmake```:
```
export TTIM_CONFIG_PATH=/path/to/the/configuration
```
2- In the same ```cmake``` call:
```
cmake -DTTIM_CONFIG_PATH=/path/to/the/configuration
```
3- Within your ```CMakeLists.txt``` file:
```
set(TTIM_CONFIG_PATH /path/to/the/configuration)
```

If your ```TTIM_CONFIG_PATH``` contains a ```CMakeLists.txt``` file (perhaps due to complex configuration requirements), CMake will process it during its parsing phase. If such a file is absent, the ```TTIM_CONFIG_PATH``` will be included solely during the compilation process

## **Porting guide**
 
* **Low level timebase configuration:** The user should provide configuration for all the low level timebase behavior. The confioguration is done by macros. **The user should have in mind that the hw timebase must have the desired range of timeouts needed**.

    * **TTIM_TIMEBASE_TYPE:** Defines the object data type that instances the timebase mechanism. If defined all the macros below, should have a first parameter that defines the global object. If not define, the timebase is driven directly by modifying peripheral's registers or a global object.
    * **TTIM_TIMEBASE_INIT(TIMER_HND):** Macro that should initialize the timebase hardware. Could be deined empty if the peripheral is initiated by other actor in the system. 
    * **TTIM_TIMEBASE_START(TIMER_HND,TIME):** Macro that should start the timebase for a given time period (measured in the same unit that every timeout for each ttim object). I the time base is running when thi macro is called, the timer should be stoped before. 
    * **TTIM_TIMEBASE_STOP(TIMER_HND):** Macro that should stop the timebase.
    * **TTIM_TIMEBASE_ELAPSED(TIMER_HND):** Macro that should return the elapsed time since ```TTIM_TIMEBASE_START``` was called, in the same time units chosen.
    * **TTIM_TIMEBASE_IS_RUNNING(TIMER_HND):** Macro that should return a positive number if the timebase is running or 0 if not. 
    * **TTIM_TIMEBASE_NEEDS_STOP_ON_HANDLER:** Boolean (DEFAULT=1). If true, means ttim needs to call ```TTIM_TIMEBASE_STOP``` on each ```ttim_update()``` call before restarting the timebase, if needed.

* **System related configuration:**
    * **TTIM_CRITICAL_START / TTIM_CRITICAL_END:** Macros to define critical sections within the library.
    * **TTIM_ASSERT(CONDITION):** Provides the library an assertion option. This is mostly used for debugging. In production releases, when a false condition happens, please report the issue [here](https://gitlab.com/fbucafusco/ttim/-/issues).

    