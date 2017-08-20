cmake_minimum_required(VERSION 3.2)

project(dekaf2-smoketests)

set(CMAKE_CXX_STANDARD 17)
# do not require cpp17
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (UHEADERS
    catch.hpp
)

set (USOURCES
    main.cpp
    kcurl_smoketests.cpp
    kwebio_smoketests.cpp
    kinshell_smoketests.cpp
)

add_executable(dekaf2-smoketests ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf2-smoketests dekaf2)

