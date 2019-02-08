# ===================================== THE BEGIN ==================================
#
# In reality we need cmake 3.8 (for C++17) but that would stop the automatic install
# of a new cmake through the cmakeinstall script below. So we lower the required
# version to 2.6, then install 3.13, and initially run into a cmake error in the
# running instance. At the next run, the new cmake then will complete without error..
#
# Note: C++20 is supported starting with 3.12
#
cmake_minimum_required(VERSION 2.6)
#
# ================================= BUILD SYSTEM SETUP =============================

# Make sure the build system is properly setup

EXECUTE_PROCESS(
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/buildsetup -quiet -go
)

# =================================== UPDATE CMAKE =================================

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
	message(STATUS "Please switch to CMake 3.8.0 or newer")
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmakeinstall -ask
	)
	message(FATAL_ERROR "Please restart the build now")
endif()

# ==================================== PROJECT =====================================

# When arriving here we guarantee at least cmake 3.8 being used
# cmake_minimum_required(VERSION 3.8)

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(dekaf2 VERSION 2.0.0 DESCRIPTION "C++14/17 rapid application development framework")

# force cpack to pick version numbers from this project, and not from a parent
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# ================================= CONFIG OPTIONS =================================

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
# in xcode 10, debug stepping crashes regularly when ASAN is active, so beware
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS_IN_XCODE "debug build with sanitizers and additional runtime checks in Xcode" OFF)
OPTION(DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS "use frozen constexpr hash for large maps (needs a lot of memory on comp)" ON)
OPTION(DEKAF2_WITH_FCGI "build with FCGI" OFF)

# =================================== C++ STANDARD =================================

set(CMAKE_CXX_STANDARD 17)
# we do not force C++17 if not available, cmake will pick the highest available
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================================== BUILD TYPE ==================================

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")
if (CMAKE_BUILD_TYPE STREQUAL "")
	# Release is the default build type
	set(CMAKE_BUILD_TYPE "Release")
endif()

# =================================== ENVIRONMENT ==================================

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
	set (CPU_x86 1)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
	set (CPU_x86 1)
elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
	set (CPU_ARM 1)
endif()

# these are supported by cmake 3.8 as well (but take care for other keys, they are
# typically only supported starting with 3.10 and cause an error with lower versions)
cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)

# ========================= SWITCHING CONFIG ON ENVIRONMENT =========================

if (DEKAF2_USE_STD_STRING_FOR_DEBUG_BUILDS)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		SET(DEKAF2_USE_FBSTRING_AS_KSTRING OFF)
		SET(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW OFF)
	endif ()
endif ()
message (STATUS "FBSTRING: ${DEKAF2_USE_FBSTRING_AS_KSTRING}")

if (CPU_ARM)
	message(STATUS "-- no ASAN/UBSAN support for ARM")
	set (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
endif()

math(EXPR NEEDED_FROZEN_MEMORY "7 * 1024")
if (TOTAL_PHYSICAL_MEMORY LESS NEEDED_FROZEN_MEMORY)
	message(STATUS "switching FROZEN off for large maps - not enough RAM (${TOTAL_PHYSICAL_MEMORY} MB)")
	# this is not enough RAM for compile time computation of large frozen maps
	set (DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS OFF)
endif()

# ================================ CMAKE INTERNALS =================================

# suppress linker warnings for empty object files on OS X
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	SET(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
	SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
	SET(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif ()

# variants only appeared in v10.14 of the MACOS SDK
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14")

# Add local cmake modules to the module path (used to find libraries which do not have find modules provided by cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

# ==================================== DEKAF1 ====================================

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

# ================================ ASAN / UBSAN =================================

if (CMAKE_GENERATOR STREQUAL "Xcode")
	if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS_IN_XCODE)
		message(STATUS "enabling ASAN/UBSAN runtime checks for Xcode")
		set(CMAKE_XCODE_GENERATE_SCHEME ON)
		set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER ON)
		set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN ON)
		set(CMAKE_XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER ON)
	endif()
