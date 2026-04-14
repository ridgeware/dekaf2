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
# Starting with cmake 4.0,  the minimal supported cmake version is 3.5
#
message(STATUS "======================== starting cmake configuration ========================")

if(CMAKE_VERSION VERSION_LESS "3.27.0")
	cmake_minimum_required(VERSION 2.8.12)
else()
	cmake_minimum_required(VERSION 3.13...4.0)
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
if (WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.15.0)
	# do not set MSVC runtime lib in CMAKE_CXX_FLAGS_XXX, but use 
	# CMAKE_MSVC_RUNTIME_LIBRARY settings
	cmake_policy(SET CMP0091 NEW)
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.27.0)
	# use case-preserved package root variable names (Boost_ROOT instead of BOOST_ROOT)
	cmake_policy(SET CMP0144 NEW)
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.30.0)
	# use boosts own BoostConfig.cmake
	cmake_policy(SET CMP0167 OLD)
endif()

if (WIN32)
	if (NOT VCPKG_TARGET_TRIPLET)
		# make sure vcpkg picks static libs
		set(VCPKG_TARGET_TRIPLET "x64-windows-static")
	endif()
endif()

# declare the project
project(dekaf2 VERSION 2.1.10 DESCRIPTION "C++14/17/20/23 rapid application development framework")

# force cpack to pick version numbers from this project, and not from a parent
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

include(ExternalProject)

# don't be verbose
set(CMAKE_REQUIRED_QUIET TRUE)

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
OPTION(DEKAF2_FORCE_STATIC_BUILD_ON_WINDOWS "force linking with static libs including system libs on Windows" ON)
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
OPTION(DEKAF2_INSTALL_FLAT_HEADERS "install flat backward-compatible header stubs" ON)
OPTION(DEKAF2_ALL_LIBS "bind to all found libs (switch off and select single libs for better binary portability)" ON)
OPTION(DEKAF2_WITH_SIMDUTF "build with simd utf implementations" ON)
OPTION(DEKAF2_WITH_USER_AGENT_PARSER "build with user agent parser" ON)

# =================================== C++ STANDARD =================================

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
	set(DEKAF2_CXX_STANDARD 23)
else()
	# cmake < 3.20 does not know about C++23
	set(DEKAF2_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_STANDARD ${DEKAF2_CXX_STANDARD})
#set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)

# we do not force C++23 if not available, cmake will pick the highest available
# dekaf2 supports 14/17/20/23, and also 11 for mostly all code, but uses smarter options
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

endif()

message(STATUS "CMake build type: ${CMAKE_BUILD_TYPE}")

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
	# suppress ranlib "has no symbols" warning for object files that compile to empty
	# ARCHIVE_FINISH handles the build step, the wrapper script handles install
	set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols <TARGET>")
	set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols <TARGET>")
	file(WRITE "${CMAKE_BINARY_DIR}/ranlib_wrapper.sh" "#!/bin/sh\nexec /usr/bin/ranlib -no_warning_for_no_symbols \"$@\"\n")
	file(CHMOD "${CMAKE_BINARY_DIR}/ranlib_wrapper.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
	set(CMAKE_RANLIB "${CMAKE_BINARY_DIR}/ranlib_wrapper.sh")
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
	set (DEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS OFF)
	# we check for unix style local sockets on Windows 10 or later once we know the boost version
	# disable wrapped json, it does not build properly on Windows
	if (DEKAF2_FORCE_STATIC_BUILD_ON_WINDOWS)
		set (DEKAF2_FORCE_STATIC_LIBS ON)
	endif()
	if (DEKAF2_PREFER_STATIC_LIBS OR DEKAF2_FORCE_STATIC_LIBS)
		message (STATUS "linking against static runtime for windows")
		if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15.0)
			# link against static runtime for windows
			set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		else()
			# this will force warnings about both /MD and /MT set, but the latter wins
			add_compile_options($<$<CONFIG:DEBUG>:/MTd>)
			add_compile_options($<$<CONFIG:RELEASE>:/MT>)
		endif()
	else()
		message (STATUS "linking against dynamic runtime for windows")
	endif()
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
	if (CMAKE_DISABLE_PRECOMPILE_HEADERS)
		message(STATUS "precompiled headers disabled by CMAKE_DISABLE_PRECOMPILE_HEADERS")
		set(DEKAF2_USE_PRECOMPILED_HEADERS OFF)
	elseif (CMAKE_VERSION VERSION_LESS "3.16.0")
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

if (CMAKE_COMPILER_IS_GNUCXX AND (CMAKE_CXX_STANDARD LESS 17 OR CMAKE_CXX_COMPILER_VERSION LESS 6))
	if (DEKAF2_WRAPPED_KJSON)
		message(STATUS "cannot use wrapped KJSON type with GCC and C++ < 17 or GCC < 7 - switching off")
		set(DEKAF2_WRAPPED_KJSON OFF)
	endif()
endif()

if (NOT DEKAF2_BUILD_STATIC_DEKAF2 AND NOT DEKAF2_BUILD_SHARED_DEKAF2)
	message(FATAL "do not switch both shared and static options off")
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

if (DEKAF2_LINK_TIME_OPTIMIZATION AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	message(STATUS "requesting link time optimization")
endif()

# on macOS, add Homebrew prefixes for keg-only packages to CMAKE_PREFIX_PATH
# so that find_package() can locate them without manual -D flags
if (APPLE)
	execute_process(
		COMMAND brew --prefix
		OUTPUT_VARIABLE HOMEBREW_PREFIX
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
		RESULT_VARIABLE HOMEBREW_RESULT
	)
	if (HOMEBREW_RESULT EQUAL 0 AND EXISTS "${HOMEBREW_PREFIX}")
		# list of keg-only formulae that dekaf2 may need
		set(DEKAF2_HOMEBREW_KEG_PACKAGES libpq)
		foreach(pkg ${DEKAF2_HOMEBREW_KEG_PACKAGES})
			set(_pkg_prefix "${HOMEBREW_PREFIX}/opt/${pkg}")
			if (EXISTS "${_pkg_prefix}")
				list(APPEND CMAKE_PREFIX_PATH "${_pkg_prefix}")
			endif()
		endforeach()
		unset(_pkg_prefix)
	endif()
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
		set(DEKAF2_HAS_FREETDS 1)
		set(DEKAF2_HAS_CTLIB 1)
	else()
		message(STATUS "No MS SQL support. Install FreeTDS package if needed")
	endif()
endif()

# =========================== Prefer static libs? =============================

# now prefer static libs if requested - we push both .a and .lib at the front
# (MacOS knows .lib as well, not only Windows)

if (DEKAF2_PREFER_STATIC_LIBS OR DEKAF2_FORCE_STATIC_LIBS)
	# make a backup of the original list
	set(LIBSFX_BACKUP ${CMAKE_FIND_LIBRARY_SUFFIXES})
	# push .lib to front
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".lib" LIBSFX_FOUND)
	if (LIBSFX_FOUND GREATER 0)
		list(REMOVE_AT CMAKE_FIND_LIBRARY_SUFFIXES ${LIBSFX_FOUND})
		list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 ".lib")
	endif()
	# push .a to front
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".a" LIBSFX_FOUND)
	if (LIBSFX_FOUND GREATER 0)
		list(REMOVE_AT CMAKE_FIND_LIBRARY_SUFFIXES ${LIBSFX_FOUND})
		list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a")
	endif()
	message(STATUS "prefering static libraries: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
endif()

# ================================= Threads ===================================

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED QUIET)

