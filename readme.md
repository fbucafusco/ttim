[![coverage report](https://gitlab.com/fbucafusco/ttim/badges/master/coverage.svg)](https://gitlab.com/fbucafusco/ttim/-/commits/master)
[![pipeline status](https://gitlab.com/fbucafusco/ttim/badges/master/pipeline.svg)](https://gitlab.com/fbucafusco/ttim/-/commits/master)

# **Tickless Timer (ttim)**

## Overview 

In many bare-metal programs, it's common to use a periodic tick—an interrupt that fires at regular intervals—to manage tasks, measure time, or schedule events. However, these periodic ticks force the processor to wake up frequently, which can lead to higher power consumption.

The library leverages the flexibility of modern timer hardware to implement asynchronous timers. Instead of relying on a continuously running tick that forces periodic wake-ups, these hardware timers can be programmed to generate interrupts only when needed. This means the processor can remain in a low-power state until a timer event occurs, significantly reducing power consumption.

The library can be used to build time management modules for simple tickless real-time operating systems (RTOSes). In these systems, the absence of a regular tick allows the processor to stay idle until an event is actually required, which is particularly beneficial for battery-powered or energy-sensitive applications.

As an option (TTIM_PERIODIC_TICK==1), user choose to use regular periodic tick behavior.

# Documentation: [here](doc/doc.md)

# Examples:

| HW Platform | SW Platform | Description | URL |
| :-: | :-: | :-: | :-: |
| EDUCIAA | firmware_v3 + sapi | Baremetal 4 LEDs blinker | [link](https://github.com/fbucafusco/ttim_example_educiaa) |
| DISCOVERY 1475 IOT 1A | CubeMX HAL | Baremetal 4 text sender | [link](https://github.com/fbucafusco/ttim_example_stm32_iot1a)   |
 