else ()
	if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
		message(STATUS "enabling ASAN/UBSAN runtime checks")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-strong -fstack-check")
	endif ()
endif ()

# ============================= COMPILER FLAGS =================================

if (CMAKE_COMPILER_IS_GNUCXX)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if (CPU_x86)

		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mfpmath=sse -maes")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mfpmath=sse -maes")
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

	# increase amount of permitted constexpr depth (default is 512)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-depth=2048")

elseif (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]")

	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

	# configure cpu architecture dependant optimization
	if (CPU_x86)

		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -mfpmath=sse -maes")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -mfpmath=sse -maes")

	endif ()

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

# ============================== ZLIB / BZIP2 =================================

find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)

# ================================ LIBPROC ====================================

# check if we have the libproc library (or at least its headers)
find_package(libproc QUIET)
if (libproc_FOUND)
	set(DEKAF2_HAS_LIBPROC 1)
endif ()

# ================================= MYSQL ====================================

# MySQL (for KSQL) - if not available, KSQL will not build something meaningful
find_package(MYSQL QUIET)
if(MYSQL_FOUND)
	set(DEKAF2_HAS_MYSQL "${MYSQL_VERSION_STRING}")
	if(MYSQL_VERSION_STRING STREQUAL "")
		set(DEKAF2_HAS_MYSQL 1)
		message(STATUS "MySQL found, but CMAKE is unable to report MySQL version, DEKAF2_HAS_MYSQL set to 1.")
	else()
		message(STATUS "configuring with MySQL ${MYSQL_VERSION_STRING}")
	endif()
else()
	message(STATUS "MySQL not found - will build without. Please install MySQL if you want to use KSQL.")
endif()

# ================================ SQLITE ====================================

# SQLite3 (for KSQL and KSQLite) - if not available it's simply skipped
find_package(SQLite3 QUIET)
if(SQLITE3_FOUND)
	message(STATUS "configuring with SQLite3")
	set(DEKAF2_HAS_SQLITE3 1)
endif()

# ================================ OPENSSL ===================================

# openssl (needed for boost asio)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # this is still needed to override an insufficient std OpenSSL on OSX
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
endif ()
find_package(OpenSSL REQUIRED)

# ================================= BOOST ====================================

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_DEBUG_LIBS OFF)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
# we prefer the boost lib that we installed ourselves in /opt/lib ..
set(BOOST_ROOT "/opt/boost")
# boost asio (header only) needs system
# dekaf2's http part needs iostreams
find_package(Boost 1.54 COMPONENTS system iostreams)

if (NOT Boost_FOUND)
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/boostinstall -ask
	)
	message(FATAL_ERROR "Please restart the build now")
endif()

message(STATUS "boost-hdrs: ${Boost_INCLUDE_DIR}")
message(STATUS "boost-libs: ${Boost_LIBRARIES}")

# ================================= DOXYGEN ==================================