# ============================== ZLIB / BZIP2 =================================

find_package(ZLIB REQUIRED QUIET)
find_package(BZip2 REQUIRED QUIET)

# =========================== Force static libs? ==============================

# now force static libs if requested

if (DEKAF2_FORCE_STATIC_LIBS)
	# .dll.lib is preset in cmake - no idea who uses that
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.lib" DLLLIB_SFX_FOUND)
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".lib" LIB_SFX_FOUND)
	list(FIND CMAKE_FIND_LIBRARY_SUFFIXES ".a" A_SFX_FOUND)
	set(CMAKE_FIND_LIBRARY_SUFFIXES "")
	if (A_SFX_FOUND GREATER_EQUAL 0)
		list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	endif()
	if (LIB_SFX_FOUND GREATER_EQUAL 0)
		list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
	endif()
	if (DLLLIB_SFX_FOUND GREATER_EQUAL 0)
		list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.lib")
	endif()
	message(STATUS "forcing static libraries: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
endif()

# ================================ OPENSSL ===================================

# always required
find_package(OpenSSL REQUIRED QUIET)
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
	endif()
endif()

# ================================ YAML-CPP ====================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_YAML)
	find_package(yaml-cpp QUIET)
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
if (APPLE)
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
		endif()
		set(DEKAF2_MYSQL_IS_MARIADB ${MYSQL_IS_MARIADB})
	else()
		find_package(unofficial-libmariadb QUIET)
		if (unofficial-libmariadb_FOUND)
			set(DEKAF2_MYSQL_IS_MARIADB 1)
			set(DEKAF2_HAS_MYSQL 1)
		endif()
	endif()
endif()

# ================================ SQLITE ====================================

# this one is not for dekaf2 but for single target ksqlite
# SQLite3 (for KSQL and KSQLite) - if not available it's simply skipped
find_package(SQLite3 QUIET)
if(SQLITE3_FOUND)
	set(DEKAF2_HAS_SQLITE3 1)
endif()

# ================================ POSTGRESQL =================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_POSTGRESQL)
	find_package(PostgreSQL QUIET)
	if(PostgreSQL_FOUND)
		set(DEKAF2_HAS_POSTGRESQL "${PostgreSQL_VERSION_STRING}")
		if(DEKAF2_HAS_POSTGRESQL STREQUAL "")
			set(DEKAF2_HAS_POSTGRESQL 1)
		endif()
	endif()
endif()

# ================================ CURSES ====================================

# this one is not for dekaf2 but for single target unixmenu
# - search for it unconditionally
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses QUIET)
if(CURSES_FOUND)
	set(DEKAF2_HAS_NCURSES 1)
	if(EXISTS "${CURSES_INCLUDE_DIR}/curses.h")
		# extract the version number
		file(STRINGS "${CURSES_INCLUDE_DIR}/curses.h" CURSES_HEADER_CONTENTS REGEX "#define NCURSES_VERSION +\"[0-9\.]+\"")
		string(REGEX REPLACE ".*#define NCURSES_VERSION +\"([0-9\.]+)\".*" "\\1" CURSES_VERSION "${CURSES_HEADER_CONTENTS}")
		unset(CURSES_HEADER_CONTENTS)
	endif()
endif()

# =============================== NGHTTP2/3 ==================================

if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_NGHTTP2)
	find_package(NGHTTP2 QUIET)
	if (NGHTTP2_FOUND)
		set(DEKAF2_HAS_NGHTTP2 1)
	endif()

	if (DEKAF2_HAS_OPENSSL_QUIC)
		if (DEKAF2_ALL_LIBS OR DEKAF2_FIND_NGHTTP3)
			# only search for nghttp3 if we have an openssl version supporting QUIC
			# - this makes some ifdefs shorter..
			find_package(NGHTTP3 QUIET)
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

if (Boost_ROOT)
	message(STATUS "boost root externally set to: ${Boost_ROOT}")
elseif (BOOST_ROOT)
	# accept upper-case BOOST_ROOT from environment or command line
	set(Boost_ROOT "${BOOST_ROOT}")
	message(STATUS "boost root externally set to: ${Boost_ROOT}")
elseif (EXISTS "/opt/boost")
	# check if we installed a boost build in /opt/boost
	set(Boost_ROOT "/opt/boost")
	# force static boost, the source build does not give dynamic libs
	set(Boost_USE_STATIC_LIBS ON)
	message(STATUS "setting boost root to: ${Boost_ROOT}")
endif ()

# boost asio (header only) needs system and date_time
# dekaf2's http part needs iostreams

# starting with boost 1.89 system is no more a component, so remove it here
find_package(Boost 1.54 COMPONENTS iostreams serialization QUIET)

if (Boost_FOUND)
	if (Boost_VERSION VERSION_LESS 1.89.0 OR Boost_VERSION VERSION_GREATER 1000)
		# but if we find that the boost version is < 1.89, or greater 1000 (which
		# represents the old numbering macro back in the days) then repeat the
		# search and include the system component as well - linking on old versions
		# will otherwise fail
		find_package(Boost 1.54 COMPONENTS system iostreams serialization QUIET)
	endif()
endif ()

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
	find_package(Boost 1.54 COMPONENTS system iostreams serialization multi_index QUIET)
