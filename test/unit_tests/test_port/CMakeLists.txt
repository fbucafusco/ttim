if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

target_include_directories(ttim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(ttim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ttim_abstraction.c)
target_link_libraries(ttim PUBLIC unity)
