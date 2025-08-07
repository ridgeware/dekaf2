find_path(FREETDS_INCLUDE_DIR NAMES ctpublic.h)
find_library(FREETDS_LIBRARIES_CT NAMES ct)
set(FREETDS_LIBRARIES ${FREETDS_LIBRARIES_CT})
# we only need libct, but if you'd like to use libdb or libodbc, include them like below
# find_library(FREETDS_LIBRARIES_SYBDB NAMES sybdb)
# set(FREETDS_LIBRARIES ${FREETDS_LIBRARIES} ${FREETDS_LIBRARIES_SYBDB})

# freetds has no version number in the headers..

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	FreeTDS
	REQUIRED_VARS FREETDS_LIBRARIES FREETDS_INCLUDE_DIR
)

if (NOT FREETDS_FOUND)
	unset(FREETDS_LIBRARIES)
	unset(FREETDS_INCLUDE_DIRS)
endif()
