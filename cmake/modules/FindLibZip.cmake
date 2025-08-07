# keep in mind that also libtcod and minizip2 install a zip.h header
# uninstall these if you want to use libzip

find_path(LIBZIP_INCLUDE_DIR NAMES zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)

# libzip has no version information in its header (zip.h), but only
# an API function to request the version number

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	LibZip
	REQUIRED_VARS LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR
)

if(NOT LIBZIP_FOUND)
	unset(LIBZIP_LIBRARY)
	unset(LIBZIP_INCLUDE_DIR)
else()
	set(LIBZIP_LIBRARIES "${LIBZIP_LIBRARY}")
	set(LIBZIP_INCLUDE_DIRS "${LIBZIP_INCLUDE_DIR}")
endif()
