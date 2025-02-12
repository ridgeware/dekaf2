find_path(FREETDS_INCLUDE_DIR NAMES ctpublic.h)
find_library(FREETDS_LIBRARIES_CT NAMES ct)
set(FREETDS_LIBRARIES ${FREETDS_LIBRARIES_CT})
# we only need libct, but if you'd like to use libdb or libodbc, include them like below
# find_library(FREETDS_LIBRARIES_SYBDB NAMES sybdb)
# set(FREETDS_LIBRARIES ${FREETDS_LIBRARIES} ${FREETDS_LIBRARIES_SYBDB})
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FreeTDS DEFAULT_MSG FREETDS_LIBRARIES FREETDS_INCLUDE_DIR)

if (NOT FREETDS_FOUND)
	set(FREETDS_LIBRARIES)
	set(FREETDS_INCLUDE_DIRS)
endif()
