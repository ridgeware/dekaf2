# dekaf2 dockerfile
#
# this dockerfile takes the arguments
# from          : the base image to build from
# to            : the base image to copy onto, defaults to ${from}
# buildtype     : either "release" or "debug", defaults to "release"
# build_options : additional cmake options for dekaf2
#
# to build a dekaf2 image

ARG from="buildenv"
ARG to="${from}"

FROM ${from} AS build-stage

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

FROM ${to} AS final

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