endif()

if (NOT Boost_FOUND)
	if (DEKAF2_NO_BUILDSETUP)
		message(FATAL_ERROR "please set Boost_ROOT to the root directory of your boost install")
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

if (false)
	message(STATUS "boost-hdrs: ${Boost_INCLUDE_DIR}")
	message(STATUS "boost-libs: ${Boost_LIBRARIES}")
endif()

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

	find_package(JEMalloc QUIET)
	if (JEMalloc_FOUND)
		set(DEKAF2_HAS_JEMALLOC 1)
	endif()
endif()

# =================================== Doxygen ====================================

# add a target to generate API documentation with Doxygen
# please make sure the graphviz package is installed, too, not only doxygen
find_package(Doxygen QUIET)

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

# ====================== show list of found/notfound libs =======================

function(RightPadString output str padchar length)
	string (LENGTH "${str}" strlen)
	math   (EXPR   strlen "${length} - ${strlen}")
	if (strlen GREATER 0)
		foreach(i RANGE 1 ${strlen})
			string(APPEND pad ${padchar})
		endforeach()
		string(APPEND str ${pad})
	endif()
	set(${output} "${str}" PARENT_SCOPE)
endfunction()

function(LibState State Name Version Lib)
	if (NOT State)
		message(STATUS "  ${Name} not found")
	else()
		if (NOT Lib)
			message(STATUS "  ${Name} found ${Version}")
		else()
			RightPadString(PaddedVersion "${Version}" " " 8)
			if (APPLE)
				string(REPLACE
					"/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/"
					""
					Lib
					"${Lib}"
				)
				string(REPLACE
					"MacOSX.sdk/System/Library/Frameworks/"
					""
					Lib
					"${Lib}"
				)
			endif()
			message(STATUS "  ${Name} found ${PaddedVersion} (${Lib})")
		endif()
	endif()
endfunction()

message(STATUS "")
message(STATUS "required libraries:")
LibState("${Threads_FOUND}"         "Threads  :" ""                    "${Threads_LIBRARIES}" )
LibState("${ZLIB_FOUND}"            "ZLIB     :" "${ZLIB_VERSION}"     "${ZLIB_LIBRARIES}"    )
LibState("${BZip2_FOUND}"           "BZip2    :" "${BZIP2_VERSION}"    "${BZIP2_LIBRARIES}"   )
LibState("${OPENSSL_FOUND}"         "OpenSSL  :" "${OPENSSL_VERSION}"  "${OPENSSL_LIBRARIES}" )
LibState("${Boost_FOUND}"           "Boost    :" "${Boost_VERSION}"    "${Boost_LIBRARIES}"   )
message(STATUS "")

message(STATUS "optional libraries:")
LibState("${DEKAF2_HAS_LIBZIP}"     "ZIP      :" "${LIBZIP_VERSION}"   "${LIBZIP_LIBRARIES}"  )
LibState("${DEKAF2_HAS_LIBLZMA}"    "LZMA     :" "${LIBLZMA_VERSION}"  "${LIBLZMA_LIBRARIES}" )
LibState("${DEKAF2_HAS_LIBZSTD}"    "ZSTD     :" "${ZSTD_VERSION}"     "${ZSTD_LIBRARIES}"    )
LibState("${DEKAF2_HAS_LIBYAML}"    "YAML     :" "${YAML_CPP_VERSION}" "${YAML_CPP_LIBRARIES}")
LibState("${DEKAF2_HAS_MYSQL}"      "MYSQL    :" "${MYSQL_VERSION}"    "${MYSQL_LIBRARIES}"   )
LibState("${DEKAF2_HAS_SQLITE3}"    "SQLITE   :" "${SQLITE3_VERSION}"  "${SQLITE3_LIBRARIES}" )
LibState("${DEKAF2_HAS_POSTGRESQL}" "POSTGRES :" "${PostgreSQL_VERSION_STRING}" "${PostgreSQL_LIBRARY}")
LibState("${DEKAF2_HAS_LIBBROTLI}"  "BROTLI   :" "${BROTLI_VERSION}"   "${BROTLI_LIBRARIES}"  )
LibState("${DEKAF2_HAS_NGHTTP2}"    "NGHTTP2  :" "${NGHTTP2_VERSION}"  "${NGHTTP2_LIBRARIES}" )
LibState("${DEKAF2_HAS_NGHTTP3}"    "NGHTTP3  :" "${NGHTTP3_VERSION}"  "${NGHTTP3_LIBRARIES}" )
LibState("${DEKAF2_HAS_FREETDS}"    "FreeTDS  :" "${FREETDS_VERSION}"  "${FREETDS_LIBRARIES}" )
LibState("${DEKAF2_HAS_JEMALLOC}"   "JEMALLOC :" "${JEMALLOC_VERSION}" "${JEMalloc_LIBRARIES}")
LibState("${DOXYGEN_FOUND}"         "Doxygen  :" "${DOXYGEN_VERSION}"  "${DOXYGEN_EXECUTABLE}")
LibState("${DEKAF2_HAS_NCURSES}"    "NCurses  :" "${CURSES_VERSION}"   "${CURSES_LIBRARIES}"  )
if (APPLE)
	LibState("${libproc_FOUND}"     "LibProc  :" "${libproc_VERSION}"  "${libproc_LIBRARIES}" )
endif()
message(STATUS "")

# =============================== COMPILE TESTS =================================

