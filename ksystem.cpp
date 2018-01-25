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

#include "ksystem.h"
#include "kstring.h"
#include "klog.h"

#include <sys/types.h>    // for getpwuid()
#include <pwd.h>          // for getpwuid()

namespace dekaf2
{

//-----------------------------------------------------------------------------
KString kGetCWD ()
//-----------------------------------------------------------------------------
{
#ifdef USE_STD_FILESYSTEM
	return fs::current_path().string();
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
KString kGetWhoami ()
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
	if (!GetUserName (szWhoami, &nSize)) {
		sWhoami = "ERROR";
	}
	else {
		sWhoami.Format ("{}", szWhoami);
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
		kDebugLog (2, "kGetWhoami(): used new method");
		sWhoami.Format ("{}", pPassStruct->pw_name);
	}
	#endif

	kDebugLog (2, "kGetWhoami(): {}", sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KString kGetHostname (KString& sHostname)
//-----------------------------------------------------------------------------
{
	KString sHostName;

	enum {MAX = 50};
	char szHostname[MAX+1];
	bool bStatus = true;

	#ifdef WIN32
	*szHostname = 0;

	DWORD nSize = MAX;
	GetComputerName (
		szHostname,         // name buffer
		&nSize              // address of size of name buffer
	);

	sHostname = szHostname;

	if (sHostname.empty()) {
		sHostname = "hostname-error";
	}

	#else
	if (gethostname (szHostname, sizeof (szHostname)) != 0) {
		kDebugLog (1, "CMD ERROR: hostname");
		sHostname = "hostname-error";
		bStatus = false;
	}
	else {
		sHostname = szHostname;
	}
	#endif

	kDebugLog (2, "kGetHostname(): %s", sHostname);

	return (sHostname);

} // kGetHostname

} // end of namespace dekaf2

