# dekaf2 OVERLAY of the stock vcpkg nlohmann-json port.
#
# Purpose: pin nlohmann-json to 3.12.0 and apply dekaf2's MSVC fix
# (fix-msvc-compatibletype-noexcept.patch) so the CompatibleType constructor's noexcept spec
# is hidden from MSVC, which otherwise hard-errors on it when string_t is not std::string.
#
# RECONCILE BEFORE USE: this is a draft modeled on the standard nlohmann-json port. Diff it
# against your installed <vcpkg>/ports/nlohmann-json/portfile.cmake and copy over the exact
# SHA512 + any options/usage this draft is missing. The only intentional difference from stock
# is the pinned REF v3.12.0 (so the patch context matches) and the added PATCHES line.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nlohmann/json
    REF "v${VERSION}"
    SHA512 0  # TODO: copy the real SHA512 for v3.12.0 from the stock nlohmann-json port
    HEAD_REF master
    PATCHES
        fix-msvc-compatibletype-noexcept.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DJSON_BuildTests=OFF
        -DJSON_Install=ON
        -DJSON_MultipleHeaders=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME nlohmann_json CONFIG_PATH share/cmake/nlohmann_json)

file(REMOVE_RECURSE
     "${CURRENT_PACKAGES_DIR}/debug"
     "${CURRENT_PACKAGES_DIR}/lib")

file(INSTALL "${SOURCE_PATH}/LICENSE.MIT"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
