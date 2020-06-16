# keep in mind that also libtcod and minizip2 install a zip.h header
# uninstall these if you want to use libzip

find_path(LIBZIP_INCLUDE_DIR NAMES zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBZIP DEFAULT_MSG LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR)

if(LIBZIP_FOUND)
else()
	set(LIBZIP_LIBRARY)
	set(LIBZIP_INCLUDE_DIR)
endif()
