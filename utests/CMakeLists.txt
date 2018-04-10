cmake_minimum_required(VERSION 3.8)

project(dekaf2-utests)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (UHEADERS
	catch.hpp
)

# keep alphabetic sort order
set (USOURCES
	main.cpp
	kbase64_tests.cpp
	kcasestring_tests.cpp
	kcompression_tests.cpp
	kencode_tests.cpp
	kfile_tests.cpp
	khtmlparser_tests.cpp
	khttpclient_tests.cpp
	kinpipe_tests.cpp
	kinshell_tests.cpp
	kjson_tests.cpp
	klog_tests.cpp
#	kostringstream_tests.cpp
	koutpipe_tests.cpp
	koutshell_tests.cpp
	kpipe_tests.cpp
	kprops_tests.cpp
	kreader_tests.cpp
	ksmtp_tests.cpp
	ksplit_tests.cpp
	kstack_tests.cpp
	kstring_tests.cpp
	kstringstream_tests.cpp
	kstringutils_tests.cpp
	kstringview_tests.cpp
	ksubscribe_tests.cpp
	ktcpserver_tests.cpp
	kurl_tests.cpp
	kutf8_tests.cpp
	kwords_tests.cpp
	kwriter_tests.cpp
)

add_executable(dekaf2-utests ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf2-utests dekaf2)

