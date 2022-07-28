# =================================== THE BEGIN ====================================
#
# In reality we need cmake 3.8 (for C++17) but that would stop the automatic install
# of a new cmake through the cmakeinstall script below. So we lower the required
# version to 2.6, then install 3.13, and initially run into a cmake error in the
# running instance. At the next run, the new cmake then will complete without error..
#
# Note: C++20 is supported starting with 3.12, targets from subdirectories with 3.13
#
# Starting with cmake 3.19, the minimal supported cmake version is 2.8.12
#
cmake_minimum_required(VERSION 2.8.12)
#
# ================================= BUILD SYSTEM SETUP =============================

# Make sure the build system is properly setup

if (NOT WIN32 AND NOT CYGWIN AND NOT DEKAF2_NO_BUILDSETUP)
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/buildsetup -quiet -go
	)
endif()

# =================================== UPDATE CMAKE =================================

if(CMAKE_VERSION VERSION_LESS "3.13.0")
	message(STATUS "Current CMake version is ${CMAKE_VERSION}")
	if (DEKAF2_NO_BUILDSETUP)
		message(FATAL_ERROR "Please make sure CMake 3.13 or newer is available")
	endif()
	message(STATUS "Please switch to CMake 3.13.0 or newer")
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmakeinstall -ask
	)
	message(FATAL_ERROR "Please restart the build now")
endif()

# ==================================== PROJECT =====================================

# When arriving here we guarantee at least cmake 3.13 being installed
cmake_minimum_required(VERSION 3.13)

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(dekaf2 VERSION 2.0.14 DESCRIPTION "C++14/17/20 rapid application development framework")

# force cpack to pick version numbers from this project, and not from a parent
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

include(ExternalProject)

# ================================= CONFIG OPTIONS =================================

# these are all configurable project options
OPTION(DEKAF2_USE_EXCEPTIONS "Use exceptions for error handling" OFF)
OPTION(DEKAF2_USE_JEMALLOC "Use jemalloc" OFF)
OPTION(DEKAF2_USE_FBSTRING_AS_KSTRING "Use Folly::FBString for KString" OFF)
OPTION(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW "Use dekaf2::detail::stringview::string_view for KStringView" OFF)
OPTION(DEKAF2_USE_STD_STRING_FOR_DEBUG_BUILDS "Use std::string for debug builds" OFF)
OPTION(DEKAF2_USE_BOOST_MULTI_INDEX "Use Boost::Multi_Index" ON)
OPTION(DEKAF2_USE_OPTIMIZED_STRING_FIND "Use optimized string::find" ON)
OPTION(DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS "With deprecated KString member functions" OFF)
OPTION(DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1 "Provide compatibility layer to dekaf1" OFF)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS "debug build with sanitizers and additional runtime checks" ON)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS_IN_XCODE "debug build with sanitizers and additional runtime checks in Xcode" OFF)
OPTION(DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS "use frozen constexpr hash for large maps (needs a lot of memory on comp)" ON)
OPTION(DEKAF2_WITH_FCGI "build with FCGI" OFF)
OPTION(DEKAF2_WITH_KLOG "build with KLOG" ON)
OPTION(DEKAF2_KLOG_WITH_TCP "enable KLOG to log on TCP/HTTP" ON)
OPTION(DEKAF2_PREFER_STATIC_LIBS "prefer linking with static libs over dynamic" OFF)

# =================================== C++ STANDARD =================================

set(CMAKE_CXX_STANDARD 20)

# we do not force C++20 if not available, cmake will pick the highest available
# dekaf2 supports 14/17/20, and also 11 for mostly all code, but uses smarter options
# with newer C++ standards
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================================== BUILD TYPE ==================================

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

