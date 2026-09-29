# dekaf2-vcpkg-targets.cmake - imported targets for the libraries that come
# from vcpkg without a CMake config of their own: nghttp2 and FreeTDS.
#
# The vcpkg build of dekaf2 links them through these targets, so that
# dekaf2-targets.cmake names targets instead of paths of the build machine.
# An imported target is not exported though: dekaf2-config.cmake includes
# this file as well and creates the same targets for the consumer. No
# pkg-config on either side - Windows has none.

include_guard(GLOBAL)

# dekaf2_vcpkg_import(<target> <header> LIBRARIES <name>... [LINK <item>...])
#
# The libraries in link order, each looked up by name in lib/ of the vcpkg
# tree for release builds and in debug/lib/ for debug builds. LINK adds what
# they need themselves, after them.
function(dekaf2_vcpkg_import sTarget sHeader)
	cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "LIBRARIES;LINK")
	if (TARGET ${sTarget})
		return()
	endif()
	string(MAKE_C_IDENTIFIER "${sTarget}" sVar)
	set(sRoot "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

	find_path(DEKAF2_${sVar}_INCLUDE_DIR NAMES "${sHeader}" PATHS "${sRoot}/include" NO_DEFAULT_PATH)
	if (NOT DEKAF2_${sVar}_INCLUDE_DIR)
		message(FATAL_ERROR "${sTarget}: ${sHeader} not found in ${sRoot}/include")
	endif()

	add_library(${sTarget} INTERFACE IMPORTED)
	set_target_properties(${sTarget} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${DEKAF2_${sVar}_INCLUDE_DIR}")

	foreach(sLib IN LISTS arg_LIBRARIES)
		find_library(DEKAF2_${sVar}_${sLib}_RELEASE NAMES ${sLib} NAMES_PER_DIR PATHS "${sRoot}/lib" NO_DEFAULT_PATH)
		find_library(DEKAF2_${sVar}_${sLib}_DEBUG NAMES ${sLib} ${sLib}d NAMES_PER_DIR PATHS "${sRoot}/debug/lib" NO_DEFAULT_PATH)
		if (NOT DEKAF2_${sVar}_${sLib}_RELEASE)
			message(FATAL_ERROR "${sTarget}: library ${sLib} not found in ${sRoot}/lib")
		endif()
		if (NOT DEKAF2_${sVar}_${sLib}_DEBUG)
			# a release-only triplet
			set(DEKAF2_${sVar}_${sLib}_DEBUG "${DEKAF2_${sVar}_${sLib}_RELEASE}")
		endif()
		# one generator expression per library - a list inside one would be split
		set_property(TARGET ${sTarget} APPEND PROPERTY INTERFACE_LINK_LIBRARIES
			"$<$<NOT:$<CONFIG:Debug>>:${DEKAF2_${sVar}_${sLib}_RELEASE}>"
			"$<$<CONFIG:Debug>:${DEKAF2_${sVar}_${sLib}_DEBUG}>")
	endforeach()

	if (arg_LINK)
		set_property(TARGET ${sTarget} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${arg_LINK})
	endif()
endfunction()

# nghttp2: vcpkg deletes its CMake config. The define NGHTTP2_STATICLIB for
# static Windows builds is patched into the header by vcpkg already
function(dekaf2_vcpkg_import_nghttp2)
	dekaf2_vcpkg_import(dekaf2-ext::nghttp2 nghttp2/nghttp2.h LIBRARIES nghttp2)
endfunction()

# FreeTDS, ct-lib: on Windows its CMake build leaves the internal libraries
# next to the static libct, elsewhere autotools puts them into libct.a
function(dekaf2_vcpkg_import_freetds)
	find_package(OpenSSL REQUIRED)
	find_package(Threads REQUIRED)
	if (WIN32)
		dekaf2_vcpkg_import(dekaf2-ext::freetds ctpublic.h
			LIBRARIES libct tds replacements tdsutils
			LINK OpenSSL::SSL OpenSSL::Crypto ws2_32 crypt32 Threads::Threads)
	else()
		find_package(Iconv REQUIRED)
		dekaf2_vcpkg_import(dekaf2-ext::freetds ctpublic.h
			LIBRARIES ct
			LINK OpenSSL::SSL OpenSSL::Crypto Iconv::Iconv Threads::Threads)
	endif()
endfunction()
