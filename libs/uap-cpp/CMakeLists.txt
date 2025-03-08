cmake_minimum_required(VERSION 3.8...3.31)

project(uap-cpp)

if (false)

IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Werror -fPIC")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

endif()

set(LIB_SOURCES
        UaParser.cpp
        internal/AlternativeExpander.cpp
        internal/Pattern.cpp
        internal/ReplaceTemplate.cpp
        internal/SnippetIndex.cpp)

set(LIB_HEADERS
        UaParser.h
        internal/AlternativeExpander.h
        internal/MakeUnique.h
        internal/Pattern.h
        internal/ReplaceTemplate.h
        internal/SnippetIndex.h
        internal/SnippetMapping.h
        internal/StringUtils.h
        internal/StringView.h)

find_package(yaml-cpp REQUIRED)

#find_package(re2)

# this is the "object library" target: compiles the sources only once
add_library(uap-cpp-objects OBJECT ${LIB_SOURCES} ${LIB_HEADERS})
target_include_directories(uap-cpp-objects PRIVATE ${YAML_CPP_INCLUDE_DIR})
target_link_libraries(uap-cpp-objects re2)

# shared libraries need PIC
#set_property(TARGET uap-cpp-objects PROPERTY POSITION_INDEPENDENT_CODE 1)

# shared and static libraries built from the same object files
add_library(uap-cpp-static STATIC $<TARGET_OBJECTS:uap-cpp-objects>)
#set_target_properties(uap-cpp-static PROPERTIES OUTPUT_NAME uaparser_cpp)

#add_library(uap-cpp-shared SHARED $<TARGET_OBJECTS:objlib>)
#set_target_properties(uap-cpp-shared PROPERTIES OUTPUT_NAME uaparser_cpp)


if (false)

set(TEST_SOURCES
        UaParserTest.cpp)

find_package(GTest)

add_executable(tests ${TEST_SOURCES} $<TARGET_OBJECTS:uap-cpp-objects>)
set_target_properties(tests PROPERTIES OUTPUT_NAME UaParserTest)

target_link_libraries(tests re2 yaml-cpp gtest pthread)



set(BENCH_SOURCES
        benchmarks/UaParserBench.cpp)

add_executable(bench ${BENCH_SOURCES} $<TARGET_OBJECTS:uap-cpp-objects>)
set_target_properties(bench PROPERTIES OUTPUT_NAME UaParserBench)

target_link_libraries(bench re2 yaml-cpp pthread)

endif()
