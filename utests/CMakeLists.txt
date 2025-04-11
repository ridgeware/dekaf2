cmake_minimum_required(VERSION 3.13)

project(dekaf2-utests)

if (EXISTS "/etc/amazon-linux-release-cpe" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_STANDARD 17)
else()
	set(CMAKE_CXX_STANDARD 20)
endif()
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# older cmake versions do not support the test for IS_TOP_LEVEL
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(dekaf2-utests_IS_TOP_LEVEL ON)
else()
	set(dekaf2-utests_IS_TOP_LEVEL OFF)
endif()

if (dekaf2-utests_IS_TOP_LEVEL OR NOT DEKAF2_GENERATED_FILES_DIR)
	set(DEKAF2_INHERIT_BUILD_FLAGS ON)
	find_package(dekaf2 2.0 REQUIRED)
	set(DEKAF2_NAMESPACE "dekaf2::")
else()
	set(DEKAF2_NAMESPACE)
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
	kawsauth_tests.cpp
	kbase64_tests.cpp
	kbcrypt_tests.cpp
	kbit_tests.cpp
	kbitfields_tests.cpp
	kcache_tests.cpp
	kcaseless_tests.cpp
	kcasestring_tests.cpp
	kchildprocess_tests.cpp
	kcompatibility_tests.cpp
	kcompression_tests.cpp
	kcookie_tests.cpp
	kcountingstreambuf_tests.cpp
	kcrc_tests.cpp
	kcsv_tests.cpp
	kdate_tests.cpp
	kdiff_tests.cpp
	kduration_tests.cpp
	kencode_tests.cpp
	keraseremove_tests.cpp
	kfilesystem_tests.cpp
	kfindsetofchars_tests.cpp
	kformat_tests.cpp
	kformtable_tests.cpp
	kgetruntimestack_tests.cpp
	khash_tests.cpp
	khmac_tests.cpp
	khtmlparser_tests.cpp
	khtmlcontentblocks_tests.cpp
	khtmldom_tests.cpp
	khtmlentities_tests.cpp
	khttp_header_tests.cpp
	khttp_method_tests.cpp
	khttp_request_tests.cpp
	khttpclient_tests.cpp
	khttpcompression_tests.cpp
	khttplog_tests.cpp
	khttpserver_tests.cpp
	kinpipe_tests.cpp
	kinshell_tests.cpp
	kjoin_tests.cpp
	kjson_tests.cpp
	kjson2_tests.cpp
	klog_tests.cpp
	kmessagedigest_tests.cpp
	kmime_tests.cpp
	kmodifyingstreambuf_tests.cpp
	kmpsearch_tests.cpp
	kngram_tests.cpp
	koptions_tests.cpp
	koutpipe_tests.cpp
	koutputtemplate_tests.cpp
	koutshell_tests.cpp
	kparallel_tests.cpp
	kpersist_tests.cpp
	kpipe_tests.cpp
	kpool_tests.cpp
	kprops_tests.cpp
	kquotedprintable_tests.cpp
	kreader_tests.cpp
	kregex_tests.cpp
	kreplacer_tests.cpp
	krest_tests.cpp
	krestclient_tests.cpp
	krestserver_tests.cpp
	kron_tests.cpp
	krow_tests.cpp
	ksharedmemory_tests.cpp
	ksharedptr_tests.cpp
	ksharedref_tests.cpp
	ksnippets_tests.cpp
	ksourcelocation_tests.cpp
	ksplit_tests.cpp
	ksql_tests.cpp
	ksqlite_tests.cpp
	kstack_tests.cpp
	kstring_tests.cpp
	kstreambuf_tests.cpp
	kstringstream_tests.cpp
	kstringutils_tests.cpp
	kstringview_tests.cpp
	kstringviewz_tests.cpp
	ksubscribe_tests.cpp
	ksystem_tests.cpp
	ksystemstats_tests.cpp
	ktcpserver_tests.cpp
	kthreadpool_tests.cpp
	kthreads_tests.cpp
	kthreadsafe_tests.cpp
	ktime_tests.cpp
	ktimer_tests.cpp
	ktimeseries_tests.cpp
	kuntar_tests.cpp
	kurl_tests.cpp
	kurlencode_tests.cpp
	kuseragent_tests.cpp
	kutf_tests.cpp
	kutf_iterator_tests.cpp
	kvariant_tests.cpp
	kversion_tests.cpp
	kwebclient_tests.cpp
	kwebobjects_tests.cpp
	kwebsocket_tests.cpp
	kwords_tests.cpp
	kwriter_tests.cpp
	kxml_tests.cpp
	kyaml_tests.cpp
	kzip_tests.cpp
	simd_tests.cpp
	string_balance_tests.cpp
)

add_executable(dekaf2-utests EXCLUDE_FROM_ALL ${UHEADERS} ${USOURCES})

if (DEKAF2_HAS_SQLITE3)
	target_link_libraries(dekaf2-utests ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}ksqlite${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}ksql2${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}re2)
else()
	target_link_libraries(dekaf2-utests ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}ksql2${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}re2)
endif()

find_package(GoogleBench QUIET)
if (GoogleBench_FOUND)
	add_executable(dekaf2-benchmark EXCLUDE_FROM_ALL benchmarks.cpp)
	target_link_libraries(dekaf2-benchmark ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} ${GOOGLEBENCH_LIBRARIES})
	target_include_directories(dekaf2-benchmark PRIVATE ${GOOGLEBENCH_INCLUDE_DIRS})
endif()
