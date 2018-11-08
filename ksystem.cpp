/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include "bits/kfilesystem.h"
#include "ksystem.h"
#include "kstring.h"
#include "klog.h"

#include <sys/types.h>    // for getpwuid()
#include <pwd.h>          // for getpwuid()
#ifndef DEKAF2_IS_OSX
#include <sys/syscall.h>
#endif

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kSetCWD (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;
	fs::current_path(sPath.c_str(), ec);
	if (ec)
	{
		kWarning("cannot set current working directory: {}", ec.message());
		return false;
	}
	return true;
#else
	if (::chdir(sPath.c_str()))
	{
		kWarning("chdir failed: {}", ::strerror(errno));
		return false;
	}
	return true;
#endif

} // kSetCWD

//-----------------------------------------------------------------------------
KString kGetCWD ()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;
	KString sPath = fs::current_path(ec).string();
	if (ec)
	{
		kWarning("cannot get current working directory: {}", ec.message());
	}
	return sPath;
#else
	enum { MAX_PATH = 1024 };
	KString str(MAX_PATH, '\0');
	if (::getcwd(&str[0], str.size()-1))
	{
		size_t iPathLen = ::strlen(str.c_str());
		str.erase(iPathLen);
	}
	else
	{
		kWarning("cannot get current working directory: {}", ::strerror(errno));
		str.erase();
	}
	return str;
#endif

} // kGetCWD

//-----------------------------------------------------------------------------
KString kGetHome()
//-----------------------------------------------------------------------------
{
	// HOME var is always the authoritative source for the home directory
	KString sHome = kGetEnv("HOME");

	if (sHome.empty())
	{
		// only if it is empty check getpwuid()
		auto ent = getpwuid(geteuid());

		if (ent)
		{
			sHome = ent->pw_dir;
		}

		if (sHome.empty())
		{
			kWarning("cannot get home directory");
		}
	}

	return sHome;

} // kGetHome

//-----------------------------------------------------------------------------
KString kGetWhoAmI ()
//-----------------------------------------------------------------------------
{
	KString sWhoami;

#ifdef WIN32
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// WINDOWS:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	enum { MAX = 100 };
	char szWhoami[MAX + 1] = { 0 };
	DWORD nSize = MAX;
	if (GetUserName (szWhoami, &nSize))
	{
		sWhoami = szWhoami;
	}
#else
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// on UNIX, we do *not* cache the kGetWhoami() result, to allow
	// identity changes through setuid():
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	int iUID = getuid();
	struct passwd* pPassStruct = getpwuid (iUID);
	if (pPassStruct)
	{
		sWhoami = pPassStruct->pw_name;
	}
#endif

	kDebugLog (2, "kGetWhoAmI(): {}", sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KString kGetHostname (KString& sHostname)
//-----------------------------------------------------------------------------
{
	KString sHostName;

	enum {MAX = 50};
	char szHostname[MAX+1];

#ifdef WIN32
	*szHostname = 0;

	DWORD nSize = MAX;
	GetComputerName (
		szHostname,         // name buffer
		&nSize              // address of size of name buffer
	);

	sHostname = szHostname;

	if (sHostname.empty())
	{
		sHostname = "hostname-error";
	}
#else
	if (gethostname (szHostname, sizeof (szHostname)) != 0)
	{
		kDebugLog (1, "CMD ERROR: hostname");
		sHostname = "hostname-error";
	}
	else
	{
		sHostname = szHostname;
	}
#endif

	kDebugLog (2, "kGetHostname(): %s", sHostname);

	return (sHostname);

} // kGetHostname

//-----------------------------------------------------------------------------
uint64_t kGetTid()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_OSX
	uint64_t TID;
	pthread_threadid_np(nullptr, &TID);
	return TID;
#else
	return syscall(SYS_gettid);
#endif

} // kGetTid

//-----------------------------------------------------------------------------
uint8_t ksystem (const KString& sCommand, KString& sOutput)
//-----------------------------------------------------------------------------
{
	KString sTmp;
	sTmp.Format ("/tmp/ksystem{}.out", getpid());

	KString sWrapped;
	sWrapped.Format ("({} 2>&1) > {}", sCommand, sTmp);

	kDebugLog (3, "ksystem: {}", sWrapped);

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	uint8_t iStatus = system (sWrapped.c_str());
	kDebugLog (3, "ksystem: exit code: {}", iStatus);

	sOutput.clear();

	if (!kFileExists (sTmp))
	{
		kDebugLog (1, "ksystem: outfile is missing: {}", sTmp);
	}
	else
	{
		kReadFile (sTmp, sOutput);
		kDebugLog (3, "ksystem: output contained {} bytes", sOutput.size());
		kRemoveFile (sTmp);
	}

	return (iStatus);  // 0 => success

} // ksystem

//-----------------------------------------------------------------------------
uint8_t ksystem (const KString& sCommand)
//-----------------------------------------------------------------------------
{
	KString sWrapped;
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "/dev/null");

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	uint8_t iStatus = system (sWrapped.c_str());

	return (iStatus);  // 0 => success

} // ksystem

//-----------------------------------------------------------------------------
KString kResolveHostIPV4 (const KString& sHostname)
//-----------------------------------------------------------------------------
{
	KString sIPV4;
	struct hostent* pHost = gethostbyname (sHostname.c_str());
	if (!pHost)
	{
		kDebugLog (1, "kResolveHostIPV4: {} --> FAILED", sHostname);
	}
	else
	{
		sIPV4 = inet_ntoa (*(in_addr*)(pHost->h_addr_list[0]));
		kDebugLog (1, "kResolveHostIPV4: {} --> {}", sHostname, sIPV4);
	}

	return sIPV4;

} // kResolveHostIPV4

} // end of namespace dekaf2

