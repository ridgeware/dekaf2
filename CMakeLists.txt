# in reality we need cmake 3.8 (for C++17) but that would
# stop the automatic install of a new cmake through the
# cmakeinstall script below. So we lower the required
# version to 2.8, then install 3.8, and initially run into
# a cmake error in the running instance. At the next run,
# the new cmake then will complete without error..
cmake_minimum_required(VERSION 2.8)

project(dekaf2)

OPTION(DEKAF2_INSTALL_HEADERS "Install headers" OFF)
OPTION(DEKAF2_USE_FBSTRING_AS_KSTRING "Use Folly::FBString for KString" ON)
OPTION(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW "Use Folly::StringPiece for KStringView" ON)
OPTION(DEKAF2_USE_BOOST_MULTI_INDEX "Use Boost::Multi_Index" ON)
OPTION(DEKAF2_USE_OPTIMIZED_STRING_FIND "Use optimized string::find" ON)
OPTION(DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS "With deprecated KString member functions" OFF)
OPTION(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1 "Provide compatibility layer to dekaf1" ON)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS "debug build with sanitizers and additional runtime checks" ON)

# The project version number.
set(VERSION_MAJOR   2) # This is DEKAF 2 !
set(VERSION_MINOR   0)
set(VERSION_PATCH   0)
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

if(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	set (DPATH $ENV{dekaf})
	if (NOT DPATH)
		set(DPATH $ENV{HOME}/src/dekaf/src)
	endif()
	if (EXISTS "${DPATH}/dekaf.h")
		set(DEKAF1_INCLUDE_PATH ${DPATH})
	else()
		message(STATUS "Option to provide compatibility layer to dekaf1 is set, but dekaf1 can't be found. Option disabled.")
	endif()
endif()

set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")

configure_file("bits/kconfiguration.h.in" "${GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)
include_directories("${GENERATED_FILES_DIR}")

# make sure we have the necessary version of cmake installed
#execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/bits/cmakeinstall -go")

set(CMAKE_CXX_STANDARD 17)
# we do not force C++17 if not available
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
   # did not yet find a way to teach Clang how to link against the asan lib.. so we do not compile with it
   if(NOT CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope")
   endif()
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-strong -fstack-check")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
		elseif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.5)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
		else ()
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
		endif()

	endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]")

	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")

	endif()

endif ()

message(STATUS "dekaf2 C++ compile flags for Release build : ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "dekaf2 C++ compile flags for Debug   build : ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# add third party libraries included in source control
add_subdirectory(libs)

# add partial third party libraries included in source control
add_subdirectory(from)

# add externally installed third party libraries:

# openssl (needed for boost asio)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # check later if this is still needed to override an insufficient std OpenSSL on OSX
	set(OPENSSL_ROOT_DIR /opt/local)
endif()
find_package(OpenSSL QUIET REQUIRED)
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

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
	# KEEP ALPHABETIZED!!
	bits/kconfiguration.h
	bits/ktemplate.h
	bits/kcppcompat.h
	bits/kmake_unique.h
	bits/kmutable_pair.h
	bits/kunique_deleter.h
	bits/kprops-multi-index.h
	bits/kprops-std-map.h
	bits/kmru-std-vector.h
	bits/kmru-multi-index.h
	bits/kbaseshell.h
	bits/kbasepipe.h
	bits/simd/kfindfirstof.h
	dekaf2.h
	dekaf2all.h
	kcache.h
	kcasestring.h
	kcgi.h
	kconnection.h
	kcrashexit.h
	kcurl.h
	kfdstream.h
	kfile.h
	kformat.h
	kgetruntimestack.h
	khash.h
	khttp.h
	khttp_header.h
	khttp_method.h
	kinpipe.h
	kinshell.h
	klog.h
	kmru.h
	kostringstream.h
	koutpipe.h
	koutshell.h
	kparallel.h
	kpipe.h
	kprof.h
	kprops.h
	kreader.h
	kregex.h
	ksharedref.h
	ksignals.h
	ksplit.h
	ksslclient.h
	ksslstream.h
	kstack.h
	kstream.h
	kstreambuf.h
	kstring.h
	kstringutils.h
	kstringview.h
	ksubscribe.h
	ktcpclient.h
	ktcpserver.h
	kurl.h
	kurlencode.h
	kuseragent.h
	kwebio.h
	kwriter.h
	)

set (SOURCES
	# KEEP ALPHABETIZED!!
	bits/kbasepipe.cpp
	bits/kbaseshell.cpp
	bits/simd/kfindfirstof.cpp
	dekaf2.cpp
	kcasestring.cpp
	kcgi.cpp
	kconnection.cpp
	kcrashexit.cpp
	kcurl.cpp
	kfdstream.cpp
	kfile.cpp
	kformat.cpp
	kgetruntimestack.cpp
	khash.cpp
	khttp.cpp
	khttp_header.cpp
	khttp_method.cpp
	kinpipe.cpp
	kinshell.cpp
	klog.cpp
	kostringstream.cpp
	koutpipe.cpp
	koutshell.cpp
	kparallel.cpp
	kpipe.cpp
	kprof.cpp
	kprops.cpp
	kreader.cpp
	kregex.cpp
	ksignals.cpp
	ksplit.cpp
	ksslstream.cpp
	kstream.cpp
	kstreambuf.cpp
	kstring.cpp
	kstringutils.cpp
	kstringview.cpp
	ktcpserver.cpp
	kurl.cpp
	kurlencode.cpp
	kwebio.cpp
	kwriter.cpp
	)

if(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	set(HEADERS ${HEADERS}
		compat/kstring.h
		compat/kprops.h
		compat/kstack.h
		compat/dekaf.h
		)
endif()

if (DEKAF2_INSTALL_HEADERS)

	set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
	set (DEKAF2_INSTALL_DIR "${DEKAF2_INCLUDE_DIR}/dekaf2")
	file(MAKE_DIRECTORY "${DEKAF2_INCLUDE_DIR}")
	file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}")
	file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}/bits")
	if(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
		file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}/compat")
	endif()

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
target_include_directories(dekaf2
   PUBLIC "${DEKAF2_INCLUDE_DIR}" "${GENERATED_FILES_DIR}"
   INTERFACE "${Boost_INCLUDE_DIR}"
)
target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD)

add_subdirectory(utests)
add_subdirectory(smoketests)
