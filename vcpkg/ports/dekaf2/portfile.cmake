# dekaf2 vcpkg portfile
#
# Two source modes:
#   * release  (default, target state for submission to microsoft/vcpkg):
#       fetches the tagged tarball from GitHub. Needs a real SHA512 below.
#   * local development testing:
#       set the DEKAF2_SOURCE_DIR environment variable to a working copy, e.g.
#         DEKAF2_SOURCE_DIR=/home/.../src/dekaf2 \
#           vcpkg install dekaf2 --overlay-ports=<repo>/vcpkg/ports
#       to build it without needing a release tag / SHA.

if(DEFINED ENV{DEKAF2_SOURCE_DIR} AND NOT "$ENV{DEKAF2_SOURCE_DIR}" STREQUAL "")
    set(SOURCE_PATH "$ENV{DEKAF2_SOURCE_DIR}")
    message(STATUS "dekaf2: building from local source tree ${SOURCE_PATH}")
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO ridgeware/dekaf2
        REF "v${VERSION}"
        SHA512 0  # TODO: replace with the real SHA512 of the v${VERSION} tarball before submission
        HEAD_REF master
    )
endif()

# feature -> dekaf2 cmake option. sqlite has no toggle (auto-detected via find_package(SQLite3));
# providing the sqlite3 dependency through the feature is enough to enable KSQLite.
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        mysql       DEKAF2_FIND_MYSQL
        postgres    DEKAF2_FIND_POSTGRESQL
        yaml        DEKAF2_FIND_YAML
        useragent   DEKAF2_WITH_USER_AGENT_PARSER
        http2       DEKAF2_FIND_NGHTTP2
        http3       DEKAF2_FIND_NGHTTP3
        zip         DEKAF2_FIND_ZIP
        freetds     DEKAF2_FIND_FREETDS
        compression DEKAF2_FIND_LZMA
        compression DEKAF2_FIND_ZSTD
        compression DEKAF2_FIND_BROTLI
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        # the package-mode switch; it already forces NO_BUILDSETUP, DUAL_TARGET=OFF,
        # VERSION_IN_TARGET=OFF, USE_SYSTEM_FMTLIB=ON and USE_VENDORED_*=OFF
        -DDEKAF2_VCPKG_BUILD=ON
        # only search for the libs the requested features pulled in
        -DDEKAF2_ALL_LIBS=OFF
        # a library port: no tools / samples
        -DDEKAF2_INSTALL_BIN=OFF
        -DDEKAF2_BUILD_SAMPLES=OFF
        # vcpkg drives static vs shared through the triplet; build static here
        -DDEKAF2_BUILD_STATIC_DEKAF2=ON
        -DDEKAF2_BUILD_SHARED_DEKAF2=OFF
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

# dekaf2 installs its package config into lib/dekaf2/cmake (DUAL_TARGET/VERSION_IN_TARGET are
# OFF in vcpkg mode); move it to the vcpkg-canonical share/dekaf2 location.
vcpkg_cmake_config_fixup(PACKAGE_NAME dekaf2 CONFIG_PATH lib/dekaf2/cmake)
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

# headers and cmake live in the release tree only
file(REMOVE_RECURSE
     "${CURRENT_PACKAGES_DIR}/debug/include"
     "${CURRENT_PACKAGES_DIR}/debug/share")
