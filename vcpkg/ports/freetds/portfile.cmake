# freetds - the libraries of FreeTDS, for dekaf2's KSQL (ct-lib)
#
# Takes the place of vcpkg's own freetds port, which is Windows only, stuck at
# 1.3.10, and depends on libmysql on Windows although FreeTDS uses nothing of
# MySQL - and libmysql cannot be installed next to libmariadb. This one builds
# the release tarball the way FreeTDS' own CI does: with CMake on Windows, with
# autotools everywhere else. Only the libraries: no apps, pool, server, ODBC
# driver or tests - the same as dekaf2's scripts/freetdsinstall.

vcpkg_download_distfile(ARCHIVE
    URLS "https://www.freetds.org/files/stable/freetds-${VERSION}.tar.gz"
    FILENAME "freetds-${VERSION}.tar.gz"
    SHA512 1d622affa6e42e5085bdf3f0e642bef37cfdeb661013af62d08bba4aecad09f4cac2860ac253a584b06432cb24183028a372721bcfcdd2a9802a6b0f6a463837
)

vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")

if(VCPKG_TARGET_IS_WINDOWS)
    # the CMake build has no switches for these, so take them out of the lists
    file(READ "${SOURCE_PATH}/CMakeLists.txt" lists)
    string(REPLACE "enable_testing()" "" lists "${lists}")
    foreach(dir IN ITEMS src/odbc src/apps src/server src/pool)
        string(REPLACE "add_subdirectory(${dir})" "" lists "${lists}")
    endforeach()
    file(WRITE "${SOURCE_PATH}/CMakeLists.txt" "${lists}")
    file(GLOB_RECURSE sublists "${SOURCE_PATH}/src/*/CMakeLists.txt")
    foreach(sublist IN LISTS sublists)
        file(READ "${sublist}" lists)
        string(REPLACE "add_subdirectory(unittests)" "" lists "${lists}")
        file(WRITE "${sublist}" "${lists}")
    endforeach()

    vcpkg_find_acquire_program(PERL)
    get_filename_component(PERL_PATH "${PERL}" DIRECTORY)
    vcpkg_add_to_path("${PERL_PATH}")
    vcpkg_add_to_path(PREPEND "${CURRENT_HOST_INSTALLED_DIR}/tools/gperf")

    vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
        FEATURES
            openssl WITH_OPENSSL
    )

    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        DISABLE_PARALLEL_CONFIGURE
        OPTIONS
            ${FEATURE_OPTIONS}
    )
    vcpkg_cmake_install()
    vcpkg_copy_pdbs()

    if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        # the build makes the DLLs regardless - keep the static libct and the
        # internal libraries it needs, drop the DLLs and their import libraries
        file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
        foreach(dir IN ITEMS "${CURRENT_PACKAGES_DIR}/lib" "${CURRENT_PACKAGES_DIR}/debug/lib")
            file(REMOVE "${dir}/ct.lib" "${dir}/sybdb.lib")
        endforeach()
    endif()
else()
    if("openssl" IN_LIST FEATURES)
        set(tls --with-openssl)
    else()
        set(tls --without-openssl)
    endif()

    # the configuration files are looked for where distributions keep them,
    # not in the package tree of the build machine
    vcpkg_make_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            ${tls}
            --without-gnutls
            --disable-apps
            --disable-server
            --disable-pool
            --disable-odbc
            --sysconfdir=/etc
    )
    vcpkg_make_install()
endif()

# no manual (autotools puts it into share/freetds), no configuration files
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    "${CURRENT_PACKAGES_DIR}/share/doc"
    "${CURRENT_PACKAGES_DIR}/share/man"
    "${CURRENT_PACKAGES_DIR}/etc"
    "${CURRENT_PACKAGES_DIR}/debug/etc"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING_LIB.txt")
