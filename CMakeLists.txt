# =================================== THE BEGIN ====================================
#
# In reality we need cmake 3.8 (for C++17) but that would stop the automatic install
# of a new cmake through the cmakeinstall script below. So we lower the required
# version to 2.8.12, then install 3.13, and initially run into a cmake error in the
# running instance. At the next run, the new cmake then will complete without error..
#
# Note: C++20 is supported starting with 3.12, targets from subdirectories with 3.13
#
# Starting with cmake 3.27, the minimal supported cmake version is 2.8.12
#
message(STATUS "======================== starting cmake configuration ========================")

if(CMAKE_VERSION VERSION_LESS "3.27.0")
	cmake_minimum_required(VERSION 2.8.12)
else()
	cmake_minimum_required(VERSION 3.13...3.31)
endif()
#
# ================================= BUILD SYSTEM SETUP =============================

# Make sure the build system is properly setup
if ((WIN32 OR CYGWIN) AND DEKAF2_NO_BUILDSETUP STREQUAL "")
	set (DEKAF2_NO_BUILDSETUP ON)
endif()

if (NOT DEKAF2_NO_BUILDSETUP)
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/buildsetup -quiet -go
 	)
endif()

# =================================== UPDATE CMAKE =================================

message(STATUS "Current CMake version is ${CMAKE_VERSION}")

if(CMAKE_VERSION VERSION_LESS "3.13.0")
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
cmake_minimum_required(VERSION 3.13...3.31)

# allow version number in project command
cmake_policy(SET CMP0048 NEW)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.27.0)
	# allow mixed case find_package names (Boost needs this with cmake 3.27)
	cmake_policy(SET CMP0144 OLD)
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.30.0)
	# use boosts own BoostConfig.cmake
	cmake_policy(SET CMP0167 OLD)
endif()

# declare the project
project(dekaf2 VERSION 2.0.72 DESCRIPTION "C++14/17/20/23 rapid application development framework")

# force cpack to pick version numbers from this project, and not from a parent
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

include(ExternalProject)

# ================================= CONFIG OPTIONS =================================

# these are all configurable project options
OPTION(DEKAF2_USE_EXCEPTIONS "Use exceptions for error handling" OFF)
OPTION(DEKAF2_USE_JEMALLOC "Use jemalloc" OFF)
OPTION(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW "Use dekaf2::detail::stringview::string_view for KStringView" OFF)
OPTION(DEKAF2_USE_OPTIMIZED_STRING_FIND "Use optimized string::find" ON)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS "debug build with sanitizers and additional runtime checks" ON)
OPTION(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS_IN_XCODE "debug build with sanitizers and additional runtime checks in Xcode" ON)
OPTION(DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS "use frozen constexpr hash for large maps (needs a lot of memory on comp)" ON)
OPTION(DEKAF2_WITH_FCGI "build with FCGI" OFF)
OPTION(DEKAF2_WITH_KLOG "build with KLOG" ON)
OPTION(DEKAF2_KLOG_WITH_TCP "enable KLOG to log on TCP/HTTP" ON)
OPTION(DEKAF2_PREFER_STATIC_LIBS "prefer linking with static libs over dynamic" OFF)
OPTION(DEKAF2_FORCE_STATIC_LIBS "force linking with static libs except for system libs" OFF)
OPTION(DEKAF2_USE_SHARED_BOOST "link dynamically to boost" OFF)
OPTION(DEKAF2_USE_PRECOMPILED_HEADERS "use precompiled headers" ON)
OPTION(DEKAF2_WITH_PEDANTIC_WARNINGS "enable pedantic warnings" ON)
OPTION(DEKAF2_FORCE_FMTLIB_OVER_STD_FORMAT "force the use of fmtlib even if std::format is available" ON)
OPTION(DEKAF2_USE_SYSTEM_FMTLIB "use a system installed fmtlib instead of our own copy" OFF)
OPTION(DEKAF2_WRAPPED_KJSON "create the KJSON class as a non-throwing wrapper type around the native type" ON)
OPTION(DEKAF2_LINK_TIME_OPTIMIZATION "force link time optimization for release builds" OFF)
OPTION(DEKAF2_BUILD_STATIC_DEKAF2 "build a static version of dekaf2" ON)
OPTION(DEKAF2_BUILD_SHARED_DEKAF2 "build a shared version of dekaf2" OFF)
OPTION(DEKAF2_DUAL_TARGET "install Release and Debug versions of dekaf2 side by side" ON)
OPTION(DEKAF2_VERSION_IN_TARGET "include version number in target" OFF)
OPTION(DEKAF2_INSTALL_BIN "build and install bin artifacts" ON)
OPTION(DEKAF2_INSTALL_LIB "build and install lib artifacts" ON)
OPTION(DEKAF2_INSTALL_SHARE "install share data" ON)
OPTION(DEKAF2_INSTALL_INCLUDE "install include files" ON)
OPTION(DEKAF2_ALL_LIBS "bind to all found libs (switch off and select single libs for better binary portability)" ON)
OPTION(DEKAF2_WITH_SIMDUTF "build with simd utf implementations" ON)
OPTION(DEKAF2_WITH_USER_AGENT_PARSER "build with user agent parser" ON)

# =================================== C++ STANDARD =================================

