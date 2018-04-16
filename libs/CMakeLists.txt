cmake_minimum_required(VERSION 3.2)

# project(libs)

include(ExternalProject)

# add the format lib
add_subdirectory(fmt-4.0.0)

# add fast regular expressions
if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]"))
	# re2 has a number of missing field initializers and unused parameters.
	# we assume this is on purpose and disable the warning / error.
	set(CXX_FLAGS_STASHED ${CMAKE_CXX_FLAGS})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-unused-parameter")
endif()
add_subdirectory(re2)
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_CXX_FLAGS ${CXX_FLAGS_STASHED})
endif()

# add the frozen lib (constexpr set and map)
add_subdirectory(frozen)

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
