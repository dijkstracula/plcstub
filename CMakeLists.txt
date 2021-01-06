cmake_minimum_required(VERSION 3.10)

# set the project name
project(plctagstub VERSION 0.9)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

option(BUILD_WITH_DEBUG "Library and tests will print debug" OFF)

if (BUILD_WITH_DEBUG)
    add_compile_definitions(DEBUG)
endif()

# add the library
add_subdirectory(src)

# add the tests
add_subdirectory(test)