# add a target to generate API documentation with Doxygen
# please make sure the graphviz package is installed, too, not only doxygen
find_package(Doxygen)
if (DOXYGEN_FOUND)

	# configure doxygen config file
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/doc/doxyfile.in
		${CMAKE_CURRENT_BINARY_DIR}/doxyfile @ONLY
	)

	# execute doxygen
	add_custom_target(
		dekaf2-doc
		${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen" VERBATIM
	)

else ()

	add_custom_target(
		dekaf2-doc
		COMMENT "Doxygen was not found on this machine. Documentation can not be generated." VERBATIM
	)

endif (DOXYGEN_FOUND)

# ========================= THIRD PARTY SOURCE LIBS ============================

# add third party libraries included in source control
add_subdirectory(libs EXCLUDE_FROM_ALL)

# add partial third party libraries included in source control
add_subdirectory(from EXCLUDE_FROM_ALL)

# =============================== GENERATED FILES =================================

# create and include the build directory for generated files
set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")

# create the configuration header file
configure_file("bits/kconfiguration.h.in" "${GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)

# =============================== HEADER FILES ====================================

set (BITS_HEADERS
	# KEEP ALPHABETIZED
	bits/kasiostream.h
	bits/kbaseshell.h
	bits/kbasepipe.h
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
	bits/ksql_dbc.h
	bits/kstring_view.h
	bits/kstringviewz.h
	bits/ktemplate.h
	bits/kunique_deleter.h
	bits/kurldualencode.h
)
set(BITS_SIMD_HEADERS
	bits/simd/kfindfirstof.h
)
set(PUBLIC_HEADERS
	# KEEP ALPHABETIZED
	dekaf2.h
	dekaf2all.h
	kaddrplus.h
	kallocator.h
	kassociative.h
	kbar.h
	kbase64.h
	kbitfields.h
	kcache.h
	kcasestring.h
	kcgistream.h
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
	khtmlparser.h
	khttpclient.h
	khttperror.h
	khttpinputfilter.h
	khttpoutputfilter.h
	khttppath.h
	khttprouter.h
	khttpserver.h
	khttp_header.h
	khttp_method.h
	khttp_request.h
	khttp_response.h
	kinpipe.h
	kinshell.h
	kjson.h
	klambdastream.h
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
	krest.h
	krestserver.h
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
	kthreadpool.h
	ktimer.h
	kunixstream.h
	kuntar.h
	kurl.h
	kurlencode.h
	kuseragent.h
	kutf8.h
	kutf8iterator.h
	kvariant.h
	kwords.h
	kwriter.h
	kxml.h
	# keep this one at the end
	khtmlentities.h
)

if(SQLITE3_FOUND)
	set(HEADERS ${HEADERS}
		ksqlite.h
	)
endif()

if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	set(COMPAT_HEADERS
		compat/kstring.h
		compat/kprops.h
		compat/kstack.h
		compat/dekaf.h
	)
endif ()

set(HEADERS
	${BITS_HEADERS}
	${COMPAT_HEADERS}
	${BITS_SIMD_HEADERS}
	${PUBLIC_HEADERS}
)

# =============================== SOURCE FILES ====================================

set (SOURCES
	# KEEP ALPHABETIZED
	bits/kbasepipe.cpp
	bits/kbaseshell.cpp
	bits/kistringstream.cpp
	bits/kostringstream.cpp
	bits/ksql_dbc.cpp
	bits/kstringviewz.cpp
	bits/simd/kfindfirstof.cpp
	dekaf2.cpp
	kbar.cpp
	kbase64.cpp
	kcasestring.cpp
	kcgistream.cpp
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
	khtmlparser.cpp
	khttpclient.cpp
	khttperror.cpp
	khttpinputfilter.cpp
	khttpoutputfilter.cpp
	khttppath.cpp
	khttprouter.cpp
	khttpserver.cpp
	khttp_header.cpp
	khttp_method.cpp
	khttp_request.cpp
	khttp_response.cpp
	kinpipe.cpp
	kinshell.cpp
	kjson.cpp
	klambdastream.cpp
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
	krest.cpp
	krestserver.cpp
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
	kthreadpool.cpp
	ktimer.cpp
	kunixstream.cpp
	kuntar.cpp
	kurl.cpp
	kurlencode.cpp
	kwords.cpp
	kwriter.cpp
	kxml.cpp
	# keep this one at the end
	khtmlentities.cpp
)

if(SQLITE3_FOUND)
	set(SOURCES ${SOURCES}
		ksqlite.cpp
	)
endif()

# =============================== PSEUDO INSTALL ====================================

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

# ============================= CONDITIONAL LIBS ==================================

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
	set (FSLIB libstdc++fs.a)
# this does currently not work well with mixed gcc/clang systems
#elseif (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]" AND NOT APPLE)
#	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
#		set (FSLIB libc++experimental.a)
#	elseif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
#		set (FSLIB libc++fs.a)
#	endif()
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

# =============================== TARGETS ====================================

# we add the HEADERS as well, as otherwise cmake based IDEs would not see them
# as belonging to the project. CMake would not need them here.
add_library(dekaf2 ${HEADERS} ${SOURCES})

target_link_libraries(dekaf2
	PUBLIC
		fmt
		nlohmann-json
		${Boost_LIBRARIES}
		${OPENSSL_LIBRARIES}
		frozen
		minifolly
	PRIVATE
		${FCGILIBS}
		${FSLIB}
		${MYSQL_LIBRARIES}
		${SQLITE3_LIBRARIES}
		${libproc_LIBRARIES}
		Threads::Threads
		re2
		ZLIB::ZLIB
		BZip2::BZip2
	)

add_dependencies(dekaf2 lib-headers-dekaf2)
target_include_directories(dekaf2
	PUBLIC "${DEKAF2_INCLUDE_DIR}" "${GENERATED_FILES_DIR}" "${Boost_INCLUDE_DIR}" "${OPENSSL_INCLUDE_DIR}"
	PRIVATE "${MYSQL_INCLUDE_DIRS}" "${SQLITE3_INCLUDE_DIRS}" "${libproc_INCLUDE_DIRS}"
	INTERFACE
)
target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD PUBLIC DEKAF2)

add_executable(klog klog_main.cpp)
target_link_libraries(klog dekaf2)

add_executable(createdbc createdbc.cpp)
target_link_libraries(createdbc dekaf2)

add_executable(getentities EXCLUDE_FROM_ALL getentities.cpp)
target_link_libraries(getentities dekaf2)

# ============================ UNIT AND SMOKE TESTS ==================================

add_subdirectory(utests)
add_subdirectory(smoketests)
add_subdirectory(benchmarks)

# ================================= INSTALLATION =====================================

# for testing
set(CMAKE_INSTALL_PREFIX "~/src/install")
file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX})

