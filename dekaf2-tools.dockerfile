# dekaf2-tools dockerfile
#
# this dockerfile takes the arguments
# from          : the base image to build from
# to            : the base image to copy onto, defaults to ${from}
# buildtype     : either "release" or "debug", defaults to "release"
# build_options : additional cmake options for dekaf2
#
# to build a dekaf2 image

ARG from="dekaf2:alpine"
ARG to="runenv:alpine"

FROM ${from} AS build-stage

FROM runenv AS final

COPY --from=build-stage /usr/local/bin/klog           /usr/local/bin/klog
COPY --from=build-stage /usr/local/bin/createdbc      /usr/local/bin/createdbc
COPY --from=build-stage /usr/local/bin/kurl           /usr/local/bin/kurl
COPY --from=build-stage /usr/local/bin/khttp          /usr/local/bin/khttp
COPY --from=build-stage /usr/local/bin/kreplace       /usr/local/bin/kreplace
COPY --from=build-stage /usr/local/bin/kgrep          /usr/local/bin/kgrep
COPY --from=build-stage /usr/local/bin/ksql           /usr/local/bin/ksql

RUN groupadd -r dekaf2 && useradd -r -s /bin/false -g dekaf2 dekaf2

USER dekaf2
