#!/bin/bash

# Updates the vendored CLDR data (plurals.json, ordinals.json, likelySubtags.json)
# from the npm package cldr-core - the data only, no ICU. Then regenerates the
# headers with generate.sh and records the package version in VERSION.txt.
# Needs only bash, curl, tar and standard POSIX tools.
#
# usage: update.sh [version]     version = an npm version of cldr-core, default latest

PACKAGE="cldr-core"
DATA="plurals.json ordinals.json likelySubtags.json"
VERSION="${1:-latest}"
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

# the registry answers with one line of JSON, the tarball URL is its field "dist":{"tarball":"..."}
TARBALL=$(curl -sL "https://registry.npmjs.org/${PACKAGE}/${VERSION}" | sed -n 's/.*"tarball": *"\([^"]*\)".*/\1/p') || Die "cannot query the npm registry"
[[ -n "${TARBALL}" ]] || Die "no tarball for ${PACKAGE} ${VERSION}"

# the tarball is named PACKAGE-VERSION.tgz
PKGVERSION="${TARBALL##*/}"
PKGVERSION="${PKGVERSION#${PACKAGE}-}"
PKGVERSION="${PKGVERSION%.tgz}"
[[ -n "${PKGVERSION}" ]] || Die "cannot read the version from ${TARBALL}"

curl -sL -o "${TMP}/${PACKAGE}.tgz" "${TARBALL}" || Die "cannot download ${TARBALL}"
tar -xzf "${TMP}/${PACKAGE}.tgz" -C "${TMP}"      || Die "cannot unpack the package"

[[ -s "${TMP}/package/LICENSE" ]] || Die "missing LICENSE in the package"

CHANGED=""

for FILE in ${DATA}; do
	[[ -s "${TMP}/package/supplemental/${FILE}" ]] || Die "missing ${FILE} in the package"
	cmp --silent "${FILE}" "${TMP}/package/supplemental/${FILE}" || CHANGED="yes"
done

[[ -n "${CHANGED}" ]] || CleanupNoChange

echo "have new data"

for FILE in ${DATA}; do
	# keep the previous version for a diff, like the other vendored files
	diff -u "${FILE}" "${TMP}/package/supplemental/${FILE}" > "${FILE%.json}.diff"
	mv "${FILE}" "${FILE}.last"                          || Die "cannot move ${FILE}"
	cp "${TMP}/package/supplemental/${FILE}" "${FILE}"    || Die "cannot copy ${FILE}"
done

cp "${TMP}/package/LICENSE" LICENSE || Die "cannot copy LICENSE"

echo "${PACKAGE} ${PKGVERSION}, $(date +%Y-%m-%d)" > VERSION.txt

./generate.sh || Die "cannot generate the headers"

Cleanup
echo "updated to ${PACKAGE} ${PKGVERSION}"
