# dekaf2 vcpkg portfile
#
# Two source modes:
#   * release  (default, target state for submission to microsoft/vcpkg):
#       fetches the tagged tarball from GitHub. Needs a real SHA512 below.
#   * local development testing:
#       set the DEKAF2_SOURCE_DIR environment variable to a working copy, e.g.
#         DEKAF2_SOURCE_DIR=/home/.../src/dekaf2 \
#           vcpkg install dekaf2 --overlay-ports=<repo>/vcpkg/ports
#       to build it without needing a release tag / SHA. Add --editable dekaf2:
#       the package hash does not cover a local tree, and without it the binary
#       cache hands out the build of an earlier state of the tree.

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

# dekaf2 is built as a static library only, also on dynamic triplets
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

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
        webview     DEKAF2_WITH_WEBVIEW
)

# the triplet decides the MSVC runtime, dekaf2 has to follow it
if(VCPKG_CRT_LINKAGE STREQUAL "static")
    set(DEKAF2_STATIC_RUNTIME ON)
else()
    set(DEKAF2_STATIC_RUNTIME OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        # the package-mode switch: no self-setup (NO_BUILDSETUP), relocatable export, and
        # the defaults of the layout and dependency options below
        -DDEKAF2_VCPKG_BUILD=ON
        # one configuration per triplet, in the unversioned layout
        -DDEKAF2_DUAL_TARGET=OFF
        -DDEKAF2_VERSION_IN_TARGET=OFF
        # fmt, simdutf and nlohmann-json from their ports (date stays vendored, it carries
        # dekaf2's std::chrono interop patches)
        -DDEKAF2_USE_SYSTEM_FMTLIB=ON
        -DDEKAF2_USE_VENDORED_SIMDUTF=OFF
        -DDEKAF2_USE_VENDORED_NLOHMANN=OFF
        # the MSVC runtime of the triplet (the option has no effect outside Windows)
        -DDEKAF2_FORCE_STATIC_BUILD_ON_WINDOWS=${DEKAF2_STATIC_RUNTIME}
        # only search for the libs the requested features pulled in
        -DDEKAF2_ALL_LIBS=OFF
        # a library port: no tools / samples
        -DDEKAF2_INSTALL_BIN=OFF
        -DDEKAF2_BUILD_SAMPLES=OFF
        # static only, see vcpkg_check_linkage() above
        -DDEKAF2_BUILD_STATIC_DEKAF2=ON
        -DDEKAF2_BUILD_SHARED_DEKAF2=OFF
        # no sanitizers in the debug library: vcpkg installs share/ of the release build
        # only, where they are off, so a consumer's debug link would lack their runtime
        -DDEKAF2_ENABLE_DEBUG_RUNTIME_CHECKS=OFF
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

# dekaf2 installs its package config into lib/dekaf2/cmake (DUAL_TARGET/VERSION_IN_TARGET are
# OFF in vcpkg mode); move it to the vcpkg-canonical share/dekaf2 location.
vcpkg_cmake_config_fixup(PACKAGE_NAME dekaf2 CONFIG_PATH lib/dekaf2/cmake)
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

# printed by vcpkg after install - documents the feature menu (see also: vcpkg search dekaf2)
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

# headers and cmake live in the release tree only
file(REMOVE_RECURSE
     "${CURRENT_PACKAGES_DIR}/debug/include"
     "${CURRENT_PACKAGES_DIR}/debug/share")