if (EXISTS "/etc/amazon-linux-release-cpe" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(DEKAF2_CXX_STANDARD 17)
	message(STATUS "Amazon Linux with ${CMAKE_CXX_COMPILER_ID} detected: lowering C++ mode to C++17")
else()
	set(DEKAF2_CXX_STANDARD 20)
	message(STATUS "using C++ mode C++20")
endif()

set(CMAKE_CXX_STANDARD ${DEKAF2_CXX_STANDARD})
#set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)

# we do not force C++20 if not available, cmake will pick the highest available
# dekaf2 supports 14/17/20, and also 11 for mostly all code, but uses smarter options
# with newer C++ standards
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================================== BUILD TYPE ==================================

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

# the cmake logic is sometimes case sensitive..
if (CMAKE_BUILD_TYPE STREQUAL "release" OR CMAKE_BUILD_TYPE STREQUAL "RELEASE")
	set(CMAKE_BUILD_TYPE "Release")
elseif (CMAKE_BUILD_TYPE STREQUAL "debug" OR CMAKE_BUILD_TYPE STREQUAL "DEBUG")
	set(CMAKE_BUILD_TYPE "Debug")
endif()

if (CMAKE_GENERATOR MATCHES "Makefile" OR CMAKE_GENERATOR MATCHES "Ninja")

	# single configuration setup - make sure we have a build type

	if (NOT CMAKE_BUILD_TYPE)
		# Release is the default build type
		set(CMAKE_BUILD_TYPE "Release")
	endif()

	if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		SET(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	endif()

else()

	# clear any build type to make understood we build a multi configuration setup
	set(CMAKE_BUILD_TYPE "")

endif()

if (DEKAF2_DUAL_TARGET)
	# we only support the Release and Debug output locations - other
	# build types have to pick either or
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set(DEKAF2_OUTPUT_TARGET "/Debug")
	else()
		set(DEKAF2_OUTPUT_TARGET "/Release")
	endif()
else()
	set(DEKAF2_OUTPUT_TARGET)
endif()

# prepare for install:

set (dekaf2_include_dest  "include/${PROJECT_NAME}"                            )
set (dekaf2_main_lib_dest "lib/${PROJECT_NAME}"                                )
set (dekaf2_lib_dest      "${dekaf2_main_lib_dest}${DEKAF2_OUTPUT_TARGET}"     )
set (dekaf2_share_dest    "share/${PROJECT_NAME}"                              )
set (dekaf2_bin_dest      "bin"                                                )

set (lib_version          "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"  )
set (lib_version_regex    "[0-9]+\\.[0-9]+$"                                   )

if (DEKAF2_VERSION_IN_TARGET)
	set (dekaf2_include_dest  "${dekaf2_include_dest}-${lib_version}"          )
	set (dekaf2_main_lib_dest "${dekaf2_main_lib_dest}-${lib_version}"         )
	set (dekaf2_lib_dest      "${dekaf2_main_lib_dest}${DEKAF2_OUTPUT_TARGET}" )
	set (dekaf2_share_dest    "${dekaf2_share_dest}-${lib_version}"            )
endif()

# =================================== ENVIRONMENT ==================================

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
	MATH(EXPR CPU_x86 "${CMAKE_SIZEOF_VOID_P} * 8")
	message(STATUS "CPU_x86: ${CPU_x86} bit OS")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "arm"       OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "aarch")
	MATH(EXPR CPU_ARM "${CMAKE_SIZEOF_VOID_P} * 8")
	message(STATUS "CPU_ARM: ${CPU_ARM} bit OS")
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

if (DEKAF2_WITH_SIMDUTF AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT "${CPU_x86}" STREQUAL "")
	# on amd64 in debug mode there are compile issues with simdutf for some
	# instruction sets (haskell)
	set(DEKAF2_WITH_SIMDUTF OFF)
	message(STATUS "simdutf: switched off for debug build on amd64")
elseif (DEKAF2_WITH_SIMDUTF AND "${CPU_ARM}" STREQUAL "32")
	set(DEKAF2_WITH_SIMDUTF OFF)
	message(STATUS "simdutf: switched off for arm32, the discrete implementation is faster")
elseif (DEKAF2_WITH_SIMDUTF AND "${CPU_ARM}" STREQUAL "64" AND CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
	set(DEKAF2_WITH_SIMDUTF OFF)
	message(STATUS "simdutf: switched off for arm64 with gcc < 8, missing vld1q_u32_x2 instruction")
else()
	message(STATUS "simdutf: ${DEKAF2_WITH_SIMDUTF}")
endif()

cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)

# ========================= SWITCHING CONFIG ON ENVIRONMENT =========================

if (CPU_ARM AND NOT CPU_ARM EQUAL 64)
	message(STATUS "no ASAN/UBSAN support for ARM32")
	set (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
endif()

if (IS_WINDOWS)
	# on Windows, disable asan/ubsan
	set(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	# we check for unix style local sockets on Windows 10 or later once we know the boost version
else ()
	# on Unixes, enable unix sockets and pipes
	set(DEKAF2_HAS_UNIX_SOCKETS ON)
	set(DEKAF2_HAS_PIPES ON)
endif()

if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
	if (DEKAF2_USE_JEMALLOC)
		set(DEKAF2_USE_JEMALLOC OFF)
		message(STATUS "jemalloc cannot be used in an ASAN enabled build - switching jemalloc off")
	endif()
endif()

if (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
	if (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS)
		# asan crashes on gcc 6
		set(DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	endif()
endif()

math(EXPR NEEDED_FROZEN_MEMORY "6 * 1024 + 512")
if (TOTAL_PHYSICAL_MEMORY LESS NEEDED_FROZEN_MEMORY)
	message(STATUS "switching FROZEN off for large maps - not enough RAM (${TOTAL_PHYSICAL_MEMORY} MB)")
	# this is not enough RAM for compile time computation of large frozen maps
	set (DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS OFF)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "debug")
	set (DEKAF2_WITH_KLOG ON)
endif()

if (NOT DEKAF2_WITH_KLOG)
	set(DEKAF2_KLOG_WITH_TCP OFF)
endif()

message(STATUS "KLog: ${DEKAF2_WITH_KLOG}")

if (DEKAF2_USE_PRECOMPILED_HEADERS)
	if (CMAKE_VERSION VERSION_LESS "3.16.0")
		message(STATUS "need cmake version >= 3.16 for precompiled headers (have ${CMAKE_VERSION}) - disabled")
		set(DEKAF2_USE_PRECOMPILED_HEADERS OFF)
	else()
		message(STATUS "build dekaf2 with precompiled headers")
		set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)
	endif()
endif()

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
	if (!DEKAF2_WITH_PEDANTIC_WARNINGS) # __int128 is not an official type!
		set(DEKAF2_MAY_HAVE_INT128 ON) # we still need to check for 32/64, but we don't do that inside cmake due to multi-arch builds
	endif()
endif()

if (CMAKE_COMPILER_IS_GNUCXX AND (CMAKE_CXX_STANDARD LESS 17 OR CMAKE_CXX_COMPILER_VERSION LESS 7))
	if (DEKAF2_WRAPPED_KJSON)
		message(STATUS "cannot use wrapped KJSON type with GCC and C++ < 17 or GCC < 7 - switching off")
		set(DEKAF2_WRAPPED_KJSON OFF)
	endif()
endif()

if (NOT DEKAF2_BUILD_STATIC_DEKAF2 AND NOT DEKAF2_BUILD_SHARED_DEKAF2)
	message(FATAL "do not switch both shared and static options off")
endif()

# ================================ CMAKE INTERNALS =================================

# Add local cmake modules to the module path (used to find libraries which do not have find modules provided by cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

# ============================== 3rd party libs ===============================

# search for the libs that are always acceptable as dynamic libs first
# (there may not be static versions anyways)

# ================================ FreeTDS ====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_FREETDS)
	# FreeTDS (for KSQL) - if not available it's simply skipped
	find_package(FreeTDS QUIET)
	if(FREETDS_FOUND)
		message(STATUS "configuring with FreeTDS/CTLIB")
		set(DEKAF2_HAS_FREETDS 1)
		set(DEKAF2_HAS_CTLIB 1)
	else()
		message(STATUS "No MS SQL support. Install FreeTDS package if needed")
	endif()
endif()

# =========================== Prefer static libs? =============================

# now prefer static libs if requested

if (DEKAF2_PREFER_STATIC_LIBS OR DEKAF2_FORCE_STATIC_LIBS)
	set(LIBSFX_BACKUP ${CMAKE_FIND_LIBRARY_SUFFIXES})
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".a" LIBSFX_FOUND)
	if (LIBSFX_FOUND GREATER 0)
		list(REMOVE_AT CMAKE_FIND_LIBRARY_SUFFIXES ${LIBSFX_FOUND})
		list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a")
		message(STATUS "prefering static libraries: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
	endif()
endif()

# ================================= Threads ===================================

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ============================== ZLIB / BZIP2 =================================

find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)

# =========================== Force static libs? ==============================

# now force static libs if requested

if (DEKAF2_FORCE_STATIC_LIBS)
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".lib" LIBSFX_FOUND)
	if (LIBSFX_FOUND GREATER 0)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.lib")
	else()
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	endif()
	message(STATUS "forcing static libraries: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
endif()

# ================================ OPENSSL ===================================

# always required
find_package(OpenSSL REQUIRED)
if (OPENSSL_VERSION VERSION_GREATER_EQUAL "3.3")
	# OpenSSL released QUIC first with v3.2.0, but we want at least v3.3,
	# because SSL_poll() was implemented only then
	message(STATUS "OpenSSL ${OPENSSL_VERSION} has QUIC")
	set(DEKAF2_HAS_OPENSSL_QUIC 1)
endif()

# ================================== LZMA ====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_LZMA)
	find_package(LibLZMA QUIET)

	if (LIBLZMA_FOUND)
		set(DEKAF2_HAS_LIBLZMA 1)
	else()
		# FindLibLZMA.cmake is causing troubles if not found and
		# if the advanced state is not cleared..
		mark_as_advanced(CLEAR LIBLZMA_INCLUDE_DIR LIBLZMA_LIBRARY LIBLZMA_LIBRARIES)
		set(LIBLZMA_LIBRARY "")
		set(LIBLZMA_INCLUDE_DIR "")
		set(LIBLZMA_LIBRARIES "")
	endif()
endif()

# ================================== ZSTD ====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_ZSTD)
	find_package(ZSTD QUIET)

	if (ZSTD_FOUND)
		set(DEKAF2_HAS_LIBZSTD 1)
	endif()
endif()

# ================================= Brotli ===================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_BROTLI)
	find_package(Brotli QUIET)

	if (BROTLI_FOUND)
		set(DEKAF2_HAS_LIBBROTLI 1)
	endif()
endif()

# ================================= LIBZIP =====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_ZIP)
	# libzip
	# warning: libzip would collide with installed packages libtcod and minizip2
	# - uninstall them if needed, or uncomment

	# do not require libzip ..
	find_package(LibZip QUIET)

	if (LIBZIP_FOUND)
		set(DEKAF2_HAS_LIBZIP 1)
	else()
		set(LIBZIP_LIBRARY "")
		set(LIBZIP_INCLUDE_DIR "")
	endif()
endif()

# ================================ YAML-CPP ====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_YAML)
	find_package(yaml-cpp)
	# there is no YAML_CPP_FOUND, so test for the libs
	if (NOT "${YAML_CPP_LIBRARIES}" STREQUAL "")
		# yaml-cpp has unfortunately changed over time the way to link to it, and
		# has not maintained a backup path.
		# It is now required to use yaml-cpp::yaml-cpp, previously it was necessary
		# to use yaml-cpp.
		# This will e.g. still hurt on AmazonLinux 2023.

		if (false)
			message(STATUS "YAML_CPP_LIBRARIES:   ${YAML_CPP_LIBRARIES}")
			message(STATUS "YAML_CPP_LIBRARY_DIR: ${YAML_CPP_LIBRARY_DIR}")
			message(STATUS "YAML_CPP_INCLUDE_DIR: ${YAML_CPP_INCLUDE_DIR}")
		endif()

		if ("${YAML_CPP_LIBRARIES}" STREQUAL "yaml-cpp" AND NOT "${YAML_CPP_LIBRARY_DIR}" STREQUAL "")
			# fedora does not use the more modern name even in latest releases (41)
			if (NOT EXISTS "/etc/fedora-release")
				set(YAML_CPP_LIBRARIES "yaml-cpp::yaml-cpp")
			endif()
		endif()

		if (false)
			message(STATUS "YAML_CPP_LIBRARIES now: ${YAML_CPP_LIBRARIES}")
		endif()

		set(DEKAF2_HAS_LIBYAML 1)

	endif()
endif()

if (NOT DEKAF2_HAS_LIBYAML)
	if (DEKAF2_WITH_USER_AGENT_PARSER)
		message(STATUS "missig libyaml, therefore no user agent parser")
		set (DEKAF2_WITH_USER_AGENT_PARSER "")
	endif()
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

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_MYSQL)
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
		find_package(unofficial-libmariadb QUIET)
		if (unofficial-libmariadb_FOUND)
			set(DEKAF2_MYSQL_IS_MARIADB 1)
			set(DEKAF2_HAS_MYSQL 1)
			message(STATUS "configuring with MariaDB")
		else()
			message(STATUS "MySQL not found - will build without. Please install MySQL if you want to use KSQL.")
		endif()
	endif()
endif()

# ================================ SQLITE ====================================

# this one is not for dekaf2 but for single target ksqlite
# SQLite3 (for KSQL and KSQLite) - if not available it's simply skipped
find_package(SQLite3 QUIET)
if(SQLITE3_FOUND)
	message(STATUS "configuring with SQLite3")
	set(DEKAF2_HAS_SQLITE3 1)
endif()

# ================================ CURSES ====================================

# this one is not for dekaf2 but for single target unixmenu
# - search for it unconditionally
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses QUIET)
if(CURSES_FOUND)
	message(STATUS "configuring with NCurses")
	set(DEKAF2_HAS_NCURSES 1)
