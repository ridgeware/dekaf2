#!/bin/bash

Die ()
{
	echo "${*} - abort"
	exit 1
}

CleanupNoChange ()
{
	rm regexes.yaml.new
	echo "data unchanged since last check"
	exit 2
}

# cd into script folder
cd $(dirname $(readlink -f "${BASH_SOURCE}")) || Die "cannot change into dir"

# update the regexes.yaml file from github:
kurl https://raw.githubusercontent.com/ua-parser/uap-core/refs/heads/master/regexes.yaml -o regexes.yaml.new || Die "cannot download"

[[ -s regexes.yaml.new ]] || Die "no file downloaded"

cmp --silent regexes.yaml regexes.yaml.new && CleanupNoChange

echo "have new data"

# create a diff
diff -u regexes.yaml regexes.yaml.new > regexes.diff

# move files into place
mv regexes.yaml regexes.yaml.last || Die "cannot move file"
mv regexes.yaml.new regexes.yaml  || Die "cannot move file"

echo "***************** CHANGES *****************"
cat regexes.diff
echo "*******************************************"
echo
echo "update done"
echo
exit 0
