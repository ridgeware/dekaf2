#!/bin/bash

# Doxygen writes group MEMBERS (typedefs/using aliases, free functions,
# enums and their values) into the navigation tree data with their fully
# qualified name, ignoring HIDE_SCOPE_NAMES - while classes in the same
# tree are correctly unqualified (verified with doxygen 1.17.0). The
# result is a mixed-style sidebar: "KString" next to "dekaf2::KCaseString".
# The namespace also shows up inside template arguments of specializations
# ("hash< dekaf2::KHTMLObject >").
#
# This strips every "dekaf2::" from the display-name position of each tree
# entry so all entries read alike. Nested scopes below the namespace
# ("KHTMLObject::TagProperty") are kept; links and anchors stay as
# generated.
#
# usage: doxygen-fix-navtree-scope.sh <doxygen html output dir>

set -e

HTMLDIR="${1:?usage: $0 <doxygen html output dir>}"

files=()
for f in "${HTMLDIR}"/navtreedata.js "${HTMLDIR}"/group__*.js; do
	[ -f "$f" ] && files+=("$f")
done

if [ ${#files[@]} -gt 0 ]; then
	perl -pi -e 's/^(\s*\[ ")([^"]*)/$1 . ($2 =~ s{dekaf2::}{}gr)/e' "${files[@]}"
fi