# create the build directory for generated files
set(DEKAF2_GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${DEKAF2_GENERATED_FILES_DIR}")
if (NOT EXISTS "${DEKAF2_GENERATED_FILES_DIR}/dekaf2")
	# create a symlink to simulate the <dekaf2/> path for includes at library build time
	# (we add DEKAF2_GENERATED_FILES_DIR to the include locations below)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/source" "${DEKAF2_GENERATED_FILES_DIR}/dekaf2"
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

# the structured/ symlink is no longer needed - generated/dekaf2 now points to source/

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
	try_compile(DEKAF2_STD_CHRONO_HAS_CLOCK_CAST
				"${DEKAF2_GENERATED_FILES_DIR}"
				"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/chrono_clock_cast_test.cpp"
				COMPILE_DEFINITIONS "--std=c++20")

	if(DEKAF2_STD_CHRONO_HAS_CLOCK_CAST)
		message(STATUS "we have clock_cast support in std::chrono")
	else()
		message(STATUS "we do not have clock_cast support in std::chrono - will configure for date-tz lib as replacement")
	endif()
else()
	message(STATUS "no clock_cast support in std::chrono before C++20 - will configure for date-tz lib as replacement")
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

# test for arc4random, which appeared only 2022 in glibc
try_compile(DEKAF2_HAS_ARC4RANDOM
			"${DEKAF2_GENERATED_FILES_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/arc4random_test.cpp")

if(DEKAF2_HAS_ARC4RANDOM)
	message(STATUS "we have arc4random")
else()
	message(STATUS "we do not have arc4random")
endif()

# test for getrandom, which is glibc specific
try_compile(DEKAF2_HAS_GETRANDOM
			"${DEKAF2_GENERATED_FILES_DIR}"
			"${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/getrandom_test.cpp")

if(DEKAF2_HAS_GETRANDOM)
	message(STATUS "we have getrandom")
else()
	message(STATUS "we do not have getrandom")
endif()

# =============================== GENERATED FILES =================================

if (DEKAF2_WITH_USER_AGENT_PARSER)
	# need this setting before creating the configuration header
	set(DEKAF2_HAS_USER_AGENT_PARSER 1)
endif()

# create the configuration header file
configure_file("cmake/kconfiguration.h.in" "${DEKAF2_GENERATED_FILES_DIR}/kconfiguration.h" @ONLY)

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

# don't be verbose again..
set(CMAKE_REQUIRED_QUIET TRUE)

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

# don't be verbose again..
set(CMAKE_REQUIRED_QUIET TRUE)

# =============================== HEADER FILES ====================================

set (BITS_HEADERS
	# KEEP ALPHABETIZED BY FULL PATH
	source/containers/associative/bits/kmutable_pair.h
	source/core/format/bits/kformat.h
	source/core/logging/bits/klogserializer.h
	source/core/logging/bits/klogwriter.h
	source/core/strings/bits/kfindsetofchars.h
	source/core/strings/bits/kstring_view.h
	source/core/strings/bits/kstringviewz.h
	source/core/types/bits/kmake_unique.h
	source/core/types/bits/kunique_deleter.h
	source/crypto/hash/bits/kdigest.h
	source/crypto/hash/bits/khash.h
	source/io/compression/bits/kiostreams_filters.h
	source/net/tcp/bits/kasio.h
	source/net/tcp/bits/kasiostream.h
	source/net/udp/bits/kdatagramstreambuf.h
	source/system/filesystem/bits/kfilesystem.h
	source/system/process/bits/kbasepipe.h
	source/system/process/bits/kbaseprocess.h
	source/system/process/bits/kbaseshell.h
	source/util/archive/bits/ktarheader.h
	source/web/url/bits/kurldualencode.h
)

set(BITS_SIMD_HEADERS
	source/core/strings/bits/simd/kfindfirstof.h
	source/core/strings/bits/simd/kutf.h
)

if (DEKAF2_HAS_LIBBROTLI)
	list(APPEND BITS_HEADERS source/io/compression/bits/kbrotli.h)
endif()

if (DEKAF2_HAS_LIBZSTD)
	list(APPEND BITS_HEADERS source/io/compression/bits/kzstd.h)
endif()

set(PUBLIC_HEADERS
	# KEEP ALPHABETIZED BY FULL PATH
	source/containers/associative/kassociative.h
	source/containers/associative/kcache.h
	source/containers/associative/klockmap.h
	source/containers/associative/kmru.h
	source/containers/associative/kprops.h
	source/containers/memory/kpersist.h
	source/containers/memory/kpool.h
	source/containers/memory/ksharedptr.h
	source/containers/memory/ksharedref.h
	source/containers/sequential/kbuffer.h
	source/containers/sequential/khistory.h
	source/containers/sequential/kspan.h
	source/containers/sequential/kstack.h
	source/core/errors/kcrashexit.h
	source/core/errors/kerror.h
	source/core/errors/kexception.h
	source/core/errors/ksourcelocation.h
	source/core/format/kbar.h
	source/core/format/kformat.h
	source/core/format/kformtable.h
	source/core/init/dekaf2.h
	source/core/init/kcompatibility.h
	source/core/init/kdefinitions.h
	source/core/logging/klog.h
	source/core/logging/klogrotate.h
	source/core/strings/kcaseless.h
	source/core/strings/kcasestring.h
	source/core/strings/kcesu8.h
	source/core/strings/kjoin.h
	source/core/strings/kmpsearch.h
	source/core/strings/kregex.h
	source/core/strings/ksplit.h
	source/core/strings/kstring.h
	source/core/strings/kstringutils.h
	source/core/strings/kstringview.h
	source/core/strings/kwords.h
	source/core/types/kbit.h
	source/core/types/kbitfields.h
	source/core/types/kctype.h
	source/core/types/keraseremove.h
	source/core/types/kfrozen.h
	source/core/types/kreference_proxy.h
	source/core/types/kscopeguard.h
	source/core/types/ktemplate.h
	source/core/strings/kutf.h
	source/core/strings/kutf_iterator.h
	source/core/types/kvariant.h
	source/crypto/auth/kawsauth.h
	source/crypto/auth/kbcrypt.h
	source/crypto/auth/kopenid.h
	source/crypto/cipher/kaes.h
	source/crypto/cipher/karia.h
	source/crypto/cipher/kblockcipher.h
	source/crypto/cipher/kcamellia.h
	source/crypto/cipher/kchacha.h
	source/crypto/encoding/kbase64.h
	source/crypto/encoding/kencode.h
	source/crypto/encoding/khex.h
	source/crypto/encoding/kquotedprintable.h
	source/crypto/hash/kcrc.h
	source/crypto/hash/khash.h
	source/crypto/hash/khmac.h
	source/crypto/hash/kmessagedigest.h
	source/crypto/rsa/krsa.h
	source/crypto/rsa/krsacert.h
	source/crypto/rsa/krsakey.h
	source/crypto/rsa/krsasign.h
	source/data/csv/kcsv.h
	source/data/json/kjson.h
	source/data/json/kjson2.h
	source/data/sql/krow.h
	source/data/template/koutputtemplate.h
	source/data/template/kreplacer.h
	source/data/template/ksnippets.h
	source/data/xml/kxml.h
	source/http/client/khttpclient.h
	source/http/client/kwebclient.h
	source/http/cookie/kcookie.h
	source/http/protocol/kchunkedtransfer.h
	source/http/protocol/khttp2.h
	source/http/protocol/khttp3.h
	source/http/protocol/khttp_header.h
	source/http/protocol/khttp_method.h
	source/http/protocol/khttp_request.h
	source/http/protocol/khttp_response.h
	source/http/protocol/khttp_version.h
	source/http/protocol/khttpcompression.h
	source/http/server/kcgistream.h
	source/http/server/khttperror.h
	source/http/server/khttpinputfilter.h
	source/http/server/khttplog.h
	source/http/server/khttpoutputfilter.h
	source/http/server/khttppath.h
	source/http/server/khttprouter.h
	source/http/server/khttpserver.h
	source/http/websocket/kwebsocket.h
	source/http/websocket/kwebsocketclient.h
	source/io/compression/kcompression.h
	source/io/pipes/kdataconsumer.h
	source/io/pipes/kdataprovider.h
	source/io/pipes/kinpipe.h
	source/io/pipes/koutpipe.h
	source/io/pipes/kpipe.h
	source/io/readwrite/kbufferedreader.h
	source/io/readwrite/kread.h
	source/io/readwrite/kreader.h
	source/io/readwrite/kwrite.h
	source/io/readwrite/kwriter.h
	source/io/streams/kcountingstreambuf.h
	source/io/streams/kfdstream.h
	source/io/streams/kinstringstream.h
	source/io/streams/klambdastream.h
	source/io/streams/kmodifyingstreambuf.h
	source/io/streams/koutstringstream.h
	source/io/streams/kstream.h
	source/io/streams/kstreambuf.h
	source/io/streams/kstreambufadaptor.h
	source/io/streams/kstringstream.h
	source/net/address/kipaddress.h
	source/net/address/kipnetwork.h
	source/net/address/knetworkinterface.h
	source/net/address/kresolve.h
	source/net/quic/kquicstream.h
	source/net/udp/kdtlsserver.h
	source/net/udp/kdtlssocket.h
	source/net/udp/kdtlsstream.h
	source/net/udp/kudpserver.h
	source/net/udp/kudpsocket.h
	source/net/udp/kudpstream.h
	source/net/tcp/ktcpclient.h
	source/net/tcp/ktcpserver.h
	source/net/tcp/ktcpstream.h
	source/net/tcp/kunixstream.h
	source/net/tls/ktlscontext.h
	source/net/tls/ktlsstream.h
	source/net/util/kiostreamsocket.h
	source/net/util/kpoll.h
	source/net/util/kstreamoptions.h
	source/net/util/ktunnel.h
	source/rest/framework/krest.h
	source/rest/framework/krestclient.h
	source/rest/framework/krestroute.h
	source/rest/framework/krestserver.h
	source/rest/limits/kconnectionlimiter.h
	source/rest/limits/kratelimiter.h
	source/rest/serving/kfileserver.h
	source/rest/serving/kwebdav.h
	source/rest/serving/kwebserver.h
	source/rest/serving/kwebserverpermissions.h
	source/system/filesystem/kfilesystem.h
	source/system/os/kgetruntimestack.h
	source/system/os/kheapmon.h
	source/system/os/ksignals.h
	source/system/os/ksystem.h
	source/system/os/ksystemstats.h
	source/system/process/kchildprocess.h
	source/system/process/kinshell.h
	source/system/process/koutshell.h
	source/system/process/kpty.h
	source/system/shared/ksharedmemory.h
	source/threading/execution/kparallel.h
	source/threading/execution/kthreadpool.h
	source/threading/execution/kthreads.h
	source/threading/patterns/ksubscribe.h
	source/threading/primitives/katomic_object.h
	source/threading/primitives/kmakecopyable.h
	source/threading/primitives/kthreadsafe.h
	source/time/clock/kdate.h
	source/time/clock/kjuliandate.h
	source/time/clock/ktime.h
	source/time/duration/kduration.h
	source/time/duration/kprof.h
	source/time/duration/ktimer.h
	source/time/scheduler/kron.h
	source/time/series/ktimeseries.h
	source/util/archive/kuntar.h
	source/util/cli/koptions.h
	source/util/cli/kxterm.h
	source/util/id/krandom.h
	source/util/id/kuuid.h
	source/util/mail/kmail.h
	source/util/mail/ksmtp.h
	source/util/misc/kaddrplus.h
	source/util/misc/ktemperature.h
	source/util/misc/kversion.h
	source/util/text/kdiff.h
	source/util/text/kngram.h
	source/web/html/khtmlcontentblocks.h
	source/web/html/khtmldom.h
	source/web/html/khtmlentities.h
	source/web/html/khtmlparser.h
	source/web/objects/kwebobjects.h
	source/web/url/kmime.h
	source/web/url/kurl.h
	source/web/url/kurlencode.h
	source/web/url/kuseragent.h
)

if (DEKAF2_HAS_LIBZIP)
	list(APPEND PUBLIC_HEADERS
		source/util/archive/kzip.h
	)
endif()

set(PRIVATE_HEADERS
	source/time/scheduler/bits/kron_utils.h
)

set(HEADERS
	${BITS_HEADERS}
	${COMPAT_HEADERS}
	${BITS_SIMD_HEADERS}
	${PUBLIC_HEADERS}
	${PRIVATE_HEADERS}
)

if (DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD)
	list(APPEND HEADERS source/io/compression/bits/from_boost/iostreams/lzma.hpp)
endif()

list(SORT HEADERS)

set (PRECOMPILED_HEADERS
	# Standard Library (frequent and/or expensive to parse)
	<algorithm>
	<array>
	<atomic>
	<chrono>
	<cstdint>
	<functional>
	<iostream>
	<map>
	<memory>
	<mutex>
	<string>
	<string_view>
	<type_traits>
	<unordered_map>
	<vector>
	# Boost (very expensive to parse)
	<boost/multi_index_container.hpp>
	<boost/multi_index/hashed_index.hpp>
	<boost/multi_index/member.hpp>
	<boost/multi_index/ordered_index.hpp>
	<boost/multi_index/sequenced_index.hpp>
	# NOTE: do NOT add <boost/iostreams/filter/gzip.hpp> here - it triggers
	# false positive -Warray-bounds/-Wstringop-overflow warnings with GCC 12.
	# It is included through kiostreams_filters.h which has the proper
	# #pragma GCC diagnostic suppression.
	# dekaf2 core headers (frequently included, relatively stable)
	"${DEKAF2_GENERATED_FILES_DIR}/kconfiguration.h"
	<dekaf2/core/init/kdefinitions.h>
	<dekaf2/core/init/kcompatibility.h>
	<dekaf2/core/strings/kstring.h>
	<dekaf2/core/strings/kstringview.h>
	<dekaf2/core/strings/kstringutils.h>
	<dekaf2/core/format/kformat.h>
	<dekaf2/core/logging/klog.h>
	<dekaf2/data/json/kjson.h>
)

# =============================== SOURCE FILES ====================================

set (SOURCES
	# KEEP ALPHABETIZED BY FULL PATH
	source/containers/associative/kprops.cpp
	source/containers/sequential/khistory.cpp
	source/core/errors/kcrashexit.cpp
	source/core/errors/kerror.cpp
	source/core/format/kbar.cpp
	source/core/format/kformat.cpp
	source/core/format/kformtable.cpp
	source/core/init/dekaf2.cpp
	source/core/init/kcompatibility.cpp
	source/core/logging/bits/klogserializer.cpp
	source/core/logging/bits/klogwriter.cpp
	source/core/logging/klog.cpp
	source/core/logging/klogrotate.cpp
	source/core/strings/bits/kfindsetofchars.cpp
	source/core/strings/bits/kstring_view.cpp
	source/core/strings/bits/kstringviewz.cpp
	source/core/strings/bits/simd/kfindfirstof.cpp
	source/core/strings/kcaseless.cpp
	source/core/strings/kcasestring.cpp
	source/core/strings/kmpsearch.cpp
	source/core/strings/kregex.cpp
	source/core/strings/ksplit.cpp
	source/core/strings/kstring.cpp
	source/core/strings/kstringutils.cpp
	source/core/strings/kstringview.cpp
	source/core/strings/kwords.cpp
	source/core/strings/bits/simd/kutf.cpp
	source/core/types/kctype.cpp
	source/crypto/auth/kawsauth.cpp
	source/crypto/auth/kbcrypt.cpp
	source/crypto/auth/kopenid.cpp
	source/crypto/cipher/kblockcipher.cpp
	source/crypto/encoding/kbase64.cpp
	source/crypto/encoding/khex.cpp
	source/crypto/encoding/kquotedprintable.cpp
	source/crypto/hash/bits/kdigest.cpp
	source/crypto/hash/kcrc.cpp
	source/crypto/hash/khash.cpp
	source/crypto/hash/khmac.cpp
	source/crypto/hash/kmessagedigest.cpp
	source/crypto/rsa/krsa.cpp
	source/crypto/rsa/krsacert.cpp
	source/crypto/rsa/krsakey.cpp
	source/crypto/rsa/krsasign.cpp
	source/data/csv/kcsv.cpp
	source/data/json/kjson.cpp
	source/data/json/kjson2.cpp
	source/data/sql/krow.cpp
	source/data/template/koutputtemplate.cpp
	source/data/template/kreplacer.cpp
	source/data/template/ksnippets.cpp
	source/data/xml/kxml.cpp
	source/http/client/khttpclient.cpp
	source/http/client/kwebclient.cpp
	source/http/cookie/kcookie.cpp
	source/http/protocol/kchunkedtransfer.cpp
	source/http/protocol/khttp2.cpp
	source/http/protocol/khttp3.cpp
	source/http/protocol/khttp_header.cpp
	source/http/protocol/khttp_method.cpp
	source/http/protocol/khttp_request.cpp
	source/http/protocol/khttp_response.cpp
	source/http/protocol/khttp_version.cpp
	source/http/protocol/khttpcompression.cpp
	source/http/server/kcgistream.cpp
	source/http/server/khttperror.cpp
	source/http/server/khttpinputfilter.cpp
	source/http/server/khttplog.cpp
	source/http/server/khttpoutputfilter.cpp
	source/http/server/khttppath.cpp
	source/http/server/khttprouter.cpp
	source/http/server/khttpserver.cpp
	source/http/websocket/kwebsocket.cpp
	source/http/websocket/kwebsocketclient.cpp
	source/io/compression/kcompression.cpp
	source/io/pipes/kdataconsumer.cpp
	source/io/pipes/kdataprovider.cpp
	source/io/pipes/kinpipe.cpp
	source/io/pipes/koutpipe.cpp
	source/io/pipes/kpipe.cpp
	source/io/readwrite/kbufferedreader.cpp
	source/io/readwrite/kread.cpp
	source/io/readwrite/kreader.cpp
	source/io/readwrite/kwrite.cpp
	source/io/readwrite/kwriter.cpp
	source/io/streams/kcountingstreambuf.cpp
	source/io/streams/kfdstream.cpp
	source/io/streams/kinstringstream.cpp
	source/io/streams/klambdastream.cpp
	source/io/streams/kmodifyingstreambuf.cpp
	source/io/streams/koutstringstream.cpp
	source/io/streams/kstream.cpp
	source/io/streams/kstreambuf.cpp
	source/io/streams/kstreambufadaptor.cpp
	source/io/streams/kstringstream.cpp
	source/net/address/kipaddress.cpp
	source/net/address/kipnetwork.cpp
	source/net/address/knetworkinterface.cpp
	source/net/address/kresolve.cpp
	source/net/quic/kquicstream.cpp
	source/net/udp/kdtlsserver.cpp
	source/net/udp/kdtlssocket.cpp
	source/net/udp/kdtlsstream.cpp
	source/net/udp/kudpserver.cpp
	source/net/udp/kudpsocket.cpp
	source/net/udp/kudpstream.cpp
	source/net/tcp/ktcpserver.cpp
	source/net/tcp/ktcpstream.cpp
	source/net/tcp/kunixstream.cpp
	source/net/tls/ktlscontext.cpp
	source/net/tls/ktlsstream.cpp
	source/net/util/kiostreamsocket.cpp
	source/net/util/kpoll.cpp
	source/net/util/kstreamoptions.cpp
	source/net/util/ktunnel.cpp
	source/rest/framework/krest.cpp
	source/rest/framework/krestclient.cpp
	source/rest/framework/krestroute.cpp
	source/rest/framework/krestserver.cpp
	source/rest/limits/kconnectionlimiter.cpp
	source/rest/limits/kratelimiter.cpp
	source/rest/serving/kfileserver.cpp
	source/rest/serving/kwebdav.cpp
	source/rest/serving/kwebserver.cpp
	source/rest/serving/kwebserverpermissions.cpp
	source/system/filesystem/kfilesystem.cpp
	source/system/os/kgetruntimestack.cpp
	source/system/os/kheapmon.cpp
	source/system/os/ksignals.cpp
	source/system/os/ksystem.cpp
	source/system/os/ksystemstats.cpp
	source/system/process/bits/kbasepipe.cpp
	source/system/process/bits/kbaseprocess.cpp
	source/system/process/bits/kbaseshell.cpp
	source/system/process/kchildprocess.cpp
	source/system/process/kinshell.cpp
	source/system/process/koutshell.cpp
	source/system/process/kpty.cpp
	source/system/shared/ksharedmemory.cpp
	source/threading/execution/kparallel.cpp
	source/threading/execution/kthreadpool.cpp
	source/threading/execution/kthreads.cpp
	source/time/clock/kdate.cpp
	source/time/clock/kjuliandate.cpp
	source/time/clock/ktime.cpp
	source/time/duration/kduration.cpp
	source/time/duration/kprof.cpp
	source/time/duration/ktimer.cpp
	source/time/scheduler/kron.cpp
	source/util/archive/kuntar.cpp
	source/util/cli/koptions.cpp
	source/util/cli/kxterm.cpp
	source/util/id/krandom.cpp
	source/util/id/kuuid.cpp
	source/util/mail/kmail.cpp
	source/util/mail/ksmtp.cpp
	source/util/misc/kversion.cpp
	source/util/text/kdiff.cpp
	source/web/html/khtmlcontentblocks.cpp
	source/web/html/khtmldom.cpp
	source/web/html/khtmlentities.cpp
	source/web/html/khtmlparser.cpp
	source/web/objects/kwebobjects.cpp
	source/web/url/kmime.cpp
	source/web/url/kurl.cpp
	source/web/url/kurlencode.cpp
	source/web/url/kuseragent.cpp
)

if (DEKAF2_HAS_LIBZIP)
	list(APPEND SOURCES
		source/util/archive/kzip.cpp
	)
endif()

if (DEKAF2_HAS_INCOMPLETE_BOOST_IOSTREAMS_LZMA_BUILD)
	list(APPEND SOURCES source/io/compression/bits/from_boost/iostreams/lzma.cpp)
endif()

if (DEKAF2_HAS_LIBBROTLI)
	list(APPEND SOURCES source/io/compression/bits/kbrotli.cpp)
endif()

if (DEKAF2_HAS_LIBZSTD)
	list(APPEND SOURCES source/io/compression/bits/kzstd.cpp)
endif()

list(SORT SOURCES)

if (NOT CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10.0)
	# keep this one at the top (it takes the most time to compile)
	list(INSERT SOURCES 0 source/web/html/khtmlentities4.cpp)
else()
	# move it to the end, as it needs a lot of RAM with older gccs
	list(APPEND SOURCES source/web/html/khtmlentities4.cpp)
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

if (IS_WINDOWS)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS NOMINMAX)
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
	# we need the core foundation framework for the locale retrieval on MacOS
	set (MACOS_FRAMEWORKS "-framework Foundation")
