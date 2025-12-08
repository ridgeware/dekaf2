# dekaf2 dockerfile
#
# this dockerfile takes the arguments
# from          : the base image to build from
#
# to build a dekaf2 image

ARG from="alpine:latest"

FROM ${from} AS runenv

ENV TZ=Europe/Paris

# run environment
RUN --mount=type=cache,target=/var/cache/apk,sharing=locked \
  apk upgrade && \
  apk add libcrypto3 libssl3 zlib libbz2 mariadb-connector-c \
    libzip xz-libs zstd-libs brotli-libs sqlite-libs jemalloc \
    musl-locales musl-locales-lang tzdata bash nghttp2-libs nghttp3 \
    shadow yaml-cpp libuuid
# freetds

FROM runenv AS buildenv

# library dev environment:
RUN --mount=type=cache,target=/var/cache/apk,sharing=locked \
  apk add boost-dev boost-static openssl-dev zlib-dev bzip2-dev \
	mariadb-connector-c-dev sqlite-dev libzip-dev xz-dev zstd-dev brotli-dev \
    nghttp2-dev nghttp3-dev jemalloc-dev yaml-cpp-dev util-linux-dev
  # util-linux-dev provides the headers for libuuid

# build environment:
RUN --mount=type=cache,target=/var/cache/apk,sharing=locked \
  apk add cmake gcc g++ gdb git make findutils patch tar

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

COPY --from=build-stage /usr/local/bin/klog          \
                        /usr/local/bin/createdbc     \
                        /usr/local/bin/dekaf2project \
                        /usr/local/bin/kurl          \
                        /usr/local/bin/khttp         \
                        /usr/local/bin/kreplace      \
                        /usr/local/bin/kgrep         \
                        /usr/local/bin/ksql          \
                        /usr/local/bin/krypt         \
                        /usr/local/bin/mysql-newuser \
                        /usr/local/bin/findcol       \
                        /usr/local/bin/kport         \
                        /usr/local/bin/ktunnel       \
                        /usr/local/bin/my-ip-addr     /usr/local/bin/
COPY --from=build-stage /usr/local/include/dekaf2     /usr/local/include/dekaf2
COPY --from=build-stage /usr/local/lib/dekaf2         /usr/local/lib/dekaf2
COPY --from=build-stage /usr/local/share/dekaf2       /usr/local/share/dekaf2
