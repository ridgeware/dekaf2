
find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZSTD DEFAULT_MSG ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

if(NOT ZSTD_FOUND)
        set(ZSTD_LIBRARY)
        set(ZSTD_INCLUDE_DIR)
else()
        set(ZSTD_LIBRARIES "${ZSTD_LIBRARY}")
        set(ZSTD_INCLUDE_DIRS "${ZSTD_INCLUDE_DIR}")
endif()
