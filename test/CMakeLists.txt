set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

enable_testing()

foreach( testname
    00-hello
    01-log_level
    02-set_tag_cb
    03-tag_create
    04-metatag_lookup
    05-tag-locking
 )
    add_executable( ${testname} ${testname}.c )
    target_include_directories(${testname} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
      )
    target_link_libraries( ${testname}
        plctagstub
        Threads::Threads
        #${CMAKE_THREAD_LIBS_INIT}
      )
    add_test(NAME ${testname} COMMAND ${testname})
endforeach()
