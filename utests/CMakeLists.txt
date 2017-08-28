cmake_minimum_required(VERSION 3.8)

project(dekaf2-utests)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (UHEADERS
    catch.hpp
)

set (USOURCES
    main.cpp
    kstringview_tests.cpp
    kstring_tests.cpp
    kstringutils_tests.cpp
    kprops_tests.cpp
    kurl_tests.cpp
    kwebio_tests.cpp
    kcurl_tests.cpp
    kfile_tests.cpp
    ksubscribe_tests.cpp
    kreader_tests.cpp
    kinshell_tests.cpp
    koutshell_tests.cpp
    kstack_tests.cpp
    ktcpserver_tests.cpp
    klog_tests.cpp
)

add_executable(dekaf2-utests ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf2-utests dekaf2)

