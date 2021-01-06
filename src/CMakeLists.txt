cmake_minimum_required(VERSION 3.10)

# set the project name
project(plcstub VERSION 0.9)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(plcstub debug.c plcstub.c tagtree.c)

target_include_directories(plcstub PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/../include"
                          "${PROJECT_SOURCE_DIR}/.."
                          )


install(TARGETS plcstub DESTINATION lib)
install(FILES "${PROJECT_SOURCE_DIR}/../libplctag.h"
  DESTINATION include
  )