install(TARGETS dekaf2 DESTINATION lib)
install(FILES ${PUBLIC_HEADERS} DESTINATION include/dekaf2)
install(FILES ${BITS_HEADERS} DESTINATION include/dekaf2/bits)
install(FILES ${BITS_SIMD_HEADERS} DESTINATION include/dekaf2/bits/simd)
install(FILES "${GENERATED_FILES_DIR}/kconfiguration.h" DESTINATION include/dekaf2)
if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	install(FILES ${COMPAT_HEADERS} DESTINATION include/dekaf2/compat)
endif()

file (GLOB FMT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/fmt-4.0.0/fmt/*.h")
install(FILES ${FMT_FILES} DESTINATION include/dekaf2/fmt)

file (GLOB FOLLY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/*.h")
install(FILES ${FOLLY_FILES} DESTINATION include/dekaf2/folly)
file (GLOB FOLLY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/portability/*.h")
install(FILES ${FOLLY_FILES} DESTINATION include/dekaf2/folly/portability)
file (GLOB FOLLY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/hash/*.h")
install(FILES ${FOLLY_FILES} DESTINATION include/dekaf2/folly/hash)
file (GLOB FOLLY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/detail/*.h")
install(FILES ${FOLLY_FILES} DESTINATION include/dekaf2/folly/detail)
file (GLOB FOLLY_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/concurrency/*.h")
install(FILES ${FOLLY_FILES} DESTINATION include/dekaf2/folly/concurrency)

file (GLOB NLOHMANN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/from/nlohmann/include/nlohmann/*.h")
install(FILES ${NLOHMANN_FILES} DESTINATION include/dekaf2/nlohmann)

file (GLOB FROZEN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/frozen/include/frozen/*.h")
install(FILES ${FROZEN_FILES} DESTINATION include/dekaf2/frozen)
file (GLOB FROZEN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/frozen/include/frozen/bits/*.h")
install(FILES ${FROZEN_FILES} DESTINATION include/dekaf2/frozen/bits)

set(CPACK_SOURCE_GENERATOR ZIP)
set(CPACK_SOURCE_IGNORE_FILES ${ignored_files})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
include(CPack)

# =================================== THE END ========================================
