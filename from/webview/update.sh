#!/bin/bash

# Updates the vendored amalgamation of webview/webview (webview.h in this
# directory) to a git ref of https://github.com/webview/webview - default is
# master. Upstream has no releases, only tags and its amalgamation script, so
# this runs the script on a shallow checkout and records the commit in VERSION.txt.
#
# usage: update.sh [ref]     ref = branch, tag, or full commit SHA

REPO="https://github.com/webview/webview.git"
REF="${1:-master}"
TMP=""

Cleanup ()
{
	[[ -n "${TMP}" ]] && rm -rf "${TMP}"
}

Die ()
{
	Cleanup
	echo "${*} - abort"
	exit 1
}

CleanupNoChange ()
{
	Cleanup
	echo "data unchanged since last check"
	exit 2
}

# cd into script folder
cd $(dirname $(readlink -f "${BASH_SOURCE}")) || Die "cannot change into dir"

TMP=$(mktemp -d) || Die "cannot create temp dir"

# shallow fetch of the requested ref - fetching by name works for branches,
# tags, and (on github) full commit SHAs
git init -q "${TMP}/src"                               || Die "cannot init repo"
git -C "${TMP}/src" remote add origin "${REPO}"        || Die "cannot add remote"
git -C "${TMP}/src" fetch -q --depth 1 origin "${REF}" || Die "cannot fetch ${REF}"
git -C "${TMP}/src" checkout -q FETCH_HEAD             || Die "cannot checkout ${REF}"

SHA=$(git -C "${TMP}/src" rev-parse HEAD)
COMMITDATE=$(git -C "${TMP}/src" log -1 --format=%cs)

# run upstream's amalgamation script from the checkout, with the relative base
# directory of the README invocation - the script writes the paths relative to
# the base into "file begin/end" markers, an absolute base would leak the temp
# dir. It would also reformat the result with the local clang-format, which
# makes the output depend on the clang-format version - the sources are already
# formatted upstream, so we pass a no-op formatter.
(
	cd "${TMP}/src" &&
	python3 scripts/amalgamate/amalgamate.py \
		--base core \
		--search include \
		--clang-format-exe true \
		--output "${TMP}/webview.h" \
		src > /dev/null
) || Die "amalgamation failed"

[[ -s "${TMP}/webview.h" ]] || Die "no amalgamation created"

[[ -f webview.h ]] && cmp --silent webview.h "${TMP}/webview.h" && CleanupNoChange

echo "have new data"

if [[ -f webview.h ]]; then
	# create a diff and keep the previous version
	diff -u webview.h "${TMP}/webview.h" > webview.diff
	mv webview.h webview.h.last || Die "cannot move file webview.h"
fi

# move files into place
mv "${TMP}/webview.h" webview.h || Die "cannot move file webview.h"
cp "${TMP}/src/LICENSE" LICENSE || Die "cannot copy LICENSE"

# VERSION.txt, not VERSION: this directory is an include path, and on a case
# insensitive filesystem a file named VERSION shadows the standard <version> header
cat > VERSION.txt <<EOF
This tree is the amalgamation of webview/webview ${REF} @ ${SHA} (committed ${COMMITDATE}),
generated $(date +%Y-%m-%d) by update.sh with upstream's scripts/amalgamate/amalgamate.py
(--base core --search include src, without the clang-format pass).
EOF

Cleanup

if [[ -f webview.diff ]]; then
	# show diff
	less webview.diff
fi

echo "update done"
echo
exit 0
