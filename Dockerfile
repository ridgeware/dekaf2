FROM fedora:latest as dekaf2-lib-environment

# run environment
RUN yum install -y openssl zlib bzip2 mariadb libzip xz libzstd brotli jemalloc glibc-langpack-en

FROM dekaf2-lib-environment as dekaf2-build-environment

# library dev environment:
RUN yum install -y boost-devel boost-static openssl-devel zlib-devel bzip2-devel \
 mariadb-devel sqlite-devel libzip-devel xz-devel libzstd-devel brotli-devel jemalloc-devel

# build environment:
RUN yum install -y cmake gcc g++ rpm-build

# create source and build directories
RUN mkdir -p /home/dekaf2/build/Release

# copy the source
COPY . /home/dekaf2/

# change into build dir
WORKDIR /home/dekaf2/build/Release

# create cmake setup
RUN cmake -DCMAKE_BUILD_TYPE=Release -DDEKAF2_NO_BUILDSETUP=ON -DDEKAF2_USE_JEMALLOC=ON ../../

# build
RUN make -j4 all

# create RPM
RUN cpack -G RPM

FROM dekaf2-lib-environment as dekaf2-run-environment

# copy RPM
COPY --from=dekaf2-build-environment /home/dekaf2/build/Release/dekaf2-2.0.13-Linux.rpm /home/dekaf2/

# install
RUN rpm --install /home/dekaf2/dekaf2-2.0.13-Linux.rpm
