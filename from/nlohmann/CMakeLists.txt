cmake_minimum_required(VERSION 3.8)

project(nlohmann)

# providing an include path for nlohmann-json (#include <nlohmann/json.hpp>)
add_library(nlohmann-json INTERFACE)

target_include_directories(nlohmann-json
	INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${dekaf2_include_dest}>
)
