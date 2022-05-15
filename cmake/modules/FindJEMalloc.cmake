if (EXISTS /opt/jemalloc-prof/)
	# this is our own build with profiling enabled
	find_path(JEMalloc_INCLUDE_DIR NO_DEFAULT_PATH PATHS /opt/jemalloc-prof/include NAMES jemalloc/jemalloc.h )
	find_library(JEMalloc_LIBRARY NO_DEFAULT_PATH PATHS /opt/jemalloc-prof/lib NAMES jemalloc )
else()
	# search a stock library
	find_path(JEMalloc_INCLUDE_DIR NAMES jemalloc/jemalloc.h)
	find_library(JEMalloc_LIBRARY NAMES jemalloc)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JEMalloc DEFAULT_MSG JEMalloc_LIBRARY JEMalloc_INCLUDE_DIR)

if(NOT JEMalloc_FOUND)
        set(JEMalloc_LIBRARY)
        set(JEMalloc_INCLUDE_DIR)
else()
        set(JEMalloc_LIBRARIES "${JEMalloc_LIBRARY}")
        set(JEMalloc_INCLUDE_DIRS "${JEMalloc_INCLUDE_DIR}")
endif()
