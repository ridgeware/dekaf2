# in reality we need cmake 3.8 (for C++17) but that would
# stop the automatic install of a new cmake through the
# cmakeinstall script below. So we lower the required
# version to 2.8, then install 3.8, and initially run into
# a cmake error in the running instance. At the next run,
# the new cmake then will complete without error..
cmake_minimum_required(VERSION 2.8)

project(dekaf2)

# make sure we have the necessary version of cmake installed
execute_process(COMMAND "./cmakeinstall -go")

set(CMAKE_CXX_STANDARD 17)
# we do not force C++17 if not available
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# add third party libraries included in source control
add_subdirectory(libs)

# add externally installed third party libraries:

# openssl (needed for boost asio)
find_package(OpenSSL QUIET REQUIRED)

#set(Boost_USE_STATIC_LIBS ON)
# boost asio (header only) needs system
find_package(Boost QUIET COMPONENTS system REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

set (HEADERS
    bits/ktemplate.h
    bits/kcppcompat.h
    bits/kmake_unique.h
    bits/kmutable_pair.h
    dekaf2.h
    kstringview.h
    kstring.h
    kcache.h
    kregex.h
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
    ktcpclient.h
    ktcpserver.h
    khash.h
    kmru.h
    ksslstream.h
    ksslclient.h
)

set (SOURCES
    dekaf2.cpp
    kstring.cpp
    kregex.cpp
    klog.cpp
    kfile.cpp
    kprof.cpp
    kurl.cpp
    kurl2.cpp
    kstringview.cpp
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
    ktcpserver.cpp
    khash.cpp
    ksslstream.cpp
)

if (DEKAF2_INSTALL_HEADERS)

    set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
    set (DEKAF2_INSTALL_DIR "${DEKAF2_INCLUDE_DIR}/dekaf2")
    file(MAKE_DIRECTORY "${DEKAF2_INCLUDE_DIR}")
    file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}")
    file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}/bits")

    foreach(HEADER ${HEADERS})
        add_custom_command(
            OUTPUT "${DEKAF2_INCLUDE_DIR}/${HEADER}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}" "${DEKAF2_INSTALL_DIR}/${HEADER}"
        )
        set(header_deps	${header_deps}	"${DEKAF2_INCLUDE_DIR}/${HEADER}")
    endforeach(HEADER dekaf_library_headers)

    add_custom_target(lib-headers-dekaf2
        DEPENDS ${header_deps}
    )

else()

    set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_custom_target(lib-headers-dekaf2)

endif(DEKAF2_INSTALL_HEADERS)

# we add the HEADERS as well, as otherwise qtcreator would not see them
# as belonging to the project. CMake would not need them here.
add_library(dekaf2 ${HEADERS} ${SOURCES})

if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
    set (FSLIB libstdc++fs.a)
else()
    set (FSLIB "")
endif()

target_link_libraries(dekaf2
    fmt
    re2
    "${FSLIB}"
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

add_dependencies(dekaf2 lib-headers-dekaf2)
set_target_properties(dekaf2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${DEKAF2_INCLUDE_DIR}")
target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD)

add_subdirectory(utests)
add_subdirectory(smoketests)
