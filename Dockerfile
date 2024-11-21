# dekaf2 dockerfile
#
# this dockerfile takes the arguments
# from          : the base image to build from
#
# to build a dekaf2 image

ARG from="alpine:latest"

FROM ${from} AS runenv

ENV TZ=Europe/Paris

RUN apk upgrade --no-cache

# run environment
RUN apk add --no-cache libcrypto3 libssl3 zlib libbz2 mariadb-connector-c \
 libzip xz-libs zstd-libs brotli-libs sqlite-libs jemalloc \
 musl-locales musl-locales-lang tzdata bash nghttp2-libs nghttp3

FROM runenv AS buildenv

# library dev environment:
RUN apk add boost-dev boost-static openssl-dev zlib-dev bzip2-dev \
 mariadb-connector-c-dev sqlite-dev libzip-dev xz-dev zstd-dev brotli-dev \
 nghttp2-dev nghttp3-dev

# build environment:
RUN apk add cmake gcc g++ gdb git make findutils patch tar

ENV CC=gcc
ENV CXX=g++

FROM buildenv AS build-stage

ARG buildtype="release"
ARG build_options=""
ARG parallel=""

# create source and build directories
RUN mkdir -p /home/dekaf2/build/${buildtype}

# copy the source
COPY . /home/dekaf2/

# change into build dir
WORKDIR /home/dekaf2/build/${buildtype}

# create cmake setup
RUN cmake \
  -DCMAKE_BUILD_TYPE="${buildtype}" \
  ${build_options} \
  -DDEKAF2_NO_BUILDSETUP=ON \
  -DDEKAF2_USE_JEMALLOC=ON \
  ../../

# build
RUN [ "${parallel}" != "" ] \
  && export CPUCORES="${parallel}" \
  || export CPUCORES=$(expr $(grep -Ei '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
  cmake --build . --parallel ${CPUCORES} --target all

# install
#RUN cmake --install .
RUN make install && /usr/local/bin/kurl -V

FROM runenv as final

COPY --from=build-stage /usr/local/bin/klog           /usr/local/bin/klog
COPY --from=build-stage /usr/local/bin/createdbc      /usr/local/bin/createdbc
COPY --from=build-stage /usr/local/bin/dekaf2project  /usr/local/bin/dekaf2project
COPY --from=build-stage /usr/local/bin/kurl           /usr/local/bin/kurl
COPY --from=build-stage /usr/local/bin/khttp          /usr/local/bin/khttp
COPY --from=build-stage /usr/local/bin/kreplace       /usr/local/bin/kreplace
COPY --from=build-stage /usr/local/bin/kgrep          /usr/local/bin/kgrep
COPY --from=build-stage /usr/local/bin/mysql-newuser  /usr/local/bin/mysql-newuser
COPY --from=build-stage /usr/local/bin/findcol        /usr/local/bin/findcol
COPY --from=build-stage /usr/local/bin/kport          /usr/local/bin/kport
COPY --from=build-stage /usr/local/bin/my-ip-addr     /usr/local/bin/my-ip-addr
COPY --from=build-stage /usr/local/include/dekaf2     /usr/local/include/dekaf2
COPY --from=build-stage /usr/local/lib/dekaf2         /usr/local/lib/dekaf2
COPY --from=build-stage /usr/local/share/dekaf2       /usr/local/share/dekaf2
