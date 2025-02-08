cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(ttim_config STATIC ttim_abstraction.c)
target_include_directories(ttim_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ttim_config PUBLIC ttim unity)