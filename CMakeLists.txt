# in reality we need cmake 3.8 (for C++17) but that would
# stop the automatic install of a new cmake through the
# cmakeinstall script below. So we lower the required
# version to 2.8, then install 3.8, and initially run into
# a cmake error in the running instance. At the next run,
# the new cmake then will complete without error..
cmake_minimum_required(VERSION 2.8)

project(dekaf2)

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
	message(STATUS "Please consider switching to CMake 3.8.0 or newer")
	set(OLD_CMAKE 1)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
	set (CPU_x86 1)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
		set(CPU_64_BIT 1)
	else()
		set(CPU_32_BIT 1)
	endif()
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
	set (CPU_ARM 1)
endif()

if (NOT OLD_CMAKE)
	set(CMAKE_CXX_STANDARD 17)
	# we do not force C++17 if not available
	# set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
	# do not even set C++14, older cmakes have trouble doing so. We will
	# set it manually in the compiler flags
endif()

# these are all configurable project options
OPTION(DEKAF2_INSTALL_HEADERS "Install headers" OFF)
OPTION(DEKAF2_USE_EXCEPTIONS "Use exceptions for error handling" OFF)
OPTION(DEKAF2_USE_FBSTRING_AS_KSTRING "Use Folly::FBString for KString" ON)
OPTION(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW "Use Folly::StringPiece for KStringView" ON)
OPTION(DEKAF2_USE_STD_STRING_FOR_DEBUG_BUILDS "Use std::string for debug builds" OFF)
OPTION(DEKAF2_USE_BOOST_MULTI_INDEX "Use Boost::Multi_Index" ON)
OPTION(DEKAF2_USE_OPTIMIZED_STRING_FIND "Use optimized string::find" ON)
OPTION(DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS "With deprecated KString member functions" OFF)
OPTION(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1 "Provide compatibility layer to dekaf1" OFF)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS "debug build with sanitizers and additional runtime checks" ON)
OPTION(DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS "use frozen constexpr hash for large maps (needs a lot of memory on comp)" ON)
OPTION(DEKAF2_WITH_FCGI "build with FCGI" OFF)

# The project version number.
set(VERSION_MAJOR   2) # This is DEKAF 2 !
set(VERSION_MINOR   0)
set(VERSION_PATCH   0)
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

if (DEKAF2_USE_STD_STRING_FOR_DEBUG_BUILDS)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		SET(DEKAF2_USE_FBSTRING_AS_KSTRING OFF)
	endif ()
endif ()
message (STATUS "FBSTRING: ${DEKAF2_USE_FBSTRING_AS_KSTRING}")

if (CPU_ARM)
	set (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	# there is typically not enough RAM for large frozen maps, except in case of cross compilation
	set (DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS OFF)
endif()

# suppress linker warnings for empty object files on OS X
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	SET(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
	SET(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif ()

# Add local cmake modules to the module path (used to find libraries which do not have find modules provided by cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

# check if we have the libproc library (or at least its headers)
find_package(libproc)
if (libproc_FOUND)
	set(DEKAF2_HAS_LIBPROC 1)
	include_directories(SYSTEM ${libproc_INCLUDE_DIRS})
endif ()

# if requested to integrate with dekaf1, check if it is available
if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	set (DPATH $ENV{dekaf})
	if (NOT DPATH)
		set(DPATH $ENV{HOME}/src/dekaf/src)
	endif ()
	if (EXISTS "${DPATH}/dekaf.h")
		set(DEKAF1_INCLUDE_PATH ${DPATH})
	else ()
		message(STATUS "Option to provide compatibility layer to dekaf1 is set, but dekaf1 can't be found. Option disabled.")
	endif ()
endif ()

# make sure we have the necessary version of cmake installed
#execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scriptsk/cmakeinstall -go")

message(STATUS ${CMAKE_GENERATOR})
if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
	if (NOT CMAKE_GENERATOR STREQUAL "Xcode")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-strong -fstack-check")
	endif ()
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if (CPU_x86)

		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
		elseif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.5)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
		else ()
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
		endif ()

	endif ()

	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.2)
		# mute the annoying warning on changing abi with gcc 7.1
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
	endif()

	if (OLD_CMAKE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.2)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
		else()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
		endif()
	endif()

	# increase amount of permitted constexpr depth (default is 512)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-depth=2048")

elseif (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]")

	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if (CPU_x86)

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mavx2 -mfpmath=sse -maes")

	endif ()

	if (OLD_CMAKE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif()

	# increase amount of permitted constexpr steps (default is around 1.000.000)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-steps=100000000")
	# increase amount of permitted constexpr depth (default is 512)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-depth=2048")

endif ()

message(STATUS "dekaf2 C++ compile flags for Release build : ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "dekaf2 C++ compile flags for Debug   build : ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CPU : ${CMAKE_SYSTEM_PROCESSOR}")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# add third party libraries included in source control
add_subdirectory(libs)

# add partial third party libraries included in source control
add_subdirectory(from)

# add externally installed third party libraries:

# MySQL (for KSQL) - if not available, KSQL will not build something meaningful
find_package(MYSQL)
if(MYSQL_FOUND)
	message(STATUS "configuring with MySQL ${MYSQL_VERSION_STRING}")
	set(DEKAF2_HAS_MYSQL "${MYSQL_VERSION_STRING}")
	if(MYSQL_VERSION_STRING STREQUAL "")
		set(DEKAF2_HAS_MYSQL 1)
		message(STATUS "MySQL found, CMAKE unable to report version.\n\tDEKAF2_HAS_MYSQL set to 1.")
	endif()
else()
	message(STATUS "MySQL not found - will build without. Please install MySQL if you want to use KSQL.")
endif()

# openssl (needed for boost asio)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # check later if this is still needed to override an insufficient std OpenSSL on OSX
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
endif ()
find_package(OpenSSL QUIET REQUIRED)
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

set(Boost_USE_STATIC_LIBS ON)
# boost asio (header only) needs system
# dekaf2's http part needs iostreams
find_package(Boost QUIET COMPONENTS system iostreams REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

# add a target to generate API documentation with Doxygen
# please make sure the graphviz package is installed, too, not only doxygen
find_package(Doxygen)
if (DOXYGEN_FOUND)

	# configure doxygen config file
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/doxyfile.in
		${CMAKE_CURRENT_BINARY_DIR}/doxyfile @ONLY)

	# execute doxygen
	add_custom_target(doc
		${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen" VERBATIM
		)

else ()

	add_custom_target(doc
		COMMENT "Doxygen was not found on this machine. Documentation can not be generated." VERBATIM
		)

endif (DOXYGEN_FOUND)

# create and include the build directory for generated files
set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")
include_directories("${GENERATED_FILES_DIR}")

# create the configuration header file
configure_file("bits/kconfiguration.h.in" "${GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)

set (HEADERS
	# KEEP ALPHABETIZED!!
	bits/kbaseshell.h
	bits/kbasepipe.h
	bits/kconfiguration.h
	bits/kcppcompat.h
	bits/kfilesystem.h
	bits/kistringstream.h
	bits/kmake_unique.h
	bits/kmru-std-vector.h
	bits/kmru-multi-index.h
	bits/kmutable_pair.h
	bits/kostringstream.h
	bits/kprops-multi-index.h
	bits/kprops-std-map.h
	bits/kstringviewz.h
	bits/ktemplate.h
	bits/kunique_deleter.h
	bits/kurldualencode.h
	bits/kvariant.h
	bits/simd/kfindfirstof.h
	dekaf2.h
	dekaf2all.h
	kassociative.h
	kbase64.h
	kcache.h
	kcasestring.h
	kcgi.h
	kchildprocess.h
	kchunkedtransfer.h
	kcompression.h
	kconnection.h
	kcrashexit.h
	kcrc.h
	kencode.h
	kexception.h
	kfdstream.h
	kfilesystem.h
	kformat.h
	kfrozen.h
	kgetruntimestack.h
	khash.h
	khmac.h
	khtmlcontentblocks.h
	khtmlentities.h
	khtmlparser.h
	khttpclient.h
	khttpinputfilter.h
	khttpoutputfilter.h
	khttpserver.h
	khttp_header.h
	khttp_method.h
	khttp_request.h
	khttp_response.h
	kinpipe.h
	kinshell.h
	kjson.h
	klog.h
	kmail.h
	kmessagedigest.h
	kmime.h
	kmpsearch.h
	kmru.h
	koptions.h
	kstringstream.h
	koutpipe.h
	koutputtemplate.h
	koutshell.h
	kparallel.h
	kpipe.h
	kprof.h
	kprops.h
	kquotedprintable.h
	kreader.h
	kregex.h
	kreplacer.h
	krow.h
	ksharedref.h
	ksignals.h
	ksmtp.h
	ksplit.h
	ksql.h
	ksslclient.h
	ksslstream.h
	kstack.h
	kstream.h
	kstreambuf.h
	kstring.h
	kstringutils.h
	kstringview.h
	ksubscribe.h
	ksystem.h
	ktcpclient.h
	ktcpserver.h
	ktcpstream.h
	ktimer.h
	kunixstream.h
	kuntar.h
	kurl.h
	kurlencode.h
	kuseragent.h
	kutf8.h
	kutf8iterator.h
	kwords.h
	kwriter.h
	)

set (SOURCES
	# KEEP ALPHABETIZED!!
	bits/kbasepipe.cpp
	bits/kbaseshell.cpp
	bits/kistringstream.cpp
	bits/kostringstream.cpp
	bits/kstringviewz.cpp
	bits/simd/kfindfirstof.cpp
	dekaf2.cpp
	kbase64.cpp
	kcasestring.cpp
	kcgi.cpp
	kchildprocess.cpp
	kchunkedtransfer.cpp
	kcompression.cpp
	kconnection.cpp
	kcrashexit.cpp
	kcrc.cpp
	kencode.cpp
	kfdstream.cpp
	kfilesystem.cpp
	kformat.cpp
	kgetruntimestack.cpp
	khmac.cpp
	khtmlcontentblocks.cpp
	khtmlentities.cpp
	khtmlparser.cpp
	khttpclient.cpp
	khttpinputfilter.cpp
	khttpoutputfilter.cpp
	khttpserver.cpp
	khttp_header.cpp
	khttp_method.cpp
	khttp_request.cpp
	khttp_response.cpp
	kinpipe.cpp
	kinshell.cpp
	kjson.cpp
	klog.cpp
	kmail.cpp
	kmessagedigest.cpp
	kmime.cpp
	kmpsearch.cpp
	koptions.cpp
	koutpipe.cpp
	koutputtemplate.cpp
	koutshell.cpp
	kparallel.cpp
	kpipe.cpp
	kprof.cpp
	kprops.cpp
	kquotedprintable.cpp
	kreader.cpp
	kregex.cpp
	kreplacer.cpp
	krow.cpp
	ksignals.cpp
	ksmtp.cpp
	ksplit.cpp
	ksql.cpp
	ksslstream.cpp
	kstream.cpp
	kstreambuf.cpp
	kstring.cpp
	kstringutils.cpp
	kstringview.cpp
	ksystem.cpp
	ktcpserver.cpp
	ktcpstream.cpp
	ktimer.cpp
	kunixstream.cpp
	kuntar.cpp
	kurl.cpp
	kurlencode.cpp
	kwords.cpp
	kwriter.cpp
	)

if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	set(HEADERS ${HEADERS}
		compat/kstring.h
		compat/kprops.h
		compat/kstack.h
		compat/dekaf.h
		)
endif ()

if (DEKAF2_INSTALL_HEADERS)

	set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
	set (DEKAF2_INSTALL_DIR "${DEKAF2_INCLUDE_DIR}/dekaf2")
	file(MAKE_DIRECTORY "${DEKAF2_INCLUDE_DIR}")
	file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}")
	file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}/bits")
	if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
		file(MAKE_DIRECTORY "${DEKAF2_INSTALL_DIR}/compat")
	endif ()

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

else ()

	set (DEKAF2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
	add_custom_target(lib-headers-dekaf2)

endif (DEKAF2_INSTALL_HEADERS)

# we add the HEADERS as well, as otherwise cmake based IDEs would not see them
# as belonging to the project. CMake would not need them here.
add_library(dekaf2 ${HEADERS} ${SOURCES})

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
	set (FSLIB libstdc++fs.a)
else ()
	set (FSLIB "")
endif ()

if (DEKAF2_WITH_FCGI)
	set (FCGILIBS
		fcgi
		fcgilib
	)
else ()
	set (FCGILIBS "")
endif ()

target_link_libraries(dekaf2
	fmt
	re2
	minifolly
	${FCGILIBS}
	${FSLIB}
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${MYSQL_LIBRARIES}
	${libproc_LIBRARIES}
	nlohmann-json
	frozen
	Threads::Threads
	z
	bz2
	)

add_dependencies(dekaf2 lib-headers-dekaf2)
target_include_directories(dekaf2
   PUBLIC "${DEKAF2_INCLUDE_DIR}" "${GENERATED_FILES_DIR}"
   PRIVATE "${MYSQL_INCLUDE_DIRS}"
   INTERFACE "${Boost_INCLUDE_DIR}" "${OPENSSL_INCLUDE_DIR}"
)
target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD)

add_executable(klog klog_main.cpp)
target_link_libraries(klog dekaf2)

add_executable(createdbc createdbc.cpp)
target_link_libraries(createdbc dekaf2)

add_executable(getentities getentities.cpp)
target_link_libraries(getentities dekaf2)

add_subdirectory(utests)
add_subdirectory(smoketests)
add_subdirectory(benchmarks)