endif()

# =============================== NGHTTP2/3 ==================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_NGHTTP2)
	find_package(NGHTTP2)
	if (NGHTTP2_FOUND)
		set(DEKAF2_HAS_NGHTTP2 1)
	endif()

	if (DEKAF2_HAS_OPENSSL_QUIC)
		if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_NGHTTP3)
			# only search for nghttp3 if we have an openssl version supporting QUIC
			# - this makes some ifdefs shorter..
			find_package(NGHTTP3)
			if (NGHTTP3_FOUND)
				set(DEKAF2_HAS_NGHTTP3 1)
			endif()
		endif()
	endif()
endif()

# ================================= BOOST ====================================

# always required
if (NOT DEKAF2_USE_SHARED_BOOST)
	set(Boost_USE_STATIC_LIBS ON)
endif()
set(Boost_USE_MULTITHREADED ON)
# to workaround a broken boost cmake config with boost 1.71 disable
# the boost cmake files
set(Boost_NO_BOOST_CMAKE ON)
# most annoying warning ever - finally we can suppress it (cmake 3.20++)
set(Boost_NO_WARN_NEW_VERSIONS ON)

if (BOOST_ROOT)
	message(STATUS "boost root externally set to: ${BOOST_ROOT}")
elseif (EXISTS "/opt/boost")
	# check if we installed a boost build in /opt/boost
	set(BOOST_ROOT "/opt/boost")
	# force static boost, the source build does not give dynamic libs
	set(Boost_USE_STATIC_LIBS ON)
	message(STATUS "setting boost root to: ${BOOST_ROOT}")
endif ()

# boost asio (header only) needs system and date_time
# dekaf2's http part needs iostreams

find_package(Boost 1.54 COMPONENTS system iostreams serialization)

# following are the boost modules that vcpkg would need for a
# successful build
#
# "boost-asio"
# "boost-crc"
# "boost-iostreams"
# "boost-multi-index"
# "boost-serialization"
# "boost-system"

if (EXISTS "${Boost_INCLUDE_DIR}/multi_index")
	# vcpackage makes multi_index a component..
	find_package(Boost 1.54 COMPONENTS multi_index)
endif()

if (NOT Boost_FOUND)
	if (DEKAF2_NO_BUILDSETUP)
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

if (IS_WINDOWS_NONONO)
	# windows local sockets require more changes than simply
	# swapping them in for tcp sockets - currently disabled
	#
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

if (DEKAF2_HAS_LIBLZMA)
	if (IS_MACOS OR IS_WINDOWS)
		# On the mac we now always fall back to our own LZMA implementation, as boost 1.79
		# comes with an lzma object file with multi threading, but fails to link against the
		# single threading lzma libs that we have installed.
		set(DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD ON)
	else ()
		# with linux, check the symbols in the boost lib for lzma
		execute_process(COMMAND /bin/sh -c "nm \"${Boost_IOSTREAMS_LIBRARY}\" | grep -i lzma | wc -l" OUTPUT_VARIABLE SYM_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE)
		if (SYM_COUNT STREQUAL "")
			set(SYM_COUNT 0)
		endif()
		if (SYM_COUNT LESS 5)
			# no iostream lzma filter object found, we have to build it ourselves
			set(DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD ON)
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

# ================== show list of found/notfound optional libs ==================

function(LibState State Name Lib)
	if ("${State}" STREQUAL "")
		message(STATUS "${Name} not found")
	else()
		if ("${Lib}" STREQUAL "")
			message(STATUS "${Name} found")
		else()
			message(STATUS "${Name} found (${Lib})")
		endif()
	endif()
endfunction()

message(STATUS "optional libraries:")
LibState("${DEKAF2_HAS_LIBZIP}"    "ZIP      :" "${LIBZIP_LIBRARIES}" )
LibState("${DEKAF2_HAS_LIBLZMA}"   "LZMA     :" "${LIBLZMA_LIBRARIES}")
LibState("${DEKAF2_HAS_LIBZSTD}"   "ZSTD     :" "${ZSTD_LIBRARIES}"   )
LibState("${DEKAF2_HAS_LIBYAML}"   "YAML     :" "${YAML_CPP_LIBRARIES}")
LibState("${DEKAF2_HAS_MYSQL}"     "MYSQL    :" "${MYSQL_LIBRARIES}"  )
LibState("${DEKAF2_HAS_SQLITE3}"   "SQLITE   :" "${SQLITE3_LIBRARIES}")
LibState("${DEKAF2_HAS_LIBBROTLI}" "BROTLI   :" "${BROTLI_LIBRARIES}" )
LibState("${DEKAF2_HAS_NGHTTP2}"   "NGHTTP2  :" "${NGHTTP2_LIBRARIES}")
LibState("${DEKAF2_HAS_NGHTTP3}"   "NGHTTP3  :" "${NGHTTP3_LIBRARIES}")
LibState("${DEKAF2_HAS_FREETDS}"   "FreeTDS  :" "${FREETDS_LIBRARIES}")
LibState("${DEKAF2_HAS_JEMALLOC}"  "JEMALLOC :" ""                    )

# =============================== COMPILE TESTS =================================

# create the build directory for generated files
set(DEKAF2_GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${DEKAF2_GENERATED_FILES_DIR}")
if (NOT EXISTS "${DEKAF2_GENERATED_FILES_DIR}/dekaf2")
	# create a symlink to simulate the <dekaf2/> path for includes at library build time
	# (we add DEKAF2_GENERATED_FILES_DIR to the include locations below)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}" "${DEKAF2_GENERATED_FILES_DIR}/dekaf2"
		RESULT_VARIABLE EXEC_RESULT
	)
	if (NOT EXEC_RESULT EQUAL 0 OR NOT EXISTS "${DEKAF2_GENERATED_FILES_DIR}/dekaf2")
		if (IS_WINDOWS)
			message(FATAL_ERROR "cannot create build time include dir symbolic link ${DEKAF2_GENERATED_FILES_DIR}/dekaf2 - see here how to fix it: https://stackoverflow.com/a/76292992")
		else()
			message(FATAL_ERROR "cannot create build time include dir symbolic link ${DEKAF2_GENERATED_FILES_DIR}/dekaf2")
		endif()
	endif()
endif ()

# test warning flags
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wstringop-overflow             DEKAF2_HAS_WARN_STRINGOP_OVERFLOW            )
check_cxx_compiler_flag(-Wstringop-overread             DEKAF2_HAS_WARN_STRINGOP_OVERREAD            )
check_cxx_compiler_flag(-Warray-bounds                  DEKAF2_HAS_WARN_ARRAY_BOUNDS                 )
check_cxx_compiler_flag(-Wdeprecated-declarations       DEKAF2_HAS_WARN_DEPRECATED_DECLARATIONS      )
check_cxx_compiler_flag(-Wliteral-suffix                DEKAF2_HAS_WARN_LITERAL_SUFFIX               )
check_cxx_compiler_flag(-Wreserved-user-defined-literal DEKAF2_HAS_WARN_RESERVED_USER_DEFINED_LITERAL)
check_cxx_compiler_flag(-Wshift-count-overflow          DEKAF2_HAS_SHIFT_COUNT_OVERFLOW              )

