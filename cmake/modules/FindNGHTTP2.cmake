
find_path(NGHTTP2_INCLUDE_DIR NAMES nghttp2/nghttp2.h)
find_library(NGHTTP2_LIBRARY NAMES nghttp2)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGHTTP2 DEFAULT_MSG NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR)

if(NOT NGHTTP2_FOUND)
        set(NGHTTP2_LIBRARY)
        set(NGHTTP2_INCLUDE_DIR)
else()
        set(NGHTTP2_LIBRARIES "${NGHTTP2_LIBRARY}")
        set(NGHTTP2_INCLUDE_DIRS "${NGHTTP2_INCLUDE_DIR}")
endif()
