# dekaf2 dockerfile
#
# this dockerfile builds dekaf2 on the latest fedora base image
# it takes no config arguments

# run environment
RUN yum install -y openssl-libs zlib bzip2-libs mariadb-connector-c libzip xz-libs libzstd \
 libbrotli jemalloc glibc-langpack-en

FROM dekaf2-lib-environment as build-stage

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

# install
RUN make install

FROM dekaf2-lib-environment as dekaf2-run-environment

COPY --from=build-stage /usr/local/bin/klog           /usr/local/bin/klog
COPY --from=build-stage /usr/local/bin/createdbc      /usr/local/bin/createdbc
COPY --from=build-stage /usr/local/bin/dekaf2project  /usr/local/bin/dekaf2project
COPY --from=build-stage /usr/local/bin/kurl           /usr/local/bin/kurl
COPY --from=build-stage /usr/local/bin/mysql-newuser  /usr/local/bin/mysql-newuser
COPY --from=build-stage /usr/local/bin/findcol        /usr/local/bin/findcol
COPY --from=build-stage /usr/local/bin/kport          /usr/local/bin/kport
COPY --from=build-stage /usr/local/bin/my-ip-addr     /usr/local/bin/my-ip-addr
COPY --from=build-stage /usr/local/include/dekaf2-2.0 /usr/local/include/dekaf2-2.0
COPY --from=build-stage /usr/local/lib/dekaf2-2.0     /usr/local/lib/dekaf2-2.0
COPY --from=build-stage /usr/local/share/dekaf2-2.0   /usr/local/share/dekaf2-2.0
