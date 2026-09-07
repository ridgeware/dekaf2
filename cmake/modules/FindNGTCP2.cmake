# finds ngtcp2 (QUIC transport) together with its OpenSSL crypto backend
# libngtcp2_crypto_ossl - the latter needs OpenSSL >= 3.5 (QUIC TLS API for
# third party QUIC stacks)

find_path(NGTCP2_INCLUDE_DIR NAMES ngtcp2/ngtcp2.h)
find_library(NGTCP2_LIBRARY NAMES ngtcp2)
find_library(NGTCP2_CRYPTO_OSSL_LIBRARY NAMES ngtcp2_crypto_ossl)

if(NGTCP2_INCLUDE_DIR AND EXISTS "${NGTCP2_INCLUDE_DIR}/ngtcp2/version.h")
	file(STRINGS "${NGTCP2_INCLUDE_DIR}/ngtcp2/version.h" NGTCP2_HEADER_CONTENTS REGEX "#define NGTCP2_VERSION +\"[0-9\.]+\"")
	string(REGEX REPLACE ".*#define NGTCP2_VERSION +\"([0-9\.]+)\".*" "\\1" NGTCP2_VERSION_STRING "${NGTCP2_HEADER_CONTENTS}")
	set(NGTCP2_VERSION ${NGTCP2_VERSION_STRING})
	unset(NGTCP2_HEADER_CONTENTS)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	NGTCP2
	REQUIRED_VARS NGTCP2_LIBRARY NGTCP2_CRYPTO_OSSL_LIBRARY NGTCP2_INCLUDE_DIR
	VERSION_VAR NGTCP2_VERSION
)

if(NOT NGTCP2_FOUND)
	unset(NGTCP2_LIBRARY)
	unset(NGTCP2_CRYPTO_OSSL_LIBRARY)
	unset(NGTCP2_INCLUDE_DIR)
else()
	# the crypto backend calls into the core lib: for a static link it has to come first
	set(NGTCP2_LIBRARIES "${NGTCP2_CRYPTO_OSSL_LIBRARY}" "${NGTCP2_LIBRARY}")
	set(NGTCP2_INCLUDE_DIRS "${NGTCP2_INCLUDE_DIR}")
endif()
