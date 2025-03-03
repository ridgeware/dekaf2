cmake_minimum_required(VERSION 3.8...3.31)
# project(libs)

include(ExternalProject)

if (NOT DEKAF2_USE_SYSTEM_FMTLIB)
	# add the format lib
	add_subdirectory(fmt-10.0.0)
endif()

# add fast regular expressions
add_subdirectory(re2 EXCLUDE_FROM_ALL)
if (DEKAF2_HAS_WARN_ARRAY_BOUNDS)
	if (CMAKE_COMPILER_IS_GNUCXX)
		# gcc 14.1.1 sees array overflows in re2
		# and, yes, one can add target options in a parent directory,
		# after the add_subdirectory!
		target_compile_options(re2 PRIVATE -Wno-array-bounds)
	endif()
endif()

# add the frozen lib (constexpr set and map)
add_subdirectory(frozen EXCLUDE_FROM_ALL)

# add the user agent parser with regular expressions
add_subdirectory(uap-cpp)

if (DEKAF2_WITH_FCGI)
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# add FCGI as an external project in the local "fcgi" directory:
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/libfcgi)

	ExternalProject_Add(
		ext_libfcgi
		PREFIX ${CMAKE_CURRENT_BINARY_DIR}/ext_libfcgi-prefix
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fcgi/
		CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fcgi/configure
		    --quiet
			--disable-shared
			--prefix=${CMAKE_CURRENT_BINARY_DIR}/libfcgi
			--includedir=${CMAKE_CURRENT_BINARY_DIR}/include/libfcgi
			--libdir=${CMAKE_CURRENT_BINARY_DIR}/lib
			--disable-wall
			--disable-werror
			CFLAGS=-w
		BUILD_COMMAND make clean && make all
	)

    # FCGI needs these warnings suppressed: "-Wunused-const-variable -Wint-to-void-pointer-cast -Wpointer-sign -Wstrict-aliasing=0

	add_library(fcgi STATIC IMPORTED GLOBAL)
	set_target_properties(fcgi PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/libfcgi.a)
	set_target_properties(fcgi PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/include/libfcgi)
	add_dependencies(fcgi ext_libfcgi)

	add_library(fcgipp STATIC IMPORTED GLOBAL)
	set_target_properties(fcgipp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/libfcgi++.a)
	add_dependencies(fcgipp ext_libfcgi)

endif()
