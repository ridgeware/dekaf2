cmake_minimum_required(VERSION 3.2)

project(dekaf2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# add third party libraries
add_subdirectory(libs)

set (HEADERS
    dekaf.h
    kstring.h
    kcache.h
    kregex.h
    kcppcompat.h
    klog.h
    kfile.h
    kprof.h
    kurl.h
    kurl2.h
    kprops.h
    kcurl.h
    kwebio.h
    kstringutils.h
    kparallel.h
    ktemplate.h
    kformat.h
    ksharedref.h
    ksignals.h
    kcrashexit.h
    kgetruntimestack.h
    kreader.h
    kfdreader.h
    kwriter.h
    kfdwriter.h
    kstream.h
    kbaseshell.h
    kinshell.h
    koutshell.h
    kstack.h
)

set (SOURCES
    dekaf.cpp
    kstring.cpp
    kregex.cpp
    klog.cpp
    kfile.cpp
    kprof.cpp
    kurl.cpp
    kurl2.cpp
    kstringutils.cpp
    kparallel.cpp
    kcurl.cpp
    kwebio.cpp
    kformat.cpp
    ksignals.cpp
    kcrashexit.cpp
    kgetruntimestack.cpp
    kreader.cpp
    kwriter.cpp
    kfdreader.cpp
    kwriter.cpp
    kfdwriter.cpp
    kstream.cpp
    kbaseshell.cpp
    kinshell.cpp
    koutshell.cpp
)

# make sure we have the necessary version of cmake installed
execute_process(COMMAND "./cmakeinstall -go")

if (DEKAF2_INSTALL_HEADERS)

    set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
    file(MAKE_DIRECTORY ${DEKAF2_INCLUDE_DIR})

    foreach(HEADER ${HEADERS})
        add_custom_command(
            OUTPUT ${DEKAF2_INCLUDE_DIR}/${HEADER}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${HEADER} ${DEKAF2_INCLUDE_DIR}/${HEADER}
        )
        set(header_deps	${header_deps}	${DEKAF2_INCLUDE_DIR}/${HEADER})
    endforeach(HEADER dekaf_library_headers)

    add_custom_target(lib-headers
        DEPENDS ${header_deps}
    )

else()

    set (DEKAF2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    add_custom_target(lib-headers)

endif(DEKAF2_INSTALL_HEADERS)

add_library(dekaf2 ${HEADERS} ${SOURCES})
target_link_libraries(dekaf2 fmt re2 libstdc++fs.a Threads::Threads)
add_dependencies(dekaf2 lib-headers)
set_target_properties(dekaf2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${DEKAF2_INCLUDE_DIR})
target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD)

add_subdirectory(utests)
add_subdirectory(smoketests)
