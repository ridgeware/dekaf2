# in reality we need cmake 3.8 (for C++17) but that would
# stop the automatic install of a new cmake through the
# cmakeinstall script below. So we lower the required
# version to 2.8, then install 3.8, and initially run into
# a cmake error in the running instance. At the next run,
# the new cmake then will complete without error..
cmake_minimum_required(VERSION 2.8)

project(dekaf2)

# The project version number.
set(VERSION_MAJOR   0)
set(VERSION_MINOR   0)
set(VERSION_PATCH   1)
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

# make sure we have the necessary version of cmake installed
execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/bits/cmakeinstall -go")

set(CMAKE_CXX_STANDARD 17)
# we do not force C++17 if not available
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# configure cpu architecture dependant optimization
if(CMAKE_COMPILER_IS_GNUCC AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mfpmath=sse -maes")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfpmath=sse -maes")
	elseif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.5)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
	else ()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
	endif()

endif()
message(STATUS "dekaf2 C++ compile flags for Release build: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# add third party libraries included in source control
add_subdirectory(libs)

# add partial third party libraries included in source control
add_subdirectory(from)

# add externally installed third party libraries:

# openssl (needed for boost asio)
find_package(OpenSSL QUIET REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
# boost asio (header only) needs system
find_package(Boost QUIET COMPONENTS system REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

# add a target to generate API documentation with Doxygen
# please make sure the graphviz package is installed, too, not only doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)

    # configure doxygen config file
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/doxyfile @ONLY)

    # execute doxygen
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )

else()

    add_custom_target(doc
        COMMENT "Doxygen was not found on this machine. Documentation can not be generated." VERBATIM
    )

endif(DOXYGEN_FOUND)

set (HEADERS
    bits/kconfiguration.h
    bits/ktemplate.h
    bits/kcppcompat.h
    bits/kmake_unique.h
    bits/kmutable_pair.h
    bits/kunique_deleter.h
    bits/simd/kfindfirstof.h
    dekaf2.h
    kstringview.h
    kstring.h
    kstring2to1.h
    kcache.h
    kregex.h
    klog.h
    kfile.h
    kprof.h
    kurl.h
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
    ksubscribe.h
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
    ksplit.h
)

set (SOURCES
	bits/simd/kfindfirstof.cpp
    dekaf2.cpp
    kstringview.cpp
    kstring.cpp
    kregex.cpp
    klog.cpp
    kfile.cpp
    kprof.cpp
    kurl.cpp
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
    minifolly
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
