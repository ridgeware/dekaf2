# =================================== THE BEGIN ====================================

cmake_minimum_required(VERSION 3.13)

# ==================================== PROJECT =====================================

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(samples VERSION 1.0.0 DESCRIPTION "dekaf2 usage samples")

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================================== BUILD TYPE ==================================

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

if (CMAKE_GENERATOR MATCHES "Makefile")

	# single configuration setup - make sure we have a build type

	if (NOT CMAKE_BUILD_TYPE)
		# Release is the default build type
		set(CMAKE_BUILD_TYPE "Release")
	endif()

else()

	# clear any build type to make understood we build a multi configuration setup
	set(CMAKE_BUILD_TYPE "")

endif()

# =================================== LIBRARIES ====================================

set(DEKAF2_INHERIT_BUILD_FLAGS ON)
find_package(dekaf2 2.0 REQUIRED)

# ==================================== FILES ========================================

set (KREPLACE_SOURCES
    kreplace.cpp
)

set (KREPLACE_HEADERS
    kreplace.h
)

# ==================================== SAMPLES =====================================

add_executable(kreplace ${KREPLACE_HEADERS} ${KREPLACE_SOURCES})
target_link_libraries(kreplace dekaf2)

# ================================= INSTALLATION =====================================

set(bin_dest "bin")
set(include_dest "include/${PROJECT_NAME}")
set(main_lib_dest "lib/${PROJECT_NAME}")
set(lib_dest "${main_lib_dest}/${CMAKE_BUILD_TYPE}")
set(share_dest "share/${PROJECT_NAME}")

# always put this first, otherwise it risks to remove other already installed cmake config files
#install(EXPORT ${PROJECT_NAME} DESTINATION "${lib_dest}")

install(TARGETS kreplace DESTINATION ${bin_dest})

set(CPACK_SOURCE_GENERATOR ZIP)
set(CPACK_SOURCE_IGNORE_FILES ${ignored_files})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
set(CPACK_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

# ================================= UNINSTALL ========================================

add_custom_target(uninstall-samples
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${main_lib_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${share_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${include_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${bin_dest}/kreplace"
)

# =================================== THE END ========================================
