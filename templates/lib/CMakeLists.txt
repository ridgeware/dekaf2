# =================================== THE BEGIN ====================================

cmake_minimum_required(VERSION 3.13)

if (POLICY CMP0091)
	# let the dekaf2 setup select the runtime library on Windows - dekaf2 may be
	# linked against the static runtime, and every target then has to use the same
	cmake_policy(SET CMP0091 NEW)
endif()

# ==================================== PROJECT =====================================

# declare the project
project(__LowerProjectName__ VERSION __ProjectVersion__ DESCRIPTION "__ProjectName__ based on dekaf2")

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

# take over the build flags of dekaf2, including its C++ standard
set(DEKAF2_INHERIT_BUILD_FLAGS ON)
find_package(dekaf2 2.0 REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ==================================== FILES ========================================

set (SOURCES
__SourceFiles__)

# the public headers of the library
set (HEADERS
	include/__LowerProjectName__/__LowerProjectName__.h
__HeaderFiles__)

# ================================ __ProjectName__ =====================================

# the library
add_library(${PROJECT_NAME} ${HEADERS} ${SOURCES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC dekaf2::dekaf2)
target_include_directories(${PROJECT_NAME}
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# an example program that uses the library
add_executable(${PROJECT_NAME}-example example/example.cpp)
target_link_libraries(${PROJECT_NAME}-example ${PROJECT_NAME})

# ================================= INSTALLATION =====================================

set(cmake_dest "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-targets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# the files for find_package(__LowerProjectName__) in other projects
install(EXPORT ${PROJECT_NAME}-targets NAMESPACE ${PROJECT_NAME}:: DESTINATION ${cmake_dest})

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	INSTALL_DESTINATION ${cmake_dest}
)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	COMPATIBILITY SameMajorVersion
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	DESTINATION ${cmake_dest}
)

if (NOT CPACK_SOURCE_GENERATOR)
	set(CPACK_SOURCE_GENERATOR ZIP)
endif()
set(CPACK_SOURCE_IGNORE_FILES ${ignored_files})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${CPACK_PACKAGE_VERSION})
include(CPack)

# ================================= UNINSTALL ========================================

add_custom_target(uninstall
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
	COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}/${cmake_dest}"
	COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
)

# =================================== THE END ========================================
