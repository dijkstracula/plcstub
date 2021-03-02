option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(plctagstub debug.c plcstub.c tagtree.c types.c)

target_include_directories(plctagstub PUBLIC
                          "${CMAKE_CURRENT_SOURCE_DIR}/../include"
                          "${CMAKE_CURRENT_SOURCE_DIR}/.."
                          )


install(TARGETS plctagstub DESTINATION lib)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../libplctag.h"
  DESTINATION include
  )
