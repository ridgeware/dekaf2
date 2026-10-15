#!/bin/bash

# Updates the vendored single header fast_float.h of https://github.com/fastfloat/fast_float
# to the fast_float.h asset of a release - default is the latest release. Records
# the release in VERSION.txt and copies the MIT license of that release.
#
# usage: update.sh [tag]     tag = a release tag, e.g. v8.3.2

REPO="fastfloat/fast_float"
TAG="${1:-}"

Die ()
{
	rm -f fast_float.h.new LICENSE-MIT.new
	echo "${*} - abort"
	exit 1
}

CleanupNoChange ()
{
	rm -f fast_float.h.new LICENSE-MIT.new
	echo "data unchanged since last check"
	exit 2
}

# cd into script folder
cd $(dirname $(readlink -f "${BASH_SOURCE}")) || Die "cannot change into dir"

if [[ -z "${TAG}" ]]; then
	URL="https://github.com/${REPO}/releases/latest/download/fast_float.h"
else
	URL="https://github.com/${REPO}/releases/download/${TAG}/fast_float.h"
fi

kurl -s "${URL}" -o fast_float.h.new || Die "cannot download fast_float.h"

[[ -s fast_float.h.new ]] || Die "no file downloaded"

# the release the header belongs to, from its version macros
Version ()
{
	grep -E "^#define FASTFLOAT_VERSION_${1} [0-9]+" fast_float.h.new | awk '{ print $3 }'
}

VERSION="v$(Version MAJOR).$(Version MINOR).$(Version PATCH)"

[[ "${VERSION}" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || Die "cannot read the version of fast_float.h"
[[ -z "${TAG}" || "${TAG}" == "${VERSION}" ]] || Die "fast_float.h of ${TAG} says it is ${VERSION}"

kurl -s "https://raw.githubusercontent.com/${REPO}/${VERSION}/LICENSE-MIT" -o LICENSE-MIT.new || Die "cannot download LICENSE-MIT"

[[ -s LICENSE-MIT.new ]] || Die "no license downloaded"

[[ -f fast_float.h ]] && cmp --silent fast_float.h fast_float.h.new && CleanupNoChange

echo "have new data: ${VERSION}"

if [[ -f fast_float.h ]]; then
	# create a diff and keep the previous version
	diff -u fast_float.h fast_float.h.new > fast_float.diff
	mv fast_float.h fast_float.h.last || Die "cannot move file fast_float.h"
fi

# move files into place
mv fast_float.h.new fast_float.h || Die "cannot move file fast_float.h.new"
mv LICENSE-MIT.new LICENSE-MIT   || Die "cannot move file LICENSE-MIT.new"

# VERSION.txt, not VERSION: on a case insensitive filesystem a file named VERSION
# in an include path shadows the standard <version> header
cat > VERSION.txt <<EOF
This file is fast_float.h of the ${REPO} release ${VERSION}
(https://github.com/${REPO}/releases/tag/${VERSION}),
downloaded $(date +%Y-%m-%d) by update.sh.
EOF

if [[ -f fast_float.diff ]]; then
	# show diff
	less fast_float.diff
fi

echo "update done"
echo
exit 0