try_compile(DEKAF2_HAS_NANOSECONDS_SYS_CLOCK
			"${DEKAF2_GENERATED_FILES_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_nanoseconds_sys_clock_test.cpp")

if(DEKAF2_HAS_NANOSECONDS_SYS_CLOCK)
	message(STATUS "std::chrono::system_clock has nanosecond resolution")
else()
	message(STATUS "std::chrono::system_clock does not have nanosecond resolution")
endif()

# discover availability of std::chrono calendar capabilities
if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_STD_CHRONO_HAS_CALENDAR
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_calendar_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_STD_CHRONO_HAS_CALENDAR)
		message(STATUS "we have chrono calendar support in std::chrono")
	else()
		message(STATUS "we do not have chrono calendar support in std::chrono - will configure for date lib as replacement")
	endif()
else()
	message(STATUS "no chrono calendar support in std::chrono before C++20 - will configure for date lib as replacement")
endif()

if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_STD_CHRONO_HAS_LOCAL_T
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_local_t_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_STD_CHRONO_HAS_LOCAL_T)
		message(STATUS "we have chrono local_t support in std::chrono")
	else()
		message(STATUS "we do not have chrono local_t support in std::chrono - will configure for date lib as replacement")
	endif()
else()
	message(STATUS "no chrono local_t support in std::chrono before C++20 - will configure for date lib as replacement")
endif()

if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_STD_CHRONO_HAS_TIMEZONE
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_timezone_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_STD_CHRONO_HAS_TIMEZONE)
		message(STATUS "we have chrono timezone support in std::chrono")
	else()
		message(STATUS "we do not have chrono timezone support in std::chrono - will configure for date-tz lib as replacement")
	endif()
else()
	message(STATUS "no chrono timezone support in std::chrono before C++20 - will configure for date-tz lib as replacement")
endif()

if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_HAS_MONTH_AND_YEAR_LITERALS
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_year_and_month_literals_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_HAS_MONTH_AND_YEAR_LITERALS)
		message(STATUS "we have year and month literals support in std::chrono")
	else()
		message(STATUS "we do not have year and month literals support in std::chrono")
	endif()
else()
	message(STATUS "no year and month literals support in std::chrono before C++20")
endif()

if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_HAS_STD_TIME_STREAM_OPS
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_stream_sys_time_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_HAS_STD_TIME_STREAM_OPS)
		message(STATUS "we have sys_time ostream support in std::chrono")
	else()
		message(STATUS "we do not have sys_time ostream support in std::chrono - will configure for date lib as replacement")
	endif()
else()
	message(STATUS "no sys_time ostream support in std::chrono before C++20 - will configure for date lib as replacement")
endif()

if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_HAS_U8STRING
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/u8string_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_HAS_U8STRING)
		message(STATUS "we have std::u8string")
	else()
		message(STATUS "we do not have std::u8string")
	endif()
else()
	message(STATUS "no std::u8string support before C++20")
endif()

# test if libatomic is needed
try_compile(DEKAF2_NO_NEED_FOR_LIBATOMIC
			"${DEKAF2_GENERATED_FILES_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/libatomic_test.cpp")

if (NOT DEKAF2_NO_NEED_FOR_LIBATOMIC)
	set(DEKAF2_ATOMIC_LIB atomic)
	message(STATUS "we need libatomic")
endif()

if (DEKAF2_USE_SYSTEM_FMTLIB)
	set(DMESSAGE_FMT "a system installed")
