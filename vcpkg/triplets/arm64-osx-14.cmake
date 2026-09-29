# macOS on Apple silicon, static libraries, for Macs from macOS 14 on.
#
#   cmake -S . -B build/arm64-osx-14 -DDEKAF2_VCPKG_BUILD=ON \
#         -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
#         -DVCPKG_MANIFEST_DIR=$PWD/vcpkg \
#         -DVCPKG_TARGET_TRIPLET=arm64-osx-14 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
#         -DCMAKE_INSTALL_PREFIX=build/arm64-osx-14/install
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 14.0)
