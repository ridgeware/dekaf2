
find_path(NGHTTP3_INCLUDE_DIR NAMES nghttp3/nghttp3.h)
find_library(NGHTTP3_LIBRARY NAMES nghttp3)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGHTTP3 DEFAULT_MSG NGHTTP3_LIBRARY NGHTTP3_INCLUDE_DIR)

if(NOT NGHTTP3_FOUND)
        set(NGHTTP3_LIBRARY)
        set(NGHTTP3_INCLUDE_DIR)
else()
        set(NGHTTP3_LIBRARIES "${NGHTTP3_LIBRARY}")
        set(NGHTTP3_INCLUDE_DIRS "${NGHTTP3_INCLUDE_DIR}")
endif()
