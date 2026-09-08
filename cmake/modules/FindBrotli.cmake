find_path(BROTLI_INCLUDE_DIR NAMES "brotli/decode.h")
find_library(BROTLICOMMON_LIBRARY NAMES brotlicommon)
find_library(BROTLIDEC_LIBRARY NAMES brotlidec)
find_library(BROTLIENC_LIBRARY NAMES brotlienc)
find_library(BROTLICOMMON_STATIC_LIBRARY NAMES brotlicommon-static)
find_library(BROTLIDEC_STATIC_LIBRARY NAMES brotlidec-static)
find_library(BROTLIENC_STATIC_LIBRARY NAMES brotlienc-static)

# brotli has no version number in its headers, only an API call

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	Brotli
	REQUIRED_VARS BROTLICOMMON_LIBRARY BROTLIDEC_LIBRARY BROTLIENC_LIBRARY BROTLI_INCLUDE_DIR
)

if(NOT BROTLI_FOUND)
	unset(BROTLICOMMON_LIBRARY)
	unset(BROTLIDEC_LIBRARY)
	unset(BROTLIENC_LIBRARY)
	unset(BROTLI_INCLUDE_DIR)
	set(BROTLI_STATIC_FOUND NOTFOUND)
else()
	# encoder and decoder call into common - a static link resolves archives in the
	# order given, so common has to come last
	set(BROTLI_LIBRARIES "${BROTLIENC_LIBRARY}" "${BROTLIDEC_LIBRARY}" "${BROTLICOMMON_LIBRARY}")
	set(BROTLI_INCLUDE_DIRS "${BROTLI_INCLUDE_DIR}")
	if (NOT BROTLICOMMON_STATIC_LIBRARY})
		set(BROTLI_STATIC_FOUND NOTFOUND)
	else()
		set(BROTLI_STATIC_FOUND OK)
		set(BROTLI_STATIC_LIBRARIES "${BROTLIENC_STATIC_LIBRARY}" "${BROTLIDEC_STATIC_LIBRARY}" "${BROTLICOMMON_STATIC_LIBRARY}")
	endif()
endif()