if (CMAKE_GENERATOR MATCHES "Makefile")

	# single configuration setup - make sure we have a build type

	if (NOT CMAKE_BUILD_TYPE)
		# Release is the default build type
		set(CMAKE_BUILD_TYPE "Release")
	endif()

	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		if (DEKAF2_USE_STD_STRING_FOR_DEBUG_BUILDS)
			SET(DEKAF2_USE_FBSTRING_AS_KSTRING OFF)
		endif()
	else()
		SET(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	endif()

else()

	# clear any build type to make understood we build a multi configuration setup
	set(CMAKE_BUILD_TYPE "")

endif()

# prepare for install:
set(lib_version ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(dekaf2_bin_dest bin)
set(dekaf2_include_dest "include/${PROJECT_NAME}-${lib_version}")
set(dekaf2_main_lib_dest "lib/${PROJECT_NAME}-${lib_version}")
set(dekaf2_lib_dest "${dekaf2_main_lib_dest}/${CMAKE_BUILD_TYPE}")
# we use the dekaf2_share_dest to configure kconfiguration.h.in, so first set it
# with an absolute path which we remove later on
set(dekaf2_share_dest "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}-${lib_version}")

# =================================== ENVIRONMENT ==================================

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
	set (CPU_x86 64)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
	set (CPU_x86 32)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
	set (CPU_ARM 64)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
	set (CPU_ARM 32)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch")
	set (CPU_ARM 64)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set (IS_MACOS ON)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set (IS_WINDOWS ON)
	# get the Windows major version
	string(REGEX MATCH "^([0-9]+)" WINDOWS_VERSION ${CMAKE_SYSTEM_VERSION})
else ()
	set (IS_LINUX ON)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	set (DEKAF2_IS_APPLE_CLANG ON)
endif()

# these are supported by cmake 3.8 as well (but take care for other keys, they are
# typically only supported starting with 3.10 and cause an error with lower versions)
cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)

# ========================= SWITCHING CONFIG ON ENVIRONMENT =========================

if (CPU_ARM AND NOT CPU_ARM EQUAL 64)
	message(STATUS "no ASAN/UBSAN support for ARM32")
	set (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
endif()

if (IS_WINDOWS)
	# on Windows, disable asan/ubsan, and fbstrings/minifolly
	SET(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	SET(DEKAF2_USE_FBSTRING_AS_KSTRING OFF)
	# we check for unix sockets on Windows 10 or later once we know the boost version
else ()
	# on Unixes, enable unix sockets and pipes
	SET(DEKAF2_HAS_UNIX_SOCKETS ON)
	SET(DEKAF2_HAS_PIPES ON)
endif()

if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
	if (DEKAF2_USE_JEMALLOC)
		set(DEKAF2_USE_JEMALLOC OFF)
		message(STATUS "jemalloc cannot be used in an ASAN enabled build - switching jemalloc off")
	endif()
endif()

math(EXPR NEEDED_FROZEN_MEMORY "6 * 1024 + 512")
if (TOTAL_PHYSICAL_MEMORY LESS NEEDED_FROZEN_MEMORY)
	message(STATUS "switching FROZEN off for large maps - not enough RAM (${TOTAL_PHYSICAL_MEMORY} MB)")
	# this is not enough RAM for compile time computation of large frozen maps
	set (DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS OFF)
endif()

message (STATUS "FBSTRING: ${DEKAF2_USE_FBSTRING_AS_KSTRING}")

if (DEKAF2_USE_FBSTRING_AS_KSTRING)
	set(DEKAF2_HAS_MINIFOLLY ON)
else()
	set(DEKAF2_HAS_MINIFOLLY OFF)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set (DEKAF2_WITH_KLOG ON)
endif()

if (NOT DEKAF2_WITH_KLOG)
	set(DEKAF2_KLOG_WITH_TCP OFF)
endif()

# ================================ CMAKE INTERNALS =================================

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

# ============================== Prefer static libs? ==============================

if (DEKAF2_PREFER_STATIC_LIBS)
	set(LIBSFX_BACKUP ${CMAKE_FIND_LIBRARY_SUFFIXES})
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".a" LIBSFX_FOUND)
	if (LIBSFX_FOUND GREATER 0)
		list(REMOVE_AT CMAKE_FIND_LIBRARY_SUFFIXES ${LIBSFX_FOUND})
		list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a")
		message(STATUS "prefering static libraries: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
	endif()
endif()

# ============================== ZLIB / BZIP2 =================================

if (IS_WINDOWS)

	if (DEKAF2_BUILD_COMPRESSORS)

		# on windows we will build zlib and bz2lib ourselves if wanted (but not per default)

		file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")

		ExternalProject_Add(
			ext_ZLIB
			URL https://www.zlib.net/zlib-1.2.11.tar.gz
			URL_HASH SHA256=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=lib
		)

		add_library(ZLIB STATIC IMPORTED GLOBAL)
		set_target_properties(ZLIB PROPERTIES IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/lib/zlibstatic.lib)
		set_target_properties(ZLIB PROPERTIES IMPORTED_LOCATION_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/lib/zlibstaticd.lib)
		set_target_properties(ZLIB PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/include)
		add_dependencies(ZLIB ext_ZLIB)

		ExternalProject_Add(
			ext_BZip2
			GIT_REPOSITORY https://git.code.sf.net/p/bzip2/bzip2
			GIT_TAG bzip2-1_0_6
			GIT_SHALLOW ON
			UPDATE_COMMAND ""
			PATCH_COMMAND cmake -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bzip2build/CMakeLists.txt ${CMAKE_CURRENT_BINARY_DIR}/ext_BZip2-prefix/src/ext_BZip2
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=lib
		)

		add_library(BZip2 STATIC IMPORTED GLOBAL)
		set_target_properties(BZip2 PROPERTIES IMPORTED_LOCATION_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/lib/bz2.lib)
		set_target_properties(BZip2 PROPERTIES IMPORTED_LOCATION_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/lib/bz2d.lib)
		set_target_properties(BZip2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/include)
		add_dependencies(BZip2 ext_BZip2)

	endif()

else()

	find_package(ZLIB REQUIRED)
	find_package(BZip2 REQUIRED)

endif()

# ============================== LZMA / ZSTD =================================

find_package(LibLZMA QUIET)

if (LIBLZMA_FOUND)
	set(DEKAF2_HAS_LIBLZMA 1)
else()
	# FindLibLZMA.cmake is causing troubles if not found and
	# if the advanced state is not cleared..
	mark_as_advanced(CLEAR LIBLZMA_INCLUDE_DIR LIBLZMA_LIBRARY )
	set(LIBLZMA_LIBRARY "")
	set(LIBLZMA_INCLUDE_DIR "")
endif()

find_package(ZSTD QUIET)

if (ZSTD_FOUND)
	set(DEKAF2_HAS_LIBZSTD 1)
endif()

# ================================= Brotli ===================================

find_package(Brotli QUIET)

if (BROTLI_FOUND)
	set(DEKAF2_HAS_LIBBROTLI 1)
endif()

# ================================= LIBZIP =====================================

# libzip
# warning: libzip would collide with installed packages libtcod and minizip2
# - uninstall them if needed, or uncomment

if (IS_WINDOWS)
	find_package(LibZip QUIET)
else()
	find_package(LibZip REQUIRED)
endif()

if (LIBZIP_FOUND)
	set(DEKAF2_HAS_LIBZIP 1)
else()
	set(LIBZIP_LIBRARY "")
	set(LIBZIP_INCLUDE_DIR "")
endif()

# ================================ LIBPROC ====================================

# check if we have the libproc library (or at least its headers)
# Warning: QUIET option is not supported and leads to failed search
# but because without QUIET we get a nasty warning on other OS than MacOS
# we now only search on MacOS
if (IS_MACOS)
	find_package(libproc)
	if (libproc_FOUND)
		set(DEKAF2_HAS_LIBPROC 1)
	else()
		message(STATUS "Cannot find libproc. Automatic application name setup for logging will not work")
	endif ()
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
	message(STATUS "detected as mariadb: ${MYSQL_IS_MARIADB}")
	set(DEKAF2_MYSQL_IS_MARIADB ${MYSQL_IS_MARIADB})
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

# ================================ CURSES ====================================

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if(CURSES_FOUND)
	message(STATUS "configuring with NCurses")
	set(DEKAF2_HAS_NCURSES 1)
endif()

# ================================ OPENSSL ===================================

# openssl (needed for boost asio)

if (IS_WINDOWS)
	set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

if (NOT OPENSSL_ROOT_DIR)
	if (IS_MACOS)
		# this is still needed to override an insufficient std OpenSSL on OSX
		set(OPENSSL_ROOT_DIR /usr/local/opt/openssl@1.1 /usr/local/opt/openssl /opt/homebrew/opt/openssl@1.1 /opt/homebrew/opt/openssl)
	elseif (IS_WINDOWS)
		set(OPENSSL_ROOT_DIR "C:\\Program Files\\OpenSSL\\lib")
	endif ()
endif ()

find_package(OpenSSL REQUIRED)

if (IS_WINDOWS)
	# unfortunately openssl forgets to link their static versions against Windows's crypt32.lib
	# ( https://github.com/openssl/openssl/issues/1061 )
	set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} crypt32)
endif()

# ================================= BOOST ====================================

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
# to workaround a broken boost cmake config with boost 1.71 disable
# the boost cmake files
set(Boost_NO_BOOST_CMAKE 1)

if (NOT BOOST_ROOT)
	# only set BOOST_ROOT if not already done by caller
	if (IS_WINDOWS)
		set(BOOST_ROOT "C:\\boost")
	else()
		# we prefer the boost lib that we installed ourselves in /opt/lib ..
		set(BOOST_ROOT "/opt/boost")
	endif()
endif ()

message(STATUS "boost root: ${BOOST_ROOT}")

# boost asio (header only) needs system and date_time
# dekaf2's http part needs iostreams

if (IS_WINDOWS)
	# Windows boost binary builds have zlib and bzip2 included, so we do not
	# need external libs
	find_package(Boost 1.54 COMPONENTS system iostreams date_time zlib bzip2)
else()
	# Unix boost packages do rely on external versions of zlib and bzip2
	find_package(Boost 1.54 COMPONENTS system iostreams date_time)
endif()

if (NOT Boost_FOUND)
	if (DEKAF2_NO_BUILDSETUP OR IS_WINDOWS)
		message(FATAL_ERROR "please set BOOST_ROOT to the root directory of your boost install")
	else()
		EXECUTE_PROCESS(
			COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/boostinstall -ask
		)
		message(STATUS "")
		message(STATUS "Boost should now be installed.")
		message(STATUS "Please remove the file ${CMAKE_CURRENT_BINARY_DIR}/CMakeCache.txt")
		message(STATUS "or remove the full build folder ${CMAKE_CURRENT_BINARY_DIR}")
		message(STATUS "and then restart the cmake build")
		message(STATUS "")
		message(FATAL_ERROR "")
	endif()
endif()

message(STATUS "boost-hdrs: ${Boost_INCLUDE_DIR}")
message(STATUS "boost-libs: ${Boost_LIBRARIES}")

if (IS_WINDOWS)
	# enable unix sockets if Windows >= 10 and Boost >= 1.75
	if (WINDOWS_VERSION GREATER_EQUAL 10)
		if (Boost_VERSION_MACRO GREATER_EQUAL 107500)
			SET(DEKAF2_HAS_UNIX_SOCKETS ON)
		endif()
	endif()
endif()

# ================== build own iostreams lzma/zstd objects? =======================

# check if boost iostreams was compiled with support for (zstd and) lzma, or if the
# build is incomplete (currently, only debian seems to have boost iostreams object
# files for zstd and lzma, others only have the header files)
# We now always build our own zstd object files, as we want to support multithreading

if (NOT DEKAF2_IS_WINDOWS)
	if (DEKAF2_HAS_LIBLZMA)
		execute_process(COMMAND /bin/sh -c "nm \"${Boost_IOSTREAMS_LIBRARY}\" | grep -i lzma | wc -l" OUTPUT_VARIABLE SYM_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE)
		if (SYM_COUNT STREQUAL "")
			set(SYM_COUNT 0)
		endif()
		if (SYM_COUNT LESS 5)
			# no iostream lzma filter object found, we have to build it ourselves
			set(DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD 1)
		endif()
	endif()
endif()

# =================================== JEMalloc ====================================

if (DEKAF2_USE_JEMALLOC)

	if (NOT WIN32 AND NOT CYGWIN AND NOT DEKAF2_NO_BUILDSETUP AND NOT EXISTS /opt/jemalloc-prof)
		EXECUTE_PROCESS(
			COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/jemalloc-with-profiler -quiet -source ${CMAKE_CURRENT_BINARY_DIR}/jemalloc-prof -install /opt/jemalloc-prof
		)
	endif()

	find_package(JEMalloc)
	if (JEMalloc_FOUND)
		set(DEKAF2_HAS_JEMALLOC 1)
	endif()
endif()

# ============================== Prefer static libs? ==============================

if (DEKAF2_PREFER_STATIC_LIBS)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ${LIBSFX_BACKUP})
endif()

