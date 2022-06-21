# dekaf2-suite

ARG from

FROM ${from}

ARG buildtype="release"

# change into build dir
WORKDIR /home/dekaf2/build/${buildtype}

# build test
RUN export CPUCORES=$(expr $(egrep '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target dekaf2-utests dekaf2-smoketests

# run tests on build
#RUN utests/dekaf2-utests

# run test on exec
CMD utests/dekaf2-utests
