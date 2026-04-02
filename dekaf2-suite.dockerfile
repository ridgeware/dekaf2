# dekaf2-suite
#
# Builds unit tests and smoke tests against an installed dekaf2 image.
#
# Usage (utests only):
#   docker run dekaf2-suite
#
# Usage with DB smoke tests (via docker-compose.smoketest.yml):
#   docker compose -f docker-compose.smoketest.yml up --build --abort-on-container-exit

ARG from

FROM ${from}

ARG buildtype="release"

# copy the entrypoint script
COPY /scripts/run-smoketests.sh /home/dekaf2/scripts/run-smoketests.sh
RUN chmod +x /home/dekaf2/scripts/run-smoketests.sh

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

ENV BUILDTYPE=${buildtype}

# default: run utests only. For DB smoke tests, override CMD with the entrypoint script
# (see docker-compose.smoketest.yml)
WORKDIR /home/dekaf2/utests/build/${buildtype}
CMD ["./dekaf2-utests"]
