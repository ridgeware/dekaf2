
cmake_minimum_required(VERSION 3.13...3.31)

# ==================================== PROJECT =====================================

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(mong2sql
    VERSION 1.0.0
    DESCRIPTION "Convert MongoDB collections to MySQL tables"
)


if (EXISTS "/etc/amazon-linux-release-cpe" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_STANDARD 17)
else()
	set(CMAKE_CXX_STANDARD 20)
endif()

# we need the share_dest early, so define it here
set(share_dest "${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}")

find_package(PkgConfig REQUIRED)

# ================================= CONFIG OPTIONS =================================

OPTION(MONG2SQL_USE_PRECOMPILED_HEADERS "use precompiled headers" ON)
OPTION(MONG2SQL_WITH_PEDANTIC_WARNINGS "enable pedantic warnings" ON)

# ==================================== BUILD TYPE ==================================

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

if (CMAKE_GENERATOR MATCHES "Makefile")
	# single configuration setup - make sure we have a build type
	if (NOT CMAKE_BUILD_TYPE)
		# Release is the default build type
		set(CMAKE_BUILD_TYPE "Release")
	endif()
else()
	if (NOT CMAKE_BUILD_TYPE)
		# Debug is the default build type for multi-config
		set(CMAKE_BUILD_TYPE "Debug")
	endif()

	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		if (CMAKE_GENERATOR STREQUAL "Xcode")
			# enable ASAN and UBSAN in xcode, as we need them to link
			# against debug libraries
			set(CMAKE_XCODE_GENERATE_SCHEME ON)
			set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER ON)
			set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN ON)
			set(CMAKE_XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER ON)
		endif()
	endif()
endif()

if ("${MONG2SQL_INSTALL_TARGET}" STREQUAL "")
	set(MONG2SQL_INSTALL_TARGET
		lib
		cli
		server
	)
endif()

if (MONG2SQL_USE_PRECOMPILED_HEADERS)
	if (CMAKE_VERSION VERSION_LESS "3.16.0")
		message(STATUS "need cmake version >= 3.16 for precompiled headers (have ${CMAKE_VERSION}) - disabled")
		set(MONG2SQL_USE_PRECOMPILED_HEADERS OFF)
	else()
		message(STATUS "build mong2sql with precompiled headers")
	endif()
endif()

# =================================== LIBRARIES ====================================

set(DEKAF2_INHERIT_BUILD_FLAGS ON)
find_package(dekaf2 2.0 REQUIRED)
find_package(xapis REQUIRED)
find_package(xliff REQUIRED)

# MongoDB C++ driver
find_package(mongocxx CONFIG)
find_package(bsoncxx CONFIG)

if(NOT mongocxx_FOUND)
    pkg_check_modules(MONGOCXX REQUIRED libmongocxx)
endif()

if(NOT bsoncxx_FOUND)
    pkg_check_modules(BSONCXX REQUIRED libbsoncxx)
endif()

set(MONG2SQL_MONGO_LIBS "")
set(MONG2SQL_MONGO_INCLUDE_DIRS "")
set(MONG2SQL_MONGO_LIBRARY_DIRS "")

if(mongocxx_FOUND)
    list(APPEND MONG2SQL_MONGO_LIBS mongo::mongocxx_shared)
else()
    list(APPEND MONG2SQL_MONGO_LIBS ${MONGOCXX_LIBRARIES})
    list(APPEND MONG2SQL_MONGO_INCLUDE_DIRS ${MONGOCXX_INCLUDE_DIRS})
    list(APPEND MONG2SQL_MONGO_LIBRARY_DIRS ${MONGOCXX_LIBRARY_DIRS})
endif()

if(bsoncxx_FOUND)
    list(APPEND MONG2SQL_MONGO_LIBS mongo::bsoncxx_shared)
