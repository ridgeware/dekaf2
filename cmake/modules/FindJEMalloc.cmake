
find_path(JEMalloc_INCLUDE_DIR NAMES jemalloc/jemalloc.h)
find_library(JEMalloc_LIBRARY NAMES jemalloc)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JEMalloc DEFAULT_MSG JEMalloc_LIBRARY JEMalloc_INCLUDE_DIR)

if(NOT JEMalloc_FOUND)
        set(JEMalloc_LIBRARY)
        set(JEMalloc_INCLUDE_DIR)
else()
        set(JEMalloc_LIBRARIES "${JEMalloc_LIBRARY}")
        set(JEMalloc_INCLUDE_DIRS "${JEMalloc_INCLUDE_DIR}")
endif()
