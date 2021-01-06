cmake_minimum_required(VERSION 3.10)

# set the project name
project(plctagstub VERSION 0.9)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# add the library
add_subdirectory(src)
