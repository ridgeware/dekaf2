if (EXISTS /opt/jemalloc-prof/)
	# this is our own build with profiling enabled
	find_path(JEMalloc_INCLUDE_DIR NO_DEFAULT_PATH PATHS /opt/jemalloc-prof/include NAMES jemalloc/jemalloc.h )
	find_library(JEMalloc_LIBRARY NO_DEFAULT_PATH PATHS /opt/jemalloc-prof/lib NAMES jemalloc )
else()
	# search a stock library
	find_path(JEMalloc_INCLUDE_DIR NAMES jemalloc/jemalloc.h)
	find_library(JEMalloc_LIBRARY NAMES jemalloc)
endif()

if(JEMalloc_INCLUDE_DIR AND EXISTS "${JEMalloc_INCLUDE_DIR}/jemalloc/jemalloc.h")
	file(STRINGS "${JEMalloc_INCLUDE_DIR}/jemalloc/jemalloc.h" JEMALLOC_HEADER_CONTENTS REGEX "#define JEMALLOC_VERSION_[A-Z]+ +[0-9]+")
	string(REGEX REPLACE ".*#define JEMALLOC_VERSION_MAJOR +([0-9]+).*" "\\1" JEMALLOC_VERSION_MAJOR "${JEMALLOC_HEADER_CONTENTS}")
	string(REGEX REPLACE ".*#define JEMALLOC_VERSION_MINOR +([0-9]+).*" "\\1" JEMALLOC_VERSION_MINOR "${JEMALLOC_HEADER_CONTENTS}")
	string(REGEX REPLACE ".*#define JEMALLOC_VERSION_BUGFIX +([0-9]+).*" "\\1" JEMALLOC_VERSION_BUGFIX "${JEMALLOC_HEADER_CONTENTS}")
	set(JEMALLOC_VERSION_STRING "${JEMALLOC_VERSION_MAJOR}.${JEMALLOC_VERSION_MINOR}.${JEMALLOC_VERSION_BUGFIX}")
	set(JEMALLOC_VERSION ${JEMALLOC_VERSION_STRING})
	unset(JEMALLOC_HEADER_CONTENTS)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	JEMalloc
	HANDLE_VERSION_RANGE
	REQUIRED_VARS JEMalloc_LIBRARY JEMalloc_INCLUDE_DIR
	VERSION_VAR JEMALLOC_VERSION
)

if(NOT JEMalloc_FOUND)
	unset(JEMalloc_LIBRARY)
	unset(JEMalloc_INCLUDE_DIR)
else()
	set(JEMalloc_LIBRARIES "${JEMalloc_LIBRARY}")
	set(JEMalloc_INCLUDE_DIRS "${JEMalloc_INCLUDE_DIR}")
endif()
