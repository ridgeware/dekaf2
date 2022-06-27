# dekaf2-suite

ARG from

FROM ${from}

ARG buildtype="release"

# copy the source
COPY . /home/dekaf2/

# create source and build directories
RUN mkdir -p /home/dekaf2/build/${buildtype}

# change into build dir
WORKDIR /home/dekaf2/build/${buildtype}

# create cmake setup
RUN cmake -DCMAKE_BUILD_TYPE="${buildtype}" -DDEKAF2_NO_BUILDSETUP=ON -DDEKAF2_USE_JEMALLOC=ON ../../

# build
RUN export CPUCORES=$(expr $(egrep '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target all

# build test
RUN export CPUCORES=$(expr $(egrep '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target dekaf2-utests

RUN export CPUCORES=$(expr $(egrep '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target dekaf2-smoketests

# run tests on build
#RUN utests/dekaf2-utests

# run tests on exec
CMD utests/dekaf2-utests
