#!/bin/bash

Die ()
{
	rm -f simdutf.h.new simdutf.cpp.new
	echo "${*} - abort"
	exit 1
}

CleanupNoChange ()
{
	rm -f simdutf.h.new simdutf.cpp.new
	echo "data unchanged since last check"
	exit 2
}

# cd into script folder
cd $(dirname $(readlink -f "${BASH_SOURCE}")) || Die "cannot change into dir"

# update the simdutf.h/simdutf.cpp files from github:
kurl -s https://github.com/simdutf/simdutf/releases/latest/download/simdutf.h   -o simdutf.h.new   || Die "cannot download simdutf.h"
kurl -s https://github.com/simdutf/simdutf/releases/latest/download/simdutf.cpp -o simdutf.cpp.new || Die "cannot download simdutf.cpp"

[[ -s simdutf.cpp.new ]] || Die "no files downloaded"

cmp --silent simdutf.cpp simdutf.cpp.new && \
    cmp --silent simdutf.h simdutf.h.new && \
    CleanupNoChange

echo "have new data"

# create a diff
diff -u simdutf.h   simdutf.h.new    > simdutf.diff
diff -u simdutf.cpp simdutf.cpp.new >> simdutf.diff

# move files into place
mv simdutf.h simdutf.h.last     || Die "cannot move file simdutf.h"
mv simdutf.h.new simdutf.h      || Die "cannot move file simdutf.h.new"
mv simdutf.cpp simdutf.cpp.last || Die "cannot move file simdutf.cpp"
mv simdutf.cpp.new simdutf.cpp  || Die "cannot move file simdutf.cpp.new"

# show diff
less simdutf.diff

echo "update done"
echo
exit 0
