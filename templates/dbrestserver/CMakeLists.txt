# =================================== THE BEGIN ====================================

cmake_minimum_required(VERSION 3.13)

# ==================================== PROJECT =====================================

# allow version number in project command
cmake_policy(SET CMP0048 NEW)

# declare the project
project(__LowerProjectName__ VERSION __ProjectVersion__ DESCRIPTION "__ProjectName__ based on dekaf2")

set(CMAKE_CXX_STANDARD 17)

# if this project requires a specific C++ version, uncomment the next line
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

set (SOURCES
__SourceFiles__)

set (HEADERS
__HeaderFiles__)

# ================================ __ProjectName__ =====================================

# Compile all __LowerProjectName__ objects
add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
target_link_libraries(${PROJECT_NAME} dekaf2 ksql2)

# ================================= INSTALLATION =====================================

set(bin_dest "${CMAKE_INSTALL_PREFIX}/bin")

install(TARGETS ${PROJECT_NAME} DESTINATION ${bin_dest})

if (NOT CPACK_SOURCE_GENERATOR)
	set(CPACK_SOURCE_GENERATOR ZIP)
endif()
set(CPACK_SOURCE_IGNORE_FILES ${ignored_files})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
include(CPack)

# ================================= UNINSTALL ========================================

add_custom_target(uninstall
	COMMAND "${CMAKE_COMMAND}" -E remove "${bin_dest}/${PROJECT_NAME}"
)

# =================================== THE END ========================================

