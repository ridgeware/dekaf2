cmake_minimum_required(VERSION 3.2)

project(dekaf2-utests)

if(${CMAKE_VERSION} VERSION_GREATER "3.7.99")
	set(CMAKE_CXX_STANDARD 17)
	#set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set (UHEADERS
	catch.hpp
)

# keep alphabetic sort order
set (USOURCES
	main.cpp
	kctype_tests.cpp
	kallocator_tests.cpp
	kassociative_tests.cpp
	kbase64_tests.cpp
	kbitfields_tests.cpp
	kcasestring_tests.cpp
	kcompression_tests.cpp
	kcookie_tests.cpp
	kcrc_tests.cpp
	kcsv_tests.cpp
	khash_tests.cpp
	khtmlcontentblocks_tests.cpp
	kencode_tests.cpp
	kfilesystem_tests.cpp
	kgetruntimestack_tests.cpp
	khmac_tests.cpp
	khtmlparser_tests.cpp
	khtmlcontentblocks_tests.cpp
	khtmlentities_tests.cpp
	khttpclient_tests.cpp
	khttpserver_tests.cpp
	kinpipe_tests.cpp
	kinshell_tests.cpp
	kjoin_tests.cpp
	kjson_tests.cpp
	klog_tests.cpp
	kmessagedigest_tests.cpp
	kmime_tests.cpp
	kmpsearch_tests.cpp
	koutpipe_tests.cpp
	koutputtemplate_tests.cpp
	koutshell_tests.cpp
	kpipe_tests.cpp
	kprops_tests.cpp
	kquotedprintable_tests.cpp
	kreader_tests.cpp
	kregex_tests.cpp
	kreplacer_tests.cpp
	krest_tests.cpp
	krestclient_tests.cpp
	krestserver_tests.cpp
	krow_tests.cpp
	ksharedptr_tests.cpp
	ksplit_tests.cpp
	ksqlite_tests.cpp
	kstack_tests.cpp
	kstring_tests.cpp
	kstringstream_tests.cpp
	kstringutils_tests.cpp
	kstringview_tests.cpp
	kstringviewz_tests.cpp
	ksubscribe_tests.cpp
	ksystem_tests.cpp
	ksystemstats_tests.cpp
	ktcpserver_tests.cpp
	kuntar_tests.cpp
	kurl_tests.cpp
	kutf8_tests.cpp
	kutf8iterator_tests.cpp
	kversion_tests.cpp
	kwords_tests.cpp
	kwriter_tests.cpp
	kxml_tests.cpp
	kzip_tests.cpp
)

add_executable(dekaf2-utests EXCLUDE_FROM_ALL ${UHEADERS} ${USOURCES})

if(SQLITE3_FOUND)
	target_link_libraries(dekaf2-utests dekaf2 ksqlite)
else()
	target_link_libraries(dekaf2-utests dekaf2)
endif()