endif()

if (IS_WINDOWS)
	# we need the dbghelp library for the data segment detection on Windows,
	# and ihlpapi for network interface code
	set (WINDOWS_LIBRARIES "dbghelp" "iphlpapi")
endif()

set(DEKAF2_SOURCE_FILES
	# we add the HEADERS as well, as otherwise cmake based IDEs would not see them
	# as belonging to the project. CMake would not need them here.
	${HEADERS}
	${SOURCES}
)

# create hierarchical source groups for IDEs (Xcode, Visual Studio, etc.)
# this places .h and .cpp files in the same tree node matching the directory structure
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${DEKAF2_SOURCE_FILES})

if (DEKAF2_INSTALL_INCLUDE)
	set (DEKAF2_PUBLIC_INCLUDE_DIRECTORIES
		$<INSTALL_INTERFACE:${dekaf2_include_dest}>
		$<INSTALL_INTERFACE:${dekaf2_lib_dest}/include>
		${Boost_INCLUDE_DIR}
		${OPENSSL_INCLUDE_DIR}
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
		${MACOS_FRAMEWORKS}
		${WINDOWS_LIBRARIES}
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
# add dekaf2's own headers as non-SYSTEM includes so that Xcode tracks their dependencies
target_include_directories (dekaf2-objects PUBLIC $<BUILD_INTERFACE:${DEKAF2_GENERATED_FILES_DIR}>)
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
	target_include_directories (dekaf2-shared  PUBLIC $<BUILD_INTERFACE:${DEKAF2_GENERATED_FILES_DIR}>)
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
	target_include_directories (dekaf2 PUBLIC $<BUILD_INTERFACE:${DEKAF2_GENERATED_FILES_DIR}>)
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

if (DEKAF2_HAS_POSTGRESQL)
	set(KSQL2_INCLUDE_DIRECTORIES
		${KSQL2_INCLUDE_DIRECTORIES}
		$<BUILD_INTERFACE:${PostgreSQL_INCLUDE_DIR}>
	)
	set(KSQL2_LINK_LIBRARIES
		${KSQL2_LINK_LIBRARIES}
		${PostgreSQL_LIBRARY}
	)
endif()

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

set(KSQL2_SOURCE_FILES source/data/sql/ksql.cpp source/data/sql/ksql.h source/data/sql/bits/ksql_dbc.cpp source/data/sql/bits/ksql_dbc.h)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${KSQL2_SOURCE_FILES})
add_library                   (ksql2-objects OBJECT ${KSQL2_SOURCE_FILES})
target_include_directories    (ksql2-objects PUBLIC ${KSQL2_INCLUDE_DIRECTORIES})
target_link_libraries         (ksql2-objects ${KSQL2_LINK_LIBRARIES} dekaf2)
target_compile_definitions    (ksql2-objects ${KSQL2_COMPILE_DEFINITIONS})
#if (DEKAF2_USE_PRECOMPILED_HEADERS)
#	target_precompile_headers (ksql2-objects REUSE_FROM dekaf2-objects)
#endif()

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
		add_library(ksqlite-shared SHARED source/data/sql/ksqlite.cpp source/data/sql/ksqlite.h)
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

		#if (DEKAF2_USE_PRECOMPILED_HEADERS)
		#	target_precompile_headers(ksqlite-shared REUSE_FROM dekaf2-objects)
		#endif()
	endif() # DEKAF2_BUILD_SHARED_DEKAF2

	if (DEKAF2_BUILD_STATIC_DEKAF2)
		add_library(ksqlite STATIC source/data/sql/ksqlite.cpp source/data/sql/ksqlite.h)
		target_include_directories (ksqlite PRIVATE $<BUILD_INTERFACE:${SQLITE3_INCLUDE_DIRS}>)
		target_link_libraries(ksqlite
			PRIVATE
				dekaf2
				${SQLITE3_LIBRARIES}
			)
		# DEKAF2_LIBRARY_BUILD is needed for dynamic libs
		target_compile_definitions(ksqlite PRIVATE DEKAF2_LIBRARY_BUILD)

		#if (DEKAF2_USE_PRECOMPILED_HEADERS)
		#	target_precompile_headers(ksqlite REUSE_FROM dekaf2-objects)
		#endif()
	endif() # DEKAF2_BUILD_STATIC_DEKAF2

	# ksql2 uses KSQLite classes, link ksqlite into ksql2
	if (DEKAF2_BUILD_SHARED_DEKAF2)
		target_link_libraries(ksql2-shared PRIVATE ksqlite-shared)
	endif()
	if (DEKAF2_BUILD_STATIC_DEKAF2)
		target_link_libraries(ksql2 PUBLIC ksqlite)
	endif()