else()
	set(DEKAF2_FMT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/libs/fmt-10.0.0/include")
	set(DMESSAGE_FMT "our own")
endif()

# check if we have a working std::format implementation
if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_HAS_STD_FORMAT
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/std_format_test.cpp")
endif()
if (DEKAF2_HAS_STD_FORMAT)
	if(NOT DEKAF2_FORCE_FMTLIB_OVER_STD_FORMAT)
		message(STATUS "we have std::format (and will use it)")
		set(DEKAF2_FMT_INCLUDE "")
	else()
		set(DEKAF2_HAS_STD_FORMAT "")
		message(STATUS "we have std::format, but will use ${DMESSAGE_FMT} fmtlib build as replacement")
	endif()
else()
	message(STATUS "we do not have std::format - will use ${DMESSAGE_FMT} fmtlib build as replacement")
endif()

# check if we have a constexpr std::string implementation
if (CMAKE_CXX_STANDARD GREATER 17)
	try_compile(DEKAF2_HAS_CONSTEXPR_STD_STRING
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/constexpr_string_test.cpp")
endif()
if (DEKAF2_HAS_CONSTEXPR_STD_STRING)
	message(STATUS "we have constexpr std::string")
else()
	message(STATUS "we do not have constexpr std::string")
endif()

if (DEKAF2_HAS_NGHTTP2)
	# our code requires nghttp2_ssize, and the 2 prefix on a few nghttp2 callbacks
	try_compile(DEKAF2_HAS_NGHTTP2
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/nghttp2_test.cpp"
				CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${NGHTTP2_INCLUDE_DIR}")

	if (NOT DEKAF2_HAS_NGHTTP2)
		message(STATUS "dropped NGHTTP2 as it has an antique API")
	endif()
endif()

if (DEKAF2_HAS_MYSQL)
	if (DEKAF2_MYSQL_IS_MARIADB)
		set(SQL_TEST_NAME mariadb)
	else()
		set(SQL_TEST_NAME mysql)
	endif()

	try_compile(DEKAF2_MYSQL_HAS_TLS
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${SQL_TEST_NAME}_has_tls.cpp"
				CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${MYSQL_INCLUDE_DIRS}")

	if (DEKAF2_MYSQL_HAS_TLS)
		message(STATUS "we have TLS support in MySQL")
	else()
		message(STATUS "no TLS support in MySQL")
	endif()

	try_compile(DEKAF2_MYSQL_HAS_ZSTD
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${SQL_TEST_NAME}_has_zstd.cpp"
				CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${MYSQL_INCLUDE_DIRS}")

	if (DEKAF2_MYSQL_HAS_ZSTD)
		message(STATUS "we have ZSTD support in MySQL")
	else()
		message(STATUS "no ZSTD support in MySQL")
	endif()
endif()

# =============================== GENERATED FILES =================================

if (DEKAF2_WITH_USER_AGENT_PARSER)
	# need this setting before creating the configuration header
	set(DEKAF2_HAS_USER_AGENT_PARSER 1)
endif()

# create the configuration header file
configure_file("bits/kconfiguration.h.in" "${DEKAF2_GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)

# create the cmake config file for find_package()
configure_file("cmake/${PROJECT_NAME}-config.cmake.in" "${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-config.cmake" @ONLY)

# create the cmake version comparison file for find_package()
configure_file("cmake/${PROJECT_NAME}-config-version.cmake.in" "${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-config-version.cmake" @ONLY)

# create the cmake version setup file for find_package()
configure_file("cmake/${PROJECT_NAME}-setup.cmake.in" "${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake" @ONLY)

# ======================== COMPILER AND LINKER OPTIONS ============================

# eat our own dog food
set (DEKAF2_INHERIT_BUILD_FLAGS ON)
include ("${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake")

# ========================= THIRD PARTY SOURCE LIBS ============================

set (BACKUP_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
if ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithLabels")
	# resetting build type to Release for third party libraries
	set (CMAKE_BUILD_TYPE "Release")
endif()

# add third party libraries included in source control
add_subdirectory(libs)

if (DEKAF2_WITH_USER_AGENT_PARSER)
	if (CMAKE_GENERATOR MATCHES "Xcode")
		set(UAP-CPP_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/build/uap-cpp-objects.build/Debug/libuap-cpp-objects.a")
	else()
		set(UAP-CPP_LIBRARIES "uap-cpp-static")
	endif()
	message(STATUS "built with user agent parser")
endif()

# add partial third party libraries included in source control
add_subdirectory(from)

set (CMAKE_BUILD_TYPE "${BACKUP_BUILD_TYPE}")

# =============================== HEADER FILES ====================================

set (BITS_HEADERS
	# KEEP ALPHABETIZED
	bits/kasio.h
	bits/kasiostream.h
	bits/kbaseshell.h
	bits/kbasepipe.h
	bits/kdigest.h
	bits/kfilesystem.h
	bits/kfindsetofchars.h
	bits/kformat.h
	bits/khash.h
	bits/kiostreams_filters.h
	bits/klogwriter.h
	bits/klogserializer.h
	bits/kmake_unique.h
	bits/kmutable_pair.h
	bits/kstring_view.h
	bits/kstringviewz.h
	bits/ktarheader.h
	bits/kunique_deleter.h
	bits/kurldualencode.h
)

set(BITS_SIMD_HEADERS
	bits/simd/kfindfirstof.h
	bits/simd/kutf.h
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
	kaes.h
	kallocator.h
	karia.h
	kassociative.h
	katomic_object.h
	kawsauth.h
	kbar.h
	kbase64.h
	kbcrypt.h
	kbit.h
	kbitfields.h
	kblockcipher.h
	kbuffer.h
	kbufferedreader.h
	kcache.h
	kcamellia.h
	kcaseless.h
	kcasestring.h
	kcesu8.h
	kcgistream.h
	kchacha.h
	kchildprocess.h
	kchunkedtransfer.h
	kcompatibility.h
	kcompression.h
	kcookie.h
	kcountingstreambuf.h
	kcrashexit.h
	kcrc.h
	kcsv.h
	kctype.h
	kdataprovider.h
	kdataconsumer.h
	kdate.h
	kdefinitions.h
	kdiff.h
	kduration.h
	kencode.h
	keraseremove.h
	kerror.h
	kexception.h
	kfdstream.h
	kfileserver.h
	kfilesystem.h
	kformat.h
	kformtable.h
	kfrozen.h
	kgetruntimestack.h
	khash.h
	kheapmon.h
	khex.h
	khistory.h
	khttp2.h
	khttp3.h
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
	khttp_version.h
	kinpipe.h
	kinshell.h
	kinstringstream.h
	kiostreamsocket.h
	kjoin.h
	kjson.h
	kjson2.h
	klambdastream.h
	klog.h
	kmail.h
	kmakecopyable.h
	kmessagedigest.h
	kmime.h
	kmodifyingstreambuf.h
	kmpsearch.h
	kmru.h
	kngram.h
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
	kquicstream.h
	kquotedprintable.h
	kread.h
	kreader.h
	kreference_proxy.h
	kregex.h
	kreplacer.h
	krest.h
	krestclient.h
	krestroute.h
	krestserver.h
	kron.h
	krow.h
	krsa.h
	krsakey.h
	krsasign.h
	kscopeguard.h
	ksharedmemory.h
	ksharedref.h
	ksharedptr.h
	ksignals.h
	ksmtp.h
	ksnippets.h
	ksourcelocation.h
	ksplit.h
	kstack.h
	kstream.h
	kstreambuf.h
	kstreambufadaptor.h
	kstreamoptions.h
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
	ktemplate.h
	kthreadpool.h
	kthreads.h
	kthreadsafe.h
	ktime.h
	ktimer.h
	ktlscontext.h
	ktlsstream.h
	ktimeseries.h
	kunixstream.h
	kuntar.h
	kurl.h
	kurlencode.h
	kuseragent.h
	kutf.h
	kutf_iterator.h
	kvariant.h
	kversion.h
	kwebclient.h
	kwebobjects.h
	kwebserver.h
	kwebsocket.h
	kwords.h
	kwrite.h
	kwriter.h
	kxml.h
	kxterm.h
)

if (DEKAF2_HAS_LIBZIP)
	list(APPEND PUBLIC_HEADERS
		kzip.h
	)
endif()

set(PRIVATE_HEADERS
	bits/kron_utils.h
)

set(HEADERS
	${BITS_HEADERS}
	${COMPAT_HEADERS}
	${BITS_SIMD_HEADERS}
	${PUBLIC_HEADERS}
	${PRIVATE_HEADERS}
)

if (DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD)
	list(APPEND HEADERS bits/from_boost/iostreams/lzma.hpp)
endif()

list(SORT HEADERS)

set (PRECOMPILED_HEADERS

	# std headers
	<unordered_map>
	<unordered_set>
	<map>
	<memory>
	<mutex>
	<set>
	<type_traits>
	<string>
	<string_view>
	<fstream>
	<iostream>
	<ostream>
	<istream>
	<vector>
	<array>
	<algorithm>
	<functional>
	<thread>
	<utility>
	<chrono>

	# our own headers
	[["kconfiguration.h"]]
	bits/kstring_view.h
	bits/khash.h
	bits/kmutable_pair.h
	bits/kunique_deleter.h
	dekaf2.h
	kassociative.h
	kcompatibility.h
	kctype.h
	kdate.h
	kdefinitions.h
	kduration.h
	kfilesystem.h
	kformat.h
	khex.h
	khtmlentities.h
	kjoin.h
	kjson.h
	kjson2.h
	klog.h
	koutstringstream.h
	kprops.h
	kread.h
	kreader.h
	kreference_proxy.h
	krow.h
	ksplit.h
	kstack.h
	kstream.h
	kstreambuf.h
	kstring.h
	kstringstream.h
	kstringutils.h
	kstringview.h
	ksystem.h
	ktemplate.h
	ktime.h
	kurl.h
	kurlencode.h
	kutf.h
	kwrite.h
	kwriter.h

	# 3rd party libs
	<nlohmann/json.hpp>
	<fmt/format.h>
	<boost/multi_index_container.hpp>
	<boost/multi_index/hashed_index.hpp>
	<boost/multi_index/ordered_index.hpp>
	<boost/multi_index/identity.hpp>
	<boost/multi_index/member.hpp>
	<boost/multi_index/sequenced_index.hpp>
)

# =============================== SOURCE FILES ====================================

set (SOURCES
	# KEEP ALPHABETIZED
	bits/kbasepipe.cpp
	bits/kbaseshell.cpp
	bits/kdigest.cpp
	bits/kfindsetofchars.cpp
	bits/klogwriter.cpp
	bits/klogserializer.cpp
	bits/kstring_view.cpp
	bits/kstringviewz.cpp
	bits/simd/kfindfirstof.cpp
	bits/simd/kutf.cpp
	dekaf2.cpp
	kawsauth.cpp
	kbar.cpp
	kbase64.cpp
	kbcrypt.cpp
	kblockcipher.cpp
	kbufferedreader.cpp
	kcaseless.cpp
	kcasestring.cpp
	kcgistream.cpp
	kchildprocess.cpp
	kchunkedtransfer.cpp
	kcompression.cpp
	kcookie.cpp
	kcountingstreambuf.cpp
	kcrashexit.cpp
	kcrc.cpp
	kcsv.cpp
	kctype.cpp
	kdataprovider.cpp
	kdataconsumer.cpp
	kdate.cpp
	kdiff.cpp
	kduration.cpp
	kerror.cpp
	kfdstream.cpp
	kfileserver.cpp
	kfilesystem.cpp
	kformat.cpp
	kformtable.cpp
	kgetruntimestack.cpp
	khash.cpp
	kheapmon.cpp
	khex.cpp
	khistory.cpp
	khttp2.cpp
	khttp3.cpp
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
	khttp_version.cpp
	kinpipe.cpp
	kinshell.cpp
	kinstringstream.cpp
	kiostreamsocket.cpp
	kjson.cpp
	kjson2.cpp
	klambdastream.cpp
	klog.cpp
	kmail.cpp
	kmessagedigest.cpp
	kmime.cpp
	kmodifyingstreambuf.cpp
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
	kquicstream.cpp
	kquotedprintable.cpp
	kread.cpp
	kreader.cpp
	kregex.cpp
	kreplacer.cpp
	krest.cpp
	krestclient.cpp
	krestroute.cpp
	krestserver.cpp
	kron.cpp
	krow.cpp
	krsa.cpp
	krsakey.cpp
	krsasign.cpp
	ksharedmemory.cpp
	ksignals.cpp
	ksmtp.cpp
	ksnippets.cpp
	ksplit.cpp
	kstream.cpp
	kstreambuf.cpp
	kstreambufadaptor.cpp
	kstreamoptions.cpp
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
	ktlscontext.cpp
	ktlsstream.cpp
	kunixstream.cpp
	kuntar.cpp
	kurl.cpp
	kurlencode.cpp
	kuseragent.cpp
	kversion.cpp
	kwebclient.cpp
	kwebobjects.cpp
	kwebserver.cpp
	kwebsocket.cpp
	kwords.cpp
	kwrite.cpp
	kwriter.cpp
	kxml.cpp
	kxterm.cpp
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

# these are for (k)bcrypt
set (BCRYPT_SOURCES
	libs/libbcrypt/crypt_blowfish.c
	libs/libbcrypt/crypt_gensalt.c
	libs/libbcrypt/wrapper.c
)

# ============================= CONDITIONAL LIBS ==================================

# see note in bits/kfilesystem.h!
#if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
#	set (FSLIB libstdc++fs.a)
# this does currently not work well with mixed gcc/clang systems
#elseif (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]" AND NOT APPLE)
#	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
#		set (FSLIB libc++experimental.a)
#	elseif (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
#		set (FSLIB libc++fs.a)
#	endif()
#endif ()

if (DEKAF2_WITH_FCGI)
	set (FCGILIBS
		fcgi
		fcgilib
	)
endif ()

# =============================== TARGETS ====================================

if (DEKAF2_WITH_PEDANTIC_WARNINGS)
	if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		add_compile_options($<$<CONFIG:DEBUG>:-Wpedantic>)
		check_cxx_compiler_flag(-Wdtor-name DEKAF2_HAS_DTOR_NAME_FLAG)
		if (DEKAF2_HAS_DTOR_NAME_FLAG)
			add_compile_options($<$<CONFIG:DEBUG>:-Wno-dtor-name>)
		endif()
	endif()
endif()

# we have to create the timezone library even when dekaf2 itself has
# std::chrono timezones - the reason is that users may compile with
# C++ < 20, in which case the std headers do not expose the timezone
# functionality.. as a fallback we then provide the tzlib of hhinnant

set(DATE_PUBLIC_HEADERS
	from/hhinnant/date/ios.h
	from/hhinnant/date/date.h
	from/hhinnant/date/tz.h
	from/hhinnant/date/tz_private.h
)

# build the date library with tz support
add_library(date-tz ${DATE_PUBLIC_HEADERS} from/hhinnant/date/tz.cpp)

target_include_directories(date-tz
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/from/hhinnant>
	$<INSTALL_INTERFACE:${dekaf2_include_dest}>
)

if (IS_WINDOWS)
	set(USE_OS_TZDB 0)
else()
	set(USE_OS_TZDB 1)
endif()

target_compile_definitions(date-tz
	PRIVATE
	AUTO_DOWNLOAD=0
	HAS_REMOTE_API=0
	INSTALL=.
	PUBLIC
	USE_OS_TZDB=${USE_OS_TZDB}
)

if (DEKAF2_STD_CHRONO_HAS_LOCAL_T)
	target_compile_definitions(date-tz PUBLIC DEKAF2_STD_CHRONO_HAS_LOCAL_T=1)
endif()
if (DEKAF2_HAS_STD_TIME_STREAM_OPS)
	target_compile_definitions(date-tz PUBLIC DEKAF2_HAS_STD_TIME_STREAM_OPS=1)
endif()

set (COMPRESSORS
	${ZLIB_LIBRARIES}
	${BZIP2_LIBRARIES}
	${LIBLZMA_LIBRARIES}
	${ZSTD_LIBRARIES}
)
if (DEKAF2_PREFER_STATIC_LIBS AND BROTLI_STATIC_FOUND)
	list(APPEND COMPRESSORS ${BROTLI_STATIC_LIBRARIES})
else()
	list(APPEND COMPRESSORS ${BROTLI_LIBRARIES})
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

set(DEKAF2_SOURCE_FILES
	# we add the HEADERS as well, as otherwise cmake based IDEs would not see them
	# as belonging to the project. CMake would not need them here.
	${HEADERS}
	${SOURCES}
)

if (DEKAF2_INSTALL_INCLUDE)
	set (DEKAF2_PUBLIC_INCLUDE_DIRECTORIES
		$<INSTALL_INTERFACE:${dekaf2_include_dest}>
		$<INSTALL_INTERFACE:${dekaf2_lib_dest}/include>
		${Boost_INCLUDE_DIR}
		${OPENSSL_INCLUDE_DIR}
		$<BUILD_INTERFACE:${DEKAF2_GENERATED_FILES_DIR}>
	)
endif()

set(DEKAF2_INCLUDE_DIRECTORIES
	SYSTEM
		# include all external headers (and own installed headers) as
		# SYSTEM headers to suppress warnings on their contents
	PUBLIC
		$<BUILD_INTERFACE:${DEKAF2_FMT_INCLUDE}>
		${DEKAF2_PUBLIC_INCLUDE_DIRECTORIES}
	PRIVATE
		$<BUILD_INTERFACE:${libproc_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${LIBZIP_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${BROTLI_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/from/google>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/croncpp/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/uap-cpp>
		$<BUILD_INTERFACE:${JEMalloc_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${NGHTTP2_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${NGHTTP3_INCLUDE_DIRS}>
)

set(DEKAF2_LINK_LIBRARIES
	PUBLIC
		fmt
		nlohmann-json
		date-tz
		${Boost_LIBRARIES}
		${OPENSSL_LIBRARIES}
		frozen
		Threads::Threads
		${DEKAF2_ATOMIC_LIB}
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
		${UAP-CPP_LIBRARIES}
		${YAML_CPP_LIBRARIES}
		${JEMalloc_LIBRARIES}
		${NGHTTP2_LIBRARIES}
		${NGHTTP3_LIBRARIES}
)

set(DEKAF2_COMPILE_DEFINITIONS
	PRIVATE
		# needed for dynamic libs
		DEKAF2_LIBRARY_BUILD
	PUBLIC
		DEKAF2=1
)

if (CMAKE_GENERATOR MATCHES "Xcode")
	# To successfully link targets, Xcode required at least one
	# real file besides object libraries in each target. We
	# create a dummy file with only one symbol.
	# With version 15.0.0 of AppleClang this seems to be fixed though.
	set (DEKAF2_DUMMYFILE "${DEKAF2_GENERATED_FILES_DIR}/dummy.cpp")
	FILE(WRITE "${DEKAF2_DUMMYFILE}"
		"void dekaf2_dummy_for_xcode() {}\n\n"
	)
endif()

add_library                (dekaf2-objects OBJECT ${DEKAF2_SOURCE_FILES})
# for the internal build, include the headers without SYSTEM property so that we get all warnings
target_include_directories (dekaf2-objects ${DEKAF2_INCLUDE_DIRECTORIES})
target_link_libraries      (dekaf2-objects ${DEKAF2_LINK_LIBRARIES})
target_compile_definitions (dekaf2-objects ${DEKAF2_COMPILE_DEFINITIONS})
if (DEKAF2_HAS_USER_AGENT_PARSER)
	add_dependencies       (dekaf2-objects uap-cpp-static)
endif()
if (DEKAF2_USE_PRECOMPILED_HEADERS)
	target_precompile_headers(dekaf2-objects PRIVATE ${PRECOMPILED_HEADERS})
endif()

# we have to build the bcrypt objects in a separate target, as otherwise the
# precompiled header generation seems to use gcc instead of g++ for the
# C++ files
add_library                (bcrypt-objects OBJECT ${BCRYPT_SOURCES})

if (DEKAF2_BUILD_SHARED_DEKAF2)
	set_target_properties      (dekaf2-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
	add_library                (dekaf2-shared  SHARED $<TARGET_OBJECTS:dekaf2-objects> $<TARGET_OBJECTS:bcrypt-objects> ${DEKAF2_DUMMYFILE})
	target_include_directories (dekaf2-shared  ${DEKAF2_INCLUDE_DIRECTORIES})
	target_link_libraries      (dekaf2-shared  ${DEKAF2_LINK_LIBRARIES})
	target_compile_definitions (dekaf2-shared  ${DEKAF2_COMPILE_DEFINITIONS})
	set_target_properties      (dekaf2-shared  PROPERTIES C_VISIBILITY_PRESET   hidden)
	set_target_properties      (dekaf2-shared  PROPERTIES CXX_VISIBILITY_PRESET hidden)
	set_target_properties      (dekaf2-shared  PROPERTIES OUTPUT_NAME "dekaf2")
	set_target_properties      (dekaf2-shared  PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR} VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
endif()

if (DEKAF2_BUILD_STATIC_DEKAF2)
	add_library                (dekaf2 STATIC $<TARGET_OBJECTS:dekaf2-objects> $<TARGET_OBJECTS:bcrypt-objects> ${DEKAF2_DUMMYFILE})
	target_include_directories (dekaf2 ${DEKAF2_INCLUDE_DIRECTORIES})
	target_link_libraries      (dekaf2 ${DEKAF2_LINK_LIBRARIES})
	target_compile_definitions (dekaf2 ${DEKAF2_COMPILE_DEFINITIONS})
endif()

set(KSQL2_INCLUDE_DIRECTORIES
	PRIVATE
		$<BUILD_INTERFACE:${MYSQL_INCLUDE_DIRS}>
		$<BUILD_INTERFACE:${FREETDS_INCLUDE_DIRS}>
)

set(KSQL2_LINK_LIBRARIES
	PRIVATE
		${MYSQL_LIBRARIES}
		${FREETDS_LIBRARIES}
)

if (unofficial-libmariadb_FOUND)
	set(KSQL2_LINK_LIBRARIES
		${KSQL2_LINK_LIBRARIES}
		unofficial::libmariadb
	)
endif()

set(KSQL2_COMPILE_DEFINITIONS
	PRIVATE
		DEKAF2_LIBRARY_BUILD
)

add_library                   (ksql2-objects OBJECT ksql.cpp ksql.h bits/ksql_dbc.cpp bits/ksql_dbc.h)
target_include_directories    (ksql2-objects PUBLIC ${KSQL2_INCLUDE_DIRECTORIES})
target_link_libraries         (ksql2-objects ${KSQL2_LINK_LIBRARIES} dekaf2)
target_compile_definitions    (ksql2-objects ${KSQL2_COMPILE_DEFINITIONS})
if (DEKAF2_USE_PRECOMPILED_HEADERS)
	target_precompile_headers (ksql2-objects REUSE_FROM dekaf2-objects)
endif()

if (DEKAF2_BUILD_SHARED_DEKAF2)
	set_target_properties      (ksql2-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
	add_library                (ksql2-shared  SHARED $<TARGET_OBJECTS:ksql2-objects> ${DEKAF2_DUMMYFILE})
	target_include_directories (ksql2-shared  ${KSQL2_INCLUDE_DIRECTORIES})
	target_link_libraries      (ksql2-shared  ${KSQL2_LINK_LIBRARIES} dekaf2-shared)
	target_compile_definitions (ksql2-shared  ${KSQL2_COMPILE_DEFINITIONS})
	set_target_properties      (ksql2-shared  PROPERTIES C_VISIBILITY_PRESET   hidden)
	set_target_properties      (ksql2-shared  PROPERTIES CXX_VISIBILITY_PRESET hidden)
	set_target_properties      (ksql2-shared  PROPERTIES OUTPUT_NAME "ksql2")
	set_target_properties      (ksql2-shared  PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR} VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
endif()

if (DEKAF2_BUILD_STATIC_DEKAF2)
	add_library                (ksql2 STATIC $<TARGET_OBJECTS:ksql2-objects> ${DEKAF2_DUMMYFILE})
	target_include_directories (ksql2 ${KSQL2_INCLUDE_DIRECTORIES})
	target_link_libraries      (ksql2 ${KSQL2_LINK_LIBRARIES} dekaf2)
	target_compile_definitions (ksql2 ${KSQL2_COMPILE_DEFINITIONS})
endif()

if (SQLITE3_FOUND)
	if (DEKAF2_BUILD_SHARED_DEKAF2)
		add_library(ksqlite-shared SHARED ksqlite.cpp ksqlite.h)
		set_target_properties      (ksqlite-shared PROPERTIES POSITION_INDEPENDENT_CODE ON)
		target_include_directories (ksqlite-shared PRIVATE $<BUILD_INTERFACE:${SQLITE3_INCLUDE_DIRS}>)
		target_link_libraries      (ksqlite-shared
			PRIVATE
				dekaf2-shared
				${SQLITE3_LIBRARIES}
			)
		# DEKAF2_LIBRARY_BUILD is needed for dynamic libs
		target_compile_definitions (ksqlite-shared PRIVATE DEKAF2_LIBRARY_BUILD)
		set_target_properties      (ksqlite-shared PROPERTIES C_VISIBILITY_PRESET   hidden)
		set_target_properties      (ksqlite-shared PROPERTIES CXX_VISIBILITY_PRESET hidden)
		set_target_properties      (ksqlite-shared PROPERTIES OUTPUT_NAME "ksqlite")
		set_target_properties      (ksqlite-shared PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR} VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

		if (DEKAF2_USE_PRECOMPILED_HEADERS)
			target_precompile_headers(ksqlite-shared REUSE_FROM dekaf2-objects)
		endif()
	endif() # DEKAF2_BUILD_SHARED_DEKAF2

	if (DEKAF2_BUILD_STATIC_DEKAF2)
		add_library(ksqlite STATIC ksqlite.cpp ksqlite.h)
		target_include_directories (ksqlite PRIVATE $<BUILD_INTERFACE:${SQLITE3_INCLUDE_DIRS}>)
		target_link_libraries(ksqlite
			PRIVATE
				dekaf2
				${SQLITE3_LIBRARIES}
			)
		# DEKAF2_LIBRARY_BUILD is needed for dynamic libs
		target_compile_definitions(ksqlite PRIVATE DEKAF2_LIBRARY_BUILD)

		if (DEKAF2_USE_PRECOMPILED_HEADERS)
			target_precompile_headers(ksqlite REUSE_FROM dekaf2-objects)
		endif()
	endif() # DEKAF2_BUILD_STATIC_DEKAF2
endif() # SQLITE3_FOUND

if (DEKAF2_INSTALL_BIN)

	add_executable(klog klog_main.cpp)
	target_link_libraries(klog dekaf2${DEKAF2_LINK_SHARED})

	add_executable(createdbc createdbc.cpp)
	target_link_libraries(createdbc ksql2${DEKAF2_LINK_SHARED} dekaf2${DEKAF2_LINK_SHARED})

	if (DEKAF2_HAS_NCURSES)
		add_executable(unixmenu unixmenu.cpp)
		target_include_directories(unixmenu PUBLIC CURSES_INCLUDE_DIRS)
		target_link_libraries(unixmenu dekaf2${DEKAF2_LINK_SHARED} ncurses)
	endif()

	add_executable(dekaf2project dekaf2project.cpp)
	target_link_libraries(dekaf2project dekaf2${DEKAF2_LINK_SHARED})

	add_executable(kurl samples/kurl.cpp samples/kurl.h)
	target_link_libraries(kurl dekaf2${DEKAF2_LINK_SHARED})

	add_executable(khttp samples/khttp.cpp)
	target_link_libraries(khttp dekaf2${DEKAF2_LINK_SHARED})

	add_executable(kgrep samples/kgrep.h samples/kgrep.cpp)
	target_link_libraries(kgrep dekaf2${DEKAF2_LINK_SHARED})

	add_executable(kreplace samples/kreplace.h samples/kreplace.cpp)
	target_link_libraries(kreplace dekaf2${DEKAF2_LINK_SHARED})

	add_executable(ksql samples/ksql.h samples/ksql.cpp)
	target_link_libraries(ksql ksql2${DEKAF2_LINK_SHARED} dekaf2${DEKAF2_LINK_SHARED})

	add_executable(krypt samples/krypt.cpp)
	target_link_libraries(krypt dekaf2${DEKAF2_LINK_SHARED})

	add_executable(statinfo samples/statinfo.cpp)
	target_link_libraries(statinfo dekaf2${DEKAF2_LINK_SHARED})

	add_executable(getentities EXCLUDE_FROM_ALL getentities.cpp)
	target_link_libraries(getentities dekaf2${DEKAF2_LINK_SHARED})

	add_executable(genunicodetables EXCLUDE_FROM_ALL genunicodetables.cpp)
	target_link_libraries(genunicodetables dekaf2${DEKAF2_LINK_SHARED})

	add_executable(diff-speedtest EXCLUDE_FROM_ALL from/google/diff-speedtest.cpp)
	target_link_libraries(diff-speedtest dekaf2${DEKAF2_LINK_SHARED})

endif()

# ============================ UNIT AND SMOKE TESTS ==================================

add_subdirectory(utests)
add_subdirectory(smoketests)
add_subdirectory(benchmarks)

# ======================== CLEANUP BEFORE INSTALLATION ===============================

# check if we have to remove previously installed directories because we changed between
# mono- and dual-target install, or version and version-less install

# version vs. version-less:
if (DEKAF2_VERSION_IN_TARGET)
	if (DEKAF2_INSTALL_INCLUDE)
		string(REGEX REPLACE "-${lib_version_regex}" "" removedir "${dekaf2_include_dest}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
	if (DEKAF2_INSTALL_LIB)
		string(REGEX REPLACE "-${lib_version_regex}" "" removedir "${dekaf2_main_lib_dest}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
	if (DEKAF2_INSTALL_SHARE)
		string(REGEX REPLACE "-${lib_version_regex}" "" removedir "${dekaf2_share_dest}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
else()
	if (DEKAF2_INSTALL_INCLUDE)
		set(removedir "${dekaf2_include_dest}-${lib_version}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
	if (DEKAF2_INSTALL_LIB)
		set(removedir "${dekaf2_main_lib_dest}-${lib_version}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
	if (DEKAF2_INSTALL_SHARE)
		set(removedir "${dekaf2_share_dest}-${lib_version}")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${removedir})")
	endif()
endif()

if (DEKAF2_INSTALL_LIB)

	# mono vs. dual:
	if (DEKAF2_DUAL_TARGET)
		# remove cmake files of mono install:
		file(GLOB GLOBBED_FILES LIST_DIRECTORIES false "${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/cmake/*")
		# list remove_item is buggy on cmake < 3.20 - skip it
		if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.20.0")
			# do not remove the main config files - they will be overwritten if necessary
			if (GLOBBED_FILES)
				list(REMOVE_ITEM "${GLOBBED_FILES}" "${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/cmake/${PROJECT_NAME}-config.cmake")
			endif()
			if (GLOBBED_FILES)
				list(REMOVE_ITEM "${GLOBBED_FILES}" "${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/cmake/${PROJECT_NAME}-config-version.cmake")
			endif()
		endif()
		if (GLOBBED_FILES)
			install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${GLOBBED_FILES})")
		endif()
		# remove libs of mono install:
		file(GLOB GLOBBED_FILES LIST_DIRECTORIES false "${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/*")
		if(GLOBBED_FILES)
			install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${GLOBBED_FILES})")
		endif()
		# remove include/kconfiguration.h of mono install:
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/include)")
	else()
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/Release)")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${dekaf2_main_lib_dest}/Debug)")
	endif()

	if (NOT DEKAF2_BUILD_SHARED_DEKAF2)
		# remove previously installed shared libs:
		# globbing takes place at cmake generation, deletion at install, so the following
		# is not a guarantee to really remove all old artifacts that are not recreated, but
		# it should work in most cases ..
		if (IS_LINUX)
			set(DYN_MASK "so*")
		elseif (IS_MACOS)
			set(DYN_MASK "*dylib")
		else ()
			set(DYN_MASK "NOTFOUND")
		endif ()
		file(GLOB GLOBBED_FILES "${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libdekaf2.${DYN_MASK}")
		set(DYN_FILES ${GLOBBED_FILES})
		file(GLOB GLOBBED_FILES "${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libksql2.${DYN_MASK}")
		set(DYN_FILES ${DYN_FILES} ${GLOBBED_FILES})
		file(GLOB GLOBBED_FILES "${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libksqlite.${DYN_MASK}")
		set(DYN_FILES ${DYN_FILES} ${GLOBBED_FILES})
		if (DYN_FILES)
			install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${DYN_FILES})")
		endif ()
	endif()

	if (NOT DEKAF2_BUILD_STATIC_DEKAF2)
		# remove previously installed static libs:
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libdekaf2${CMAKE_STATIC_LIBRARY_SUFFIX})")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libdekaf2${CMAKE_STATIC_LIBRARY_SUFFIX})")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libksql2${CMAKE_STATIC_LIBRARY_SUFFIX})")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libsqlite${CMAKE_STATIC_LIBRARY_SUFFIX})")
	endif()

	# remove minifolly, we do not build it anymore since at least 2023, but forgot to remove the lib if it was built, do it until mid-2025
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_lib_dest}/libminifolly.a)")

endif() # DEKAF2_INSTALL_LIB

if (DEKAF2_INSTALL_INCLUDE)

	if (NOT SQLITE3_FOUND)
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/ksqlite.h)")
	endif()

	if (NOT DEKAF2_HAS_LIBZIP)
		# make sure to remove a kzip.h header if this build did not find libzip
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/kzip.h)")
	endif()

	if(DEKAF2_USE_SYSTEM_FMTLIB)
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/fmt)")
	endif()

	# remove old headers, until end-2025:
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/kutf8.h)")
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/kutf8iterator.h)")

endif() # DEKAF2_INSTALL_INCLUDE

# ================================= INSTALLATION =====================================

if (DEKAF2_INSTALL_LIB)

	# always put this first, otherwise it risks to remove other already installed cmake config files
	install(EXPORT ${PROJECT_NAME}-targets NAMESPACE dekaf2:: DESTINATION "${dekaf2_lib_dest}/cmake")

	# create dummy exports to satisfy cmake for the statically and PRIVATE linked in libraries
	if(NOT DEKAF2_USE_SYSTEM_FMTLIB)
		install(TARGETS fmt EXPORT ${PROJECT_NAME}-targets DESTINATION ${dekaf2_lib_dest})
	endif()

	install(TARGETS nlohmann-json re2 frozen date-tz EXPORT ${PROJECT_NAME}-targets DESTINATION ${dekaf2_lib_dest})

	if (DEKAF2_HAS_USER_AGENT_PARSER)
		install(TARGETS uap-cpp-static EXPORT ${PROJECT_NAME}-targets               DESTINATION ${dekaf2_lib_dest})
	endif()

	if (DEKAF2_BUILD_SHARED_DEKAF2)
		install(TARGETS dekaf2-shared ksql2-shared EXPORT ${PROJECT_NAME}-targets   DESTINATION ${dekaf2_lib_dest})
		if(SQLITE3_FOUND)
			install(TARGETS ksqlite-shared EXPORT ${PROJECT_NAME}-targets           DESTINATION ${dekaf2_lib_dest})
		endif()
	endif()

	if (DEKAF2_BUILD_STATIC_DEKAF2)
		install(TARGETS dekaf2 ksql2 EXPORT ${PROJECT_NAME}-targets DESTINATION ${dekaf2_lib_dest})
		if(SQLITE3_FOUND)
			install(TARGETS ksqlite EXPORT ${PROJECT_NAME}-targets  DESTINATION ${dekaf2_lib_dest})
		endif()
	endif()

	install(FILES ${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-config.cmake         DESTINATION ${dekaf2_main_lib_dest}/cmake)
	install(FILES ${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION ${dekaf2_main_lib_dest}/cmake)
	install(FILES ${DEKAF2_GENERATED_FILES_DIR}/${PROJECT_NAME}-setup.cmake          DESTINATION ${dekaf2_lib_dest}/cmake)

	# although being an include file, kconfiguration.h belongs to the lib install, as it changes
	# depending on the build type
	install(FILES "${DEKAF2_GENERATED_FILES_DIR}/kconfiguration.h" DESTINATION ${dekaf2_lib_dest}/include)

endif() # DEKAF2_INSTALL_LIB

if (DEKAF2_INSTALL_BIN)

	install(TARGETS klog createdbc dekaf2project kurl khttp kgrep kreplace ksql krypt statinfo DESTINATION ${dekaf2_bin_dest})
	if (DEKAF2_HAS_NCURSES)
		install(TARGETS unixmenu DESTINATION ${dekaf2_bin_dest})
	endif()

	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/mysql-newuser" DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/findcol"       DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/kport"         DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/my-ip-addr"    DESTINATION ${dekaf2_bin_dest})

endif() # DEKAF2_INSTALL_BIN

if (DEKAF2_INSTALL_INCLUDE)

	install(FILES ${PUBLIC_HEADERS}    DESTINATION ${dekaf2_include_dest}/dekaf2)
	install(FILES ksql.h               DESTINATION ${dekaf2_include_dest}/dekaf2)
	install(FILES ${BITS_HEADERS}      DESTINATION ${dekaf2_include_dest}/dekaf2/bits)
	install(FILES ${BITS_SIMD_HEADERS} DESTINATION ${dekaf2_include_dest}/dekaf2/bits/simd)
	install(FILES bits/ksql_dbc.h      DESTINATION ${dekaf2_include_dest}/dekaf2/bits)
	if (SQLITE3_FOUND)
		install(FILES ksqlite.h        DESTINATION ${dekaf2_include_dest}/dekaf2)
	endif()

	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/from/nlohmann/include/nlohmann/"  DESTINATION "${dekaf2_include_dest}/nlohmann" FILES_MATCHING PATTERN "*.hpp")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs/frozen/include/frozen/"      DESTINATION "${dekaf2_include_dest}/frozen"   FILES_MATCHING PATTERN "*.h")
	if (NOT DEKAF2_USE_SYSTEM_FMTLIB)
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/libs/fmt-10.0.0/include/fmt/" DESTINATION "${dekaf2_include_dest}/fmt"      FILES_MATCHING PATTERN "*.h")
	endif()
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/from/hhinnant/date/"              DESTINATION "${dekaf2_include_dest}/date"     FILES_MATCHING PATTERN "*.h")

endif() # DEKAF2_INSTALL_INCLUDE

if (DEKAF2_INSTALL_SHARE)

	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/templates"                        DESTINATION "${dekaf2_share_dest}")
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/uap-cpp/uap-core/regexes.yaml"   DESTINATION "${dekaf2_share_dest}/uap-core")

	# remove old shared, until mid-2026:
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}/uap-core/LICENSE)"          )
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}/uap-core/package.json)"     )
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}/uap-core/update.sh)"        )
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}/uap-core/regexes.diff)"     )
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_share_dest}/uap-core/regexes.yaml.last)")

endif() # DEKAF2_INSTALL_SHARE

# ============================= PREPARE PACKAGING ====================================

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
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/khttp"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/kgrep"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/kreplace"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/ksql"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/statinfo"
)

# =================================== THE END ========================================
