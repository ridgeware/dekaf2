#! /bin/bash
#
#%
#% folly-gen.sh: generator of our "minifolly" library from Facebook's
#% folly lib on github.
#%
#% usage: folly-gen.sh install target-directory patchfile
#%
#% where target-directory is the directory in which our minifolly
#% library sources will be installed (normally dekaf2/from/folly/folly)
#% and patchfile is a diff to adapt the sources to our build system
#%
#% The diff command generates the patchfile to be used for the install,
#% should you have made further modifications to the sources:
#%
#% usage: folly-gen.sh diff original-directory target-directory patchfile
#%

# are we in test mode?
if [ "$1" = "-x" ]; then
	set -x
	shift
fi # end check test mode

sSources="
SharedMutex.cpp
SharedMutex.h
concurrency/CacheLocality.h
concurrency/CacheLocality.cpp
Indestructible.h
portability/Asm.h
portability/Config.h
portability/SysTypes.h
portability/Unistd.cpp
portability/Unistd.h
portability/Windows.h
portability/IOVec.h
portability/SysResource.cpp
portability/SysResource.h
portability/SysTime.cpp
portability/SysTime.h
portability/BitsFunctexcept.cpp
portability/BitsFunctexcept.h
portability/Memory.cpp
portability/Memory.h
portability/Malloc.cpp
portability/Malloc.h
portability/SysSyscall.h
portability/PThread.cpp
portability/PThread.h
portability/Builtins.cpp
portability/Builtins.h
CPortability.h
CppAttributes.h
Bits.h
detail/BitIteratorDetail.h
detail/Futex.h
detail/Futex.cpp
detail/MallocImpl.cpp
detail/MallocImpl.h
FBString.h
Malloc.h
FormatTraits.h
FBVector.h
Likely.h
Portability.h
Hash.h
ApplyTuple.h
Utility.h
hash/SpookyHashV1.cpp
hash/SpookyHashV1.h
hash/SpookyHashV2.cpp
hash/SpookyHashV2.h
ThreadId.h
Assume.h
Assume.cpp
Memory.h
Traits.h
"

setx=Synopsis
#==================
Synopsis ()
#==================
{
	awk '/^#%/ {print substr($0,3)}' "${ME_FULLPATH}"

} # Synopsis

setx=SetupColors
#==============================
SetupColors ()
#==============================
{
	# Note, coloring possibility is tested for stderr and thus should not be used for stdout
	if test -t 2 && which tput >/dev/null 2>&1; then
		ncolors=$(tput colors)
		if test -n "$ncolors"; then
			xWarnColor=$((tput setf 6 || tput setaf 3) && (tput setb 0 || tput setab 0)) # yellow on black
			xErrorColor=$((tput setf 7 || tput setaf 7) && (tput setb 4 || tput setab 1)) # white on red
			xResetColor=$(tput sgr0)
		fi
	fi

} # SetupColors

setx=Info
#==================
Info ()
#==================
{
	# All input args are interpreted as info text to display

	echo ":: $(date +%T) : ${*:-unspecified}"

} # Info

setx=Error
#==================
Error ()
#==================
{
	# All input args are interpreted as error text to display

	echo "${xErrorColor}${ME}: $(date +%T) : ERROR: ${*:-unspecified}${xResetColor}" >&2

} # Error

setx=Die
#==================
Die ()
#==================
{
	# All input args are interpreted as error text to display

	local sMessage="${FUNCNAME[1]}: ${*}"
	[[ ${BASH_SUBSHELL} -eq 0 ]] && sMessage="${sMessage}. Aborting"
	Error "${sMessage}"

	exit 1

} # Die

setx=FollySetup
#==============================
FollySetup ()
#==============================
{
	sTargetDir="$1"

	sCurDir=$(pwd)

	test -d "${sTargetDir}" || Die "target directory ${sTargetDir} does not exist"

	TMPDIR="/tmp/folly-install";

	sudo rm -rf ${TMPDIR} \
		&& mkdir -p ${TMPDIR} \
		&& cd ${TMPDIR} \
			|| Die "cannot setup build dir ${TMPDIR}"

	wget https://github.com/facebook/folly/archive/master.zip \
		|| Die "cannot download from github"

	unzip -q master.zip || Die "cannot unzip ${TMPDIR}/master.zip"

	cd "folly-master/folly"   || Die "cannot cd into ${TMPDIR}/folly-master/folly"

	sCopyTo="$1"

	for sSource in ${sSources}
	do

		sDir=$(dirname "${sSource}")

		if [ "${sDir}" != "" ]; then

			mkdir -p "${sCopyTo}/${sDir}" \
				|| Die "cannot create dir ${sCopyTo}/${sDir}"

		fi

		cp -f "${sSource}" "${sCopyTo}/${sDir}" \
			|| Die "cannot copy file ${sSource} to ${sCopyTo}/${sDir}"

	done

	cd "${sCurDir}"    || Die "cannot cd to ${sCurDir}"

	rm -rf "${TMPDIR}" || Die "cannot remove ${TMPDIR}"

} # FollySetup

setx=FollyPatch
#==============================
FollyPatch ()
#==============================
{
	sTargetDir="$1"
	sPatch="$2"

	sCurDir=$(pwd)

	test -d "${sTargetDir}" || Die "target directory ${sTargetDir} does not exist"
	test -f "${sPatch}"     || Die "patchfile $sPatch does not exist"

	if [ $sPatch[0] != '/' ]; then
		sPatch="$(pwd)/${sPatch}"
	fi

	cd "${sTargetDir}"      || Die "cannot cd to ${sTargetDir}"

	patch -s -p0 < "${sPatch}" || Die "patch < ${sPatch} failed"

	cd "${sCurDir}"    || Die "cannot cd to ${sCurDir}"

} # FollyPatch

setx=FollyDiff
#==============================
FollyDiff ()
#==============================
{
	sOrigDir="$1"
	sTargetDir="$2"
	sPatch="$3"

	sCurDir=$(pwd)

	test -d "${sOrigDir}"   || Die "original directory ${sOrigDir} does not exist"
	test -d "${sTargetDir}" || Die "target directory ${sTargetDir} does not exist"
	test -f "${sPatch}" \
		&& cp -f "${sPatch}" "${sPatch}.bak" \
		|| Die "cannot create backup of ${sPatch}"

	if [ $sOrigDir[0] != '/' ]; then
		sOrigDir="$(pwd)/${sOrigDir}"
	fi

	if [ $sPatch[0] != '/' ]; then
		sPatch="$(pwd)/${sPatch}"
	fi

	cd "${sTargetDir}"      || Die "cannot cd to ${sTargetDir}"

	diff -ur "${sOrigDir}" ./ | grep -v "^Only in" > "${sPatch}"

	cd "${sCurDir}"    || Die "cannot cd to ${sCurDir}"

} # FollyDiff


#==================
setx=Main_Body
#==================

ME=$(basename "${BASH_SOURCE}")
ME_ONLYPATH="$(cd $(dirname ${BASH_SOURCE}) && pwd)"
ME_FULLPATH="$ME_ONLYPATH/$ME"

if [ $# -lt 3 ]; then
	Synopsis
	exit 1
fi

SetupColors

case "$1" in
	diff)
		FollyDiff "$2" "$3" "$4"
		;;
	patch)
		FollyPatch "$2" "$3"
		;;
	install)
		FollySetup "$2"
		FollyPatch "$2" "$3"
		;;
	installifmissing)
		test -f "$2/FBString.h" && exit 1
		FollySetup "$2"
		FollyPatch "$2" "$3"
		;;
	*)
		Synopsis
		exit 1
		;;
esac

exit 0