endif() # SQLITE3_FOUND

if (DEKAF2_INSTALL_BIN)

	add_executable(klog tools/klog_main.cpp)
	target_link_libraries(klog dekaf2${DEKAF2_LINK_SHARED})

	add_executable(createdbc tools/createdbc.cpp)
	target_link_libraries(createdbc ksql2${DEKAF2_LINK_SHARED} dekaf2${DEKAF2_LINK_SHARED})

	if (DEKAF2_HAS_NCURSES)
		add_executable(unixmenu tools/unixmenu.cpp)
		target_include_directories(unixmenu PUBLIC CURSES_INCLUDE_DIRS)
		target_link_libraries(unixmenu dekaf2${DEKAF2_LINK_SHARED} ncurses)
	endif()

	add_executable(dekaf2project tools/dekaf2project.cpp)
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

	add_executable(ktunnel samples/ktunnel.cpp samples/ktunnel.h)
	target_link_libraries(ktunnel dekaf2${DEKAF2_LINK_SHARED})

	add_executable(statinfo samples/statinfo.cpp)
	target_link_libraries(statinfo dekaf2${DEKAF2_LINK_SHARED})

	add_executable(getentities EXCLUDE_FROM_ALL source/web/html/getentities.cpp)
	target_link_libraries(getentities dekaf2${DEKAF2_LINK_SHARED})

	add_executable(genunicodetables EXCLUDE_FROM_ALL source/core/types/genunicodetables.cpp)
	target_link_libraries(genunicodetables dekaf2${DEKAF2_LINK_SHARED})

	add_executable(diff-speedtest EXCLUDE_FROM_ALL from/google/diff-speedtest.cpp)
	target_link_libraries(diff-speedtest dekaf2${DEKAF2_LINK_SHARED})

	add_subdirectory(samples/mong2sql)

