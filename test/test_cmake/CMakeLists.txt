cmake_minimum_required(VERSION 3.10)

project(test_cmake LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  Unity
  URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.1.zip
)
FetchContent_MakeAvailable(Unity)

add_compile_definitions(TEST_STATIC NO_CEEDLING)

# add the ttim root lib with the configuration of it. 
set(TTIM_CONFIG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../test/test_port)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../  "${CMAKE_BINARY_DIR}/ttim_build" )

add_executable(test_cmake ${CMAKE_CURRENT_SOURCE_DIR}/../test/test_general.c )
target_link_libraries(test_cmake PUBLIC ttim unity)

enable_testing()
add_test(NAME test_cmake COMMAND test_cmake)
