cmake_minimum_required(VERSION 3.2)

project(dekaf-smoketests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (UHEADERS
    catch.hpp
)

set (USOURCES
    main.cpp
    kcurl_smoketests.cpp
    kwebio_smoketests.cpp
    kinshell_smoketests.cpp
)

add_executable(dekaf-smoketests ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf-smoketests dekaf2)

