# - Find UUID
# Find the native UUID includes and library
# This module defines
#  UUID_INCLUDE_DIR, where to find jpeglib.h, etc.
#  UUID_LIBRARIES, the libraries needed to use UUID.
#  UUID_FOUND, If false, do not try to use UUID.
# also defined, but not for general use are
#  UUID_LIBRARY, where to find the UUID library.
#
#  Copyright (c) 2006-2016 Mathieu Malaterre <mathieu.malaterre@gmail.com>
#  Modified  (c) 2025 Ridgeware
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

# On Mac OS X the uuid functions are in the System library.
if(APPLE)
	set(UUID_LIBRARY_VAR System)
else()
	# Linux type:
	set(UUID_LIBRARY_VAR uuid)
endif()

find_library(UUID_LIBRARY
	NAMES ${UUID_LIBRARY_VAR}
	PATHS /lib /usr/lib /usr/local/lib
)

# Must be *after* the lib itself
set(CMAKE_FIND_FRAMEWORK_SAVE ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK NEVER)

find_path(UUID_INCLUDE_DIR uuid/uuid.h
  /usr/local/include
  /usr/include
)

# there is no version number for the UUID lib

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	UUID
	REQUIRED_VARS UUID_LIBRARY UUID_INCLUDE_DIR
)

if (UUID_FOUND)
	set(UUID_LIBRARIES "${UUID_LIBRARY}")
	set(UUID_INCLUDE_DIRS "${UUID_INCLUDE_DIR}")
else ()
	unset(UUID_LIBRARY)
	unset(UUID_INCLUDE_DIR)
endif ()

mark_as_advanced(
	UUID_LIBRARY
	UUID_INCLUDE_DIR
)

unset(UUID_LIBRARY_VAR)
set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_SAVE})