endif()

# ============================ UNIT AND SMOKE TESTS ==================================

enable_testing()

if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/utests")
	message(STATUS "adding unit tests")
	add_subdirectory(utests)
	add_test(NAME dekaf2-unit-tests COMMAND dekaf2-utests)
	set_tests_properties(dekaf2-unit-tests PROPERTIES LABELS "unit")
endif()
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/smoketests")
	message(STATUS "adding smoke tests")
	add_subdirectory(smoketests)
	add_test(NAME smoketest-mysql
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/smoketest" --build-dir "${CMAKE_BINARY_DIR}" mysql)
	set_tests_properties(smoketest-mysql PROPERTIES
		LABELS "smoketest;mysql"
		TIMEOUT 120)
	add_test(NAME smoketest-postgresql
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/smoketest" --build-dir "${CMAKE_BINARY_DIR}" postgresql)
	set_tests_properties(smoketest-postgresql PROPERTIES
		LABELS "smoketest;postgresql"
		TIMEOUT 120)
endif()
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
	message(STATUS "adding benchmark tests")
	add_subdirectory(benchmarks)
endif()

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
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/data/sql/ksqlite.h)")
	endif()

	if (NOT DEKAF2_HAS_LIBZIP)
		# make sure to remove a kzip.h header if this build did not find libzip
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/kzip.h)")
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/dekaf2/util/archive/kzip.h)")
	endif()

	if(DEKAF2_USE_SYSTEM_FMTLIB)
		install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/${dekaf2_include_dest}/fmt)")
	endif()

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

	install(TARGETS klog createdbc dekaf2project kurl khttp kgrep kreplace ksql krypt ktunnel statinfo DESTINATION ${dekaf2_bin_dest})
	if (DEKAF2_HAS_NCURSES)
		install(TARGETS unixmenu DESTINATION ${dekaf2_bin_dest})
	endif()

	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/mysql-newuser"   DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/findcol"         DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/kport"           DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/my-ip-addr"      DESTINATION ${dekaf2_bin_dest})
	install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/dekaf2unflat.py" DESTINATION ${dekaf2_bin_dest})

endif() # DEKAF2_INSTALL_BIN

if (DEKAF2_INSTALL_INCLUDE)

	# install hierarchical headers (preserves source/ directory structure under dekaf2/)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source/" DESTINATION "${dekaf2_include_dest}/dekaf2" FILES_MATCHING PATTERN "*.h")

	# install flat backward-compatible header stubs
	if (DEKAF2_INSTALL_FLAT_HEADERS)
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/compat/flat/" DESTINATION "${dekaf2_include_dest}/dekaf2" FILES_MATCHING PATTERN "*.h")
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
	if (DEKAF2_HAS_USER_AGENT_PARSER)
		install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/libs/uap-cpp/uap-core/regexes.yaml" DESTINATION "${dekaf2_share_dest}/uap-core")
	endif()

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
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/krypt"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/ktunnel"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${dekaf2_bin_dest}/statinfo"
)

# =================================== THE END ========================================
