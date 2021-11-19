[![coverage report](https://gitlab.com/fbucafusco/ttim/badges/master/coverage.svg)](https://gitlab.com/fbucafusco/ttim/-/commits/master)
[![pipeline status](https://gitlab.com/fbucafusco/ttim/badges/master/pipeline.svg)](https://gitlab.com/fbucafusco/ttim/-/commits/master)

# **Tickless Timer (ttim)**

This library aims to provide to baremetal programs the ability to implement asynchronous timers without
the need to tick periodically and, then, consume less power.

It can be used to implement time management modules in simple tickless RTOSses.

As an option (TTIM_PERIODIC_TICK==1), user choose to use regular periodic tick behavior.

# Documentation: [here](doc/doc.md)

# Examples:

| HW Platform | SW Platform | Description | URL |
| :-: | :-: | :-: | :-: |
| EDUCIAA | firmware_v3 + sapi | Baremetal 4 LEDs blinker | [link](https://github.com/fbucafusco/ttim_example_educiaa) |
| DISCOVERY | CubeMX HAL | Baremetal 1 LEDs blinker |  link **COMMING SOON**    |
