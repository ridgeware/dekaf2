# dekaf2-suite

ARG from

FROM ${from}

ARG buildtype="release"

# copy the source
COPY /utests     /home/dekaf2/utests

# create source and build directories
RUN mkdir -p /home/dekaf2/utests/build/${buildtype}

# change into build dir
WORKDIR /home/dekaf2/utests/build/${buildtype}

# create cmake setup
RUN cmake -DCMAKE_BUILD_TYPE="${buildtype}" ../../

# build test
RUN export CPUCORES=$(expr $(grep -Ei '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target dekaf2-utests

COPY /smoketests /home/dekaf2/smoketests

# create source and build directories
RUN mkdir -p /home/dekaf2/smoketests/build/${buildtype}

# change into build dir
WORKDIR /home/dekaf2/smoketests/build/${buildtype}

# create cmake setup
RUN cmake -DCMAKE_BUILD_TYPE="${buildtype}" ../../

RUN export CPUCORES=$(expr $(grep -Ei '^BogoMIPS' /proc/cpuinfo | wc -l) + 1); \
    cmake --build . --parallel ${CPUCORES} --target dekaf2-smoketests

# change back into utests build dir
WORKDIR /home/dekaf2/utests/build/${buildtype}

# run tests on build
#RUN dekaf2-utests

# run tests on exec
CMD ./dekaf2-utests
