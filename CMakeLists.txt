cmake_minimum_required(VERSION 3.10)

# Check if the path has been provided
if (NOT TTIM_CONFIG_PATH)
    message(FATAL_ERROR "You must specify TTIM_CONFIG_PATH with the path to the configuration folder")
endif()

# Add the main source files of the library
add_library(ttim STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/ttim.c )

# Set C99 standard for the ttim library
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) 

# Check if the config directory contains a CMakeLists.txt file
if (EXISTS "${TTIM_CONFIG_PATH}/CMakeLists.txt")
    # Handle external directory cases properly
    if (IS_ABSOLUTE "${TTIM_CONFIG_PATH}")
        set(CONFIG_BUILD_DIR "${CMAKE_BINARY_DIR}/config_build")
    else()
        set(CONFIG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TTIM_CONFIG_PATH}")
    endif()
    
    add_subdirectory(${TTIM_CONFIG_PATH} ${CONFIG_BUILD_DIR})
    set(CONFIG_FOUND TRUE)
else()
    message(STATUS "No CMakeLists.txt found in the configuration path, adding include directory only.")
    set(CONFIG_FOUND FALSE)
endif()

# Include the main and configuration directories in the target
target_include_directories(
    ttim 
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${TTIM_CONFIG_PATH}  # Provided configuration path
)

# Link additional sources from config if available
if (CONFIG_FOUND AND TARGET ttim_config)
    target_link_libraries(ttim PRIVATE ttim_config)
endif()
