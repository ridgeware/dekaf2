cmake_minimum_required(VERSION 3.2)

project(nlohmann)

# providing an include path for nlohmann-json (#include <nlohmann/json.hpp>)
add_library(nlohmann-json INTERFACE)
set_target_properties(nlohmann-json PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
