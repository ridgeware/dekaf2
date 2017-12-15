# - Try to find libproc
# Once done, this will define
#
#  libproc_FOUND - system has libproc
#  libproc_INCLUDE_DIRS - the libproc include directories
#  libproc_LIBRARIES - link these to use libproc

include(LibFindMacros)

# Dependencies
#libfind_package(Magick++ Magick)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(libproc_PKGCONF libproc)

# Include dir
find_path(libproc_INCLUDE_DIR
	NAMES libproc.h
	PATHS ${libproc_PKGCONF_INCLUDE_DIRS}
)

if(!${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	# Finally find the library itself - but not on OSX, as it is part of the
	# system SDK
	find_library(libproc_LIBRARY
		NAMES libproc
		PATHS ${libproc_PKGCONF_LIBRARY_DIRS}
	)
endif()

libfind_process(libproc)
