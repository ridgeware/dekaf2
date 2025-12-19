
cmake_minimum_required(VERSION 3.13...3.31)

# ==================================== PROJECT =====================================

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(mong2sql
    VERSION 1.0.0
    DESCRIPTION "Convert MongoDB collections to MySQL tables"
)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig QUIET)

# ================================= CONFIG OPTIONS =================================

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

# =================================== LIBRARIES ====================================

# MongoDB C++ driver
find_package(mongocxx CONFIG QUIET)
find_package(bsoncxx CONFIG QUIET)

if(NOT mongocxx_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(MONGOCXX libmongocxx QUIET)
endif()

if(NOT bsoncxx_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(BSONCXX libbsoncxx QUIET)
endif()

if (NOT mongocxx_FOUND OR NOT bsoncxx_FOUND)
	return()
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

# create and include the build directory for generated files
set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_FILES_DIR}")

# create the cmake config file for find_package()
configure_file("cmake/${PROJECT_NAME}-config.cmake.in" "${GENERATED_FILES_DIR}/${PROJECT_NAME}-config.cmake" @ONLY)

# ==================================== MONG2SQL =========================================

if (MONG2SQL_WITH_PEDANTIC_WARNINGS)
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(mong2sql ${SOURCES} ${HEADERS})

target_include_directories(mong2sql
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
	PRIVATE
	# needed for libmong2sql
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(MONG2SQL_MONGO_INCLUDE_DIRS)
	target_include_directories(mong2sql
		PRIVATE
			${MONG2SQL_MONGO_INCLUDE_DIRS}
	)
endif()

target_link_libraries(mong2sql
	PRIVATE
		dekaf2
		ksql2
		${MONG2SQL_MONGO_LIBS}
)

if(MONG2SQL_MONGO_LIBRARY_DIRS)
	target_link_directories(mong2sql
		PRIVATE
			${MONG2SQL_MONGO_LIBRARY_DIRS}
	)
endif()

# ==================================== INSTALL ========================================

install(TARGETS mong2sql DESTINATION bin)

# =================================== UNINSTALL ========================================

#add_custom_target(uninstall
#	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}"
#)

# =================================== THE END ========================================
