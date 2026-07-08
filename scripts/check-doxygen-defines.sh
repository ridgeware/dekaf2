#!/bin/bash
#
# check-doxygen-defines.sh - drift guard for the Doxygen define profile.
#
# Doxygen documents the source with a curated set of DEKAF2_* feature macros
# (PREDEFINED in doc/doxyfile.in), not with any real build's kconfiguration.h.
# Every macro that kconfiguration.h.in can define must therefore be a conscious
# choice: either forced on (listed in PREDEFINED) or deliberately left off
# (listed in the DOXYGEN-DEFINE-PROFILE block). This script fails when a
# #cmakedefine exists that is in neither list, so a new feature switch cannot
# silently disappear from the generated documentation.
#
# Exit 0 when in sync, 1 on drift, 2 on a setup error.

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
kconf="$root/cmake/kconfiguration.h.in"
doxy="$root/doc/doxyfile.in"

for f in "$kconf" "$doxy"; do
	if [ ! -r "$f" ]; then
		echo "check-doxygen-defines: cannot read $f" >&2
		exit 2
	fi
done

# every macro kconfiguration.h.in can define
defined_by_cmake=$(grep -oE '#[[:space:]]*cmakedefine(01)?[[:space:]]+[A-Za-z0-9_]+' "$kconf" \
	| grep -oE '(DEKAF|DEKAF2)_[A-Za-z0-9_]+' | sort -u)

# macros forced on: the PREDEFINED logical line (with its \ continuations)
predefined=$(awk '
	$1 == "PREDEFINED" { inblock = 1 }
	inblock {
		print
		if ($0 !~ /\\[[:space:]]*$/) inblock = 0
	}
' "$doxy" | grep -oE '(DEKAF|DEKAF2)_[A-Za-z0-9_]+' | sort -u)

# macros deliberately left off: the profile comment block
profiled=$(awk '
	/DOXYGEN-DEFINE-PROFILE-BEGIN/ { inblock = 1; next }
	/DOXYGEN-DEFINE-PROFILE-END/   { inblock = 0 }
	inblock
' "$doxy" | grep -oE '(DEKAF|DEKAF2)_[A-Za-z0-9_]+' | sort -u)

known=$(printf '%s\n%s\n' "$predefined" "$profiled" | sort -u)

drift=$(comm -23 <(printf '%s\n' "$defined_by_cmake") <(printf '%s\n' "$known"))

if [ -n "$drift" ]; then
	echo "check-doxygen-defines: unclassified kconfiguration.h.in macro(s) -" >&2
	echo "  add each to PREDEFINED (force on) or the DOXYGEN-DEFINE-PROFILE block" >&2
	echo "  (off) in doc/doxyfile.in:" >&2
	printf '    %s\n' $drift >&2
	exit 1
fi

echo "check-doxygen-defines: doc/doxyfile.in is in sync with cmake/kconfiguration.h.in"
