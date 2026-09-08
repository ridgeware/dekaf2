# dekaf2-suite

ARG from

FROM ${from}

ARG buildtype="release"

# copy the source
COPY /benchmarks     /home/dekaf2/benchmarks

# create source and build directories
RUN mkdir -p /home/dekaf2/benchmarks/build/${buildtype}

# change into build dir
WORKDIR /home/dekaf2/benchmarks/build/${buildtype}

# create cmake setup
RUN cmake -DCMAKE_BUILD_TYPE="${buildtype}" ../../

# build test
RUN export CPUCORES=$(expr $(grep -Ei '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target benchmarks

# run tests on build
#RUN benchmarks

# run tests on exec
CMD ./benchmarks
