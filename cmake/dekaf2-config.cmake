# dekaf2-config.cmake - package configuration file

# if no build type is used, pick the release version of the library
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

# we only support the Release and Debug output locations - other
# build types have to pick either or
if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "release")
	set(DEKAF2_OUTPUT_PATH "Release")
elseif(CMAKE_BUILD_TYPE STREQUAL "debug")
	set(DEKAF2_OUTPUT_PATH "Debug")
else()
	set(DEKAF2_OUTPUT_PATH ${CMAKE_BUILD_TYPE})
endif()

get_filename_component(SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(${SELF_DIR}/${DEKAF2_OUTPUT_PATH}/dekaf2.cmake)
include(${SELF_DIR}/${DEKAF2_OUTPUT_PATH}/dekaf2-setup.cmake)
