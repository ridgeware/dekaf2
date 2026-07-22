#!/bin/bash

# Doxygen writes group MEMBERS (typedefs/using aliases, free functions,
# enums) into the navigation tree data with their fully qualified name,
# ignoring HIDE_SCOPE_NAMES - while classes in the same tree are correctly
# unqualified (verified with doxygen 1.17.0). The result is a mixed-style
# sidebar: "KString" next to "dekaf2::KCaseString".
#
# This strips the redundant "dekaf2::" display prefix from the tree data
# files so all entries read alike. Only the display-name position of each
# tree entry is touched - links and anchors stay as generated.
#
# usage: doxygen-fix-navtree-scope.sh <doxygen html output dir>

set -e

HTMLDIR="${1:?usage: $0 <doxygen html output dir>}"

files=()
for f in "${HTMLDIR}"/navtreedata.js "${HTMLDIR}"/group__*.js; do
	[ -f "$f" ] && files+=("$f")
done

if [ ${#files[@]} -gt 0 ]; then
	perl -pi -e 's/\[ "dekaf2::/[ "/g' "${files[@]}"
fi