else()
    list(APPEND MONG2SQL_MONGO_LIBS ${BSONCXX_LIBRARIES})
    list(APPEND MONG2SQL_MONGO_INCLUDE_DIRS ${BSONCXX_INCLUDE_DIRS})
    list(APPEND MONG2SQL_MONGO_LIBRARY_DIRS ${BSONCXX_LIBRARY_DIRS})
endif()

if(MONG2SQL_MONGO_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES MONG2SQL_MONGO_INCLUDE_DIRS)
endif()

if(MONG2SQL_MONGO_LIBRARY_DIRS)
    list(REMOVE_DUPLICATES MONG2SQL_MONGO_LIBRARY_DIRS)
endif()

# ==================================== FILES ========================================

set (SOURCES
	mong2sql.cpp
)

set (HEADERS
#	mong2sql.h
)

set (PRECOMPILED_HEADERS
	<memory>
	<unordered_map>
	<vector>
	<dekaf2/kcompatibility.h>
	<dekaf2/dekaf2.h>
	<dekaf2/kcrashexit.h>
	<dekaf2/kencode.h>
	<dekaf2/kexception.h>
	<dekaf2/kfilesystem.h>
	<dekaf2/kformat.h>
	<dekaf2/kjson.h>
	<dekaf2/klog.h>
	<dekaf2/kmessagedigest.h>
	<dekaf2/koptions.h>
	<dekaf2/krest.h>
	<dekaf2/kstring.h>
	<dekaf2/kstringview.h>
	<dekaf2/ksql.h>
	<dekaf2/ktime.h>
	<dekaf2/kurl.h>
	<dekaf2/kwebclient.h>
)

# create and include the build directory for generated files
set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")

# create the cmake config file for find_package()
configure_file("cmake/${PROJECT_NAME}-config.cmake.in" "${GENERATED_FILES_DIR}/${PROJECT_NAME}-config.cmake" @ONLY)

# ==================================== MONG2SQL =========================================
if (MONG2SQL_WITH_PEDANTIC_WARNINGS)
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (MONG2SQL_INSTALL_TARGET MATCHES "lib")
    add_library(mong2sql-objects OBJECT ${SOURCES} ${HEADERS})
    target_link_libraries(mong2sql-objects
        PRIVATE
            dekaf2::dekaf2
            xapislib
            ${MONG2SQL_MONGO_LIBS}
    )
    target_include_directories(mong2sql-objects
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        # needed for libmong2sql
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(MONG2SQL_MONGO_INCLUDE_DIRS)
        target_include_directories(mong2sql-objects
            PRIVATE
                ${MONG2SQL_MONGO_INCLUDE_DIRS}
        )
    endif()

    if(MONG2SQL_MONGO_LIBRARY_DIRS)
        target_link_directories(mong2sql-objects
            PRIVATE
                ${MONG2SQL_MONGO_LIBRARY_DIRS}
        )
    endif()

    if (MONG2SQL_USE_PRECOMPILED_HEADERS)
        target_precompile_headers(mong2sql-objects PRIVATE ${PRECOMPILED_HEADERS} ${HEADERS})
    endif()
endif()

if (MONG2SQL_INSTALL_TARGET MATCHES "server")
    add_executable(mong2sql $<TARGET_OBJECTS:mong2sql-objects>)
    target_link_libraries(mong2sql
        PRIVATE
            dekaf2::dekaf2
            xapislib
            ${MONG2SQL_MONGO_LIBS}
    )
    target_include_directories(mong2sql
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(MONG2SQL_MONGO_INCLUDE_DIRS)
        target_include_directories(mong2sql
            PRIVATE
                ${MONG2SQL_MONGO_INCLUDE_DIRS}
        )
    endif()

    if(MONG2SQL_MONGO_LIBRARY_DIRS)
        target_link_directories(mong2sql
            PRIVATE
                ${MONG2SQL_MONGO_LIBRARY_DIRS}
        )
    endif()

    install(TARGETS mong2sql DESTINATION bin)
endif()

# ==================================== INSTALL ========================================

# =================================== UNINSTALL ========================================

add_custom_target(uninstall
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}"
)

# =================================== THE END ========================================