# we have to switch back to dynamic linking before searching for libct, which does
# not play well with iconv and gssapi when linked statically

# ================================ FreeTDS ====================================

# FreeTDS (for KSQL) - if not available it's simply skipped
find_package(FreeTDS QUIET)
if(FREETDS_FOUND)
	message(STATUS "configuring with FreeTDS/CTLIB")
	set(DEKAF2_HAS_FREETDS 1)
	set(DEKAF2_HAS_CTLIB 1)
else()
	message(STATUS "No MS SQL support. Install FreeTDS package if needed")
endif()

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

# =============================== GENERATED FILES =================================

# create and include the build directory for generated files
set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")

# create the configuration header file
configure_file("bits/kconfiguration.h.in" "${GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)

# create the cmake version comparison file for find_package()
configure_file("cmake/${PROJECT_NAME}-config-version.cmake.in" "${GENERATED_FILES_DIR}/${PROJECT_NAME}-config-version.cmake" @ONLY)

# create the cmake version setup file for find_package()
configure_file("cmake/${PROJECT_NAME}-setup.cmake.in" "${GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake" @ONLY)

# ======================== COMPILER AND LINKER OPTIONS ============================

# eat our own dog food
set (DEKAF2_INHERIT_BUILD_FLAGS ON)
include ("${GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake")

# ========================= THIRD PARTY SOURCE LIBS ============================

# add third party libraries included in source control
add_subdirectory(libs EXCLUDE_FROM_ALL)

# add partial third party libraries included in source control
add_subdirectory(from EXCLUDE_FROM_ALL)

# =============================== HEADER FILES ====================================

set (BITS_HEADERS
	# KEEP ALPHABETIZED
	bits/kasio.h
	bits/kasiostream.h
	bits/kbaseshell.h
	bits/kbasepipe.h
	bits/kcppcompat.h
	bits/kfilesystem.h
	bits/khash.h
	bits/kiostreams_filters.h
	bits/klogwriter.h
	bits/klogserializer.h
	bits/kmake_unique.h
	bits/kmru-std-vector.h
	bits/kmru-multi-index.h
	bits/kmutable_pair.h
	bits/kprops-multi-index.h
	bits/kprops-std-map.h
	bits/kstring_view.h
	bits/kstringviewz.h
	bits/ktarheader.h
	bits/ktemplate.h
	bits/kunique_deleter.h
	bits/kurldualencode.h
)

set(BITS_SIMD_HEADERS
	bits/simd/kfindfirstof.h
)

if (DEKAF2_HAS_LIBBROTLI)
	list(APPEND BITS_HEADERS bits/kbrotli.h)
endif()

if (DEKAF2_HAS_LIBZSTD)
	list(APPEND BITS_HEADERS bits/kzstd.h)
endif()

set(PUBLIC_HEADERS
	# KEEP ALPHABETIZED
	dekaf2.h
	dekaf2all.h
	kaddrplus.h
	kallocator.h
	kassociative.h
	katomic_object.h
	kbar.h
	kbase64.h
	kbitfields.h
	kbufferedreader.h
	kcache.h
	kcaseless.h
	kcasestring.h
	kcgistream.h
	kchildprocess.h
	kchunkedtransfer.h
	kcompression.h
	kconnection.h
	kcookie.h
	kcountingstreambuf.h
	kcrashexit.h
	kcrc.h
	kcsv.h
	kctype.h
	kdiff.h
	kduration.h
	kencode.h
	kexception.h
	kfdstream.h
	kfileserver.h
	kfilesystem.h
	kformat.h
	kfrozen.h
	kgetruntimestack.h
	khash.h
	kheapmon.h
	khmac.h
	khtmlcontentblocks.h
	khtmldom.h
	khtmlentities.h
	khtmlparser.h
	khttpclient.h
	khttpcompression.h
	khttperror.h
	khttpinputfilter.h
	khttplog.h
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
	kinstringstream.h
	kjoin.h
	kjson.h
	klambdastream.h
	klog.h
	kmail.h
	kmakecopyable.h
	kmessagedigest.h
	kmime.h
	kmpsearch.h
	kmru.h
	koptions.h
	kopenid.h
	koutpipe.h
	koutputtemplate.h
	koutshell.h
	koutstringstream.h
	kparallel.h
	kpersist.h
	kpipe.h
	kpool.h
	kpoll.h
	kprof.h
	kprops.h
	kquotedprintable.h
	kreader.h
	kregex.h
	kreplacer.h
	krest.h
	krestclient.h
	krestroute.h
	krestserver.h
	kron.h
	krow.h
	krsakey.h
	krsasign.h
	kscopeguard.h
	ksharedmemory.h
	ksharedref.h
	ksharedptr.h
	ksignals.h
	ksmtp.h
	ksnippets.h
	ksplit.h
	ksslclient.h
	ksslstream.h
	kstack.h
	kstream.h
	kstreambuf.h
	kstring.h
	kstringstream.h
	kstringutils.h
	kstringview.h
	ksubscribe.h
	ksystem.h
	ksystemstats.h
	ktcpclient.h
	ktcpserver.h
	ktcpstream.h
	kthreadpool.h
	kthreads.h
	kthreadsafe.h
	ktime.h
	ktimer.h
	kunixstream.h
	kuntar.h
	kurl.h
	kurlencode.h
	kuseragent.h
	kutf8.h
	kutf8iterator.h
	kvariant.h
	kversion.h
	kwebclient.h
	kwebobjects.h
	kwebsocket.h
	kwords.h
	kwriter.h
	kxml.h
)

if (DEKAF2_HAS_LIBZIP)
	list(APPEND PUBLIC_HEADERS
		kzip.h
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

if (DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD)
	list(APPEND HEADERS bits/from_boost/iostreams/lzma.hpp)
endif()

list(SORT HEADERS)

# =============================== SOURCE FILES ====================================

set (SOURCES
	# KEEP ALPHABETIZED
	bits/kbasepipe.cpp
	bits/kbaseshell.cpp
	bits/klogwriter.cpp
	bits/klogserializer.cpp
	bits/kstring_view.cpp
	bits/kstringviewz.cpp
	bits/simd/kfindfirstof.cpp
	dekaf2.cpp
	kbar.cpp
	kbase64.cpp
	kbufferedreader.cpp
	kcaseless.cpp
	kcasestring.cpp
	kcgistream.cpp
	kchildprocess.cpp
	kchunkedtransfer.cpp
	kcompression.cpp
	kconnection.cpp
	kcookie.cpp
	kcountingstreambuf.cpp
	kcrashexit.cpp
	kcrc.cpp
	kcsv.cpp
	kctype.cpp
	kdiff.cpp
	kduration.cpp
	kencode.cpp
	kfdstream.cpp
	kfileserver.cpp
	kfilesystem.cpp
	kformat.cpp
	kgetruntimestack.cpp
	khash.cpp
	kheapmon.cpp
	khmac.cpp
	khtmlcontentblocks.cpp
	khtmldom.cpp
	khtmlentities.cpp
	khtmlparser.cpp
	khttpclient.cpp
	khttpcompression.cpp
	khttperror.cpp
	khttpinputfilter.cpp
	khttplog.cpp
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
	kinstringstream.cpp
	kjson.cpp
	klambdastream.cpp
	klog.cpp
	kmail.cpp
	kmessagedigest.cpp
	kmime.cpp
	kmpsearch.cpp
	kopenid.cpp
	koptions.cpp
	koutpipe.cpp
	koutputtemplate.cpp
	koutshell.cpp
	koutstringstream.cpp
	kparallel.cpp
	kpipe.cpp
	kpoll.cpp
	kprof.cpp
	kprops.cpp
	kquotedprintable.cpp
	kreader.cpp
	kregex.cpp
	kreplacer.cpp
	krest.cpp
	krestclient.cpp
	krestroute.cpp
	krestserver.cpp
	kron.cpp
	krow.cpp
	krsakey.cpp
	krsasign.cpp
	kscopeguard.cpp
	ksharedmemory.cpp
	ksignals.cpp
	ksmtp.cpp
	ksnippets.cpp
	ksplit.cpp
	ksslstream.cpp
	kstream.cpp
	kstreambuf.cpp
	kstring.cpp
	kstringstream.cpp
	kstringutils.cpp
	kstringview.cpp
	ksystem.cpp
	ksystemstats.cpp
	ktcpserver.cpp
	ktcpstream.cpp
	kthreadpool.cpp
	kthreads.cpp
	ktime.cpp
	ktimer.cpp
	kunixstream.cpp
	kuntar.cpp
	kurl.cpp
	kurlencode.cpp
	kversion.cpp
	kwebclient.cpp
	kwebobjects.cpp
	kwebsocket.cpp
	kwords.cpp
	kwriter.cpp
	kxml.cpp
)

if (DEKAF2_HAS_LIBZIP)
	list(APPEND SOURCES
		kzip.cpp
	)
endif()

if (DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD)
	list(APPEND SOURCES bits/from_boost/iostreams/lzma.cpp)
endif()

if (DEKAF2_HAS_LIBBROTLI)
	list(APPEND SOURCES bits/kbrotli.cpp)
endif()

if (DEKAF2_HAS_LIBZSTD)
	list(APPEND SOURCES bits/kzstd.cpp)
endif()

list(SORT SOURCES)

if (NOT CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10.0)
	# keep this one at the top (it takes the most time to compile)
	list(INSERT SOURCES 0 khtmlentities4.cpp)
else()
	# move it to the end, as it needs a lot of RAM with older gccs
	list(APPEND SOURCES khtmlentities4.cpp)
endif()

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

if (CPU_ARM EQUAL 32)
	set(ARM_NEEDS_ATOMIC_LIB atomic)
endif()

# we add the HEADERS as well, as otherwise cmake based IDEs would not see them
# as belonging to the project. CMake would not need them here.
add_library(dekaf2 ${HEADERS} ${SOURCES})

if (DEKAF2_HAS_MINIFOLLY)
	set (MINIFOLLY minifolly)
endif ()

if (IS_WINDOWS)
	if (DEKAF2_BUILD_COMPRESSORS)
		set (COMPRESSORS
			ZLIB
			BZip2
		)
	endif()
else()
	set (COMPRESSORS
		${ZLIB_LIBRARIES}
		${BZIP2_LIBRARIES}
		${LIBLZMA_LIBRARIES}
		${ZSTD_LIBRARIES}
		${BROTLI_LIBRARIES}
	)
endif()

if (IS_LINUX)
	# librt is needed for posix shared memory on linux, but not on macOS (where linking to it
	# causes an error, but shared memory functions are nonetheless available)
	set (RT
		rt
	)
endif()

if (IS_MACOS)
	# we need the core foundation framework for the locale retrieval on MacOS..
	set (CORE_FOUNDATION "-framework Foundation")
endif()

if (IS_WINDOWS)
	# we need the dbghelp library for the data segment detection on Windows..
	set (DEBUG_HELP "dbghelp")
endif()

target_link_libraries(dekaf2
	PUBLIC
		fmt
		nlohmann-json
		${Boost_LIBRARIES}
		${OPENSSL_LIBRARIES}
		frozen
		${MINIFOLLY}
		Threads::Threads
	PRIVATE
		${FCGILIBS}
		${FSLIB}
		${libproc_LIBRARIES}
		${COMPRESSORS}
		${LIBZIP_LIBRARIES}
		${RT}
		${CORE_FOUNDATION}
		${DEBUG_HELP}
		re2
		${JEMalloc_LIBRARIES}
	)

target_include_directories(dekaf2
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
	$<BUILD_INTERFACE:${GENERATED_FILES_DIR}>
	$<INSTALL_INTERFACE:${dekaf2_include_dest}>
	$<INSTALL_INTERFACE:${dekaf2_lib_dest}>
	${Boost_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
	PRIVATE
	$<BUILD_INTERFACE:${SQLITE3_INCLUDE_DIRS}>
	$<BUILD_INTERFACE:${libproc_INCLUDE_DIRS}>
	$<BUILD_INTERFACE:${LIBZIP_INCLUDE_DIRS}>
	$<BUILD_INTERFACE:${BROTLI_INCLUDE_DIRS}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/from/google>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/croncpp/include>
	$<BUILD_INTERFACE:${JEMalloc_INCLUDE_DIRS}>
	# for centos libzip:
	/include
)

target_compile_definitions(dekaf2 PRIVATE DEKAF2_LIBRARY_BUILD PUBLIC DEKAF2)

add_library(ksql2 ksql.cpp ksql.h bits/ksql_dbc.cpp bits/ksql_dbc.h)
target_link_libraries(ksql2
	PUBLIC
		dekaf2
	PRIVATE
		${MYSQL_LIBRARIES}
		${FREETDS_LIBRARIES}
		${ARM_NEEDS_ATOMIC_LIB}
	)
target_include_directories(ksql2
	PRIVATE
	$<BUILD_INTERFACE:${MYSQL_INCLUDE_DIRS}>
	$<BUILD_INTERFACE:${FREETDS_INCLUDE_DIRS}>
)
add_dependencies(ksql2 dekaf2)

if(SQLITE3_FOUND)
	add_library(ksqlite ksqlite.cpp ksqlite.h)
	target_link_libraries(ksqlite
		PUBLIC
			dekaf2
		PRIVATE
			${SQLITE3_LIBRARIES}
		)
	add_dependencies(ksqlite dekaf2)
endif()

add_executable(klog klog_main.cpp)
target_link_libraries(klog dekaf2)
add_dependencies(klog dekaf2)

add_executable(createdbc createdbc.cpp)
target_link_libraries(createdbc dekaf2 ksql2)
add_dependencies(createdbc dekaf2 ksql2)

if (DEKAF2_HAS_NCURSES)
	add_executable(unixmenu unixmenu.cpp)
	target_include_directories(unixmenu PUBLIC CURSES_INCLUDE_DIRS)
	target_link_libraries(unixmenu dekaf2 ncurses)
	add_dependencies(unixmenu dekaf2)
endif()

add_executable(dekaf2project dekaf2project.cpp)
target_link_libraries(dekaf2project dekaf2)
add_dependencies(dekaf2project dekaf2)

add_executable(kurl samples/kurl.cpp samples/kurl.h)
target_link_libraries(kurl dekaf2)
add_dependencies(kurl dekaf2)

add_executable(getentities EXCLUDE_FROM_ALL getentities.cpp)
target_link_libraries(getentities dekaf2)
add_dependencies(getentities dekaf2)

add_executable(genunicodetables EXCLUDE_FROM_ALL genunicodetables.cpp)
target_link_libraries(genunicodetables dekaf2)
add_dependencies(genunicodetables dekaf2)

add_executable(diff-speedtest EXCLUDE_FROM_ALL from/google/diff-speedtest.cpp)
target_link_libraries(diff-speedtest dekaf2)
add_dependencies(diff-speedtest dekaf2)

# ============================ UNIT AND SMOKE TESTS ==================================

add_subdirectory(utests)
add_subdirectory(smoketests)
add_subdirectory(benchmarks)

# ================================= INSTALLATION =====================================

# now remove the install prefix from dekaf2_share_dest
string(REPLACE "${CMAKE_INSTALL_PREFIX}/" "" dekaf2_share_dest ${dekaf2_share_dest})

# always put this first, otherwise it risks to remove other already installed cmake config files
install(EXPORT ${PROJECT_NAME} DESTINATION "${dekaf2_lib_dest}")

# create dummy exports to satisfy cmake for the statically and PRIVATE linked in libraries (this is a bug in cmake)
if (DEKAF2_HAS_MINIFOLLY)
	install(TARGETS minifolly EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
endif()
install(TARGETS fmt EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
install(TARGETS nlohmann-json EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
install(TARGETS re2 EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
install(TARGETS frozen EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})

install(TARGETS dekaf2 ksql2 EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake DESTINATION ${dekaf2_main_lib_dest})
install(FILES ${GENERATED_FILES_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${dekaf2_main_lib_dest})
install(FILES ${GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake DESTINATION ${dekaf2_lib_dest})
install(TARGETS klog createdbc dekaf2project kurl DESTINATION ${dekaf2_bin_dest})
if (DEKAF2_HAS_NCURSES)
	install(TARGETS unixmenu DESTINATION ${dekaf2_bin_dest})
endif()
#install(TARGETS getentities genunicodetables DESTINATION ${dekaf2_bin_dest})

install(FILES ${PUBLIC_HEADERS} DESTINATION ${dekaf2_include_dest}/dekaf2)
install(FILES ksql.h DESTINATION ${dekaf2_include_dest}/dekaf2)
install(FILES bits/ksql_dbc.h DESTINATION ${dekaf2_include_dest}/dekaf2/bits)
if(SQLITE3_FOUND)
	install(TARGETS ksqlite EXPORT dekaf2 DESTINATION ${dekaf2_lib_dest})
	install(FILES ksqlite.h DESTINATION ${dekaf2_include_dest}/dekaf2)
endif()
install(FILES ${BITS_HEADERS} DESTINATION ${dekaf2_include_dest}/dekaf2/bits)
install(FILES ${BITS_SIMD_HEADERS} DESTINATION ${dekaf2_include_dest}/dekaf2/bits/simd)
install(FILES "${GENERATED_FILES_DIR}/kconfiguration.h" DESTINATION ${dekaf2_lib_dest})
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/mysql-newuser" DESTINATION ${dekaf2_bin_dest})
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/findcol" DESTINATION ${dekaf2_bin_dest})
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/kport" DESTINATION ${dekaf2_bin_dest})
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/my-ip-addr" DESTINATION ${dekaf2_bin_dest})
if (DEKAF2_WITH_COMPATIBILITY_LAYER_TO_DEKAF1)
	install(FILES ${COMPAT_HEADERS} DESTINATION ${dekaf2_include_dest}/compat)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/templates                       DESTINATION ${dekaf2_share_dest})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/from/nlohmann/include/nlohmann/ DESTINATION ${dekaf2_include_dest}/nlohmann FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/frozen/include/frozen/     DESTINATION ${dekaf2_include_dest}/frozen   FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/fmt-8.1.1/include/fmt/     DESTINATION ${dekaf2_include_dest}/fmt      FILES_MATCHING PATTERN "*.h")
if (DEKAF2_HAS_MINIFOLLY)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/from/folly/folly/           DESTINATION ${dekaf2_include_dest}/folly    FILES_MATCHING PATTERN "*.h")
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/from/folly/glog/            DESTINATION ${dekaf2_include_dest}/glog     FILES_MATCHING PATTERN "*.h")
endif()

set(CPACK_SOURCE_GENERATOR ZIP)
set(CPACK_SOURCE_IGNORE_FILES ${ignored_files})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

# ================================= UNINSTALL ========================================

add_custom_target(uninstall
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "/opt/jemalloc-prof"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/klog"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/kurl"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/createdbc"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/unixmenu"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/dekaf2project"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/mysql-newuser"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/kport"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/my-ip-addr"
)

# =================================== THE END ========================================
