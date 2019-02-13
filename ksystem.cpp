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

#include <thread>
#include "bits/kfilesystem.h"
#include "ksystem.h"
#include "kstring.h"
#include "kfilesystem.h"
#include "klog.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
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

	kDebug (2, "{}", sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KStringViewZ kGetHostname ()
//-----------------------------------------------------------------------------
{
	enum { MAXNAMELEN = 50 };
	static char szHostname[MAXNAMELEN+1] = "";

	if (*szHostname)
	{
		// hostname already queried
		return szHostname;
	}

#ifdef WIN32
	*szHostname = 0;

	DWORD nSize = MAXNAMELEN;
	GetComputerName (
		szHostname,         // name buffer
		&nSize              // address of size of name buffer
	);

	if (!*szHostname)
#else
	if (gethostname (szHostname, sizeof (szHostname)) != 0)
#endif
	{
		kDebug (1, "cannot get hostname");
		return "hostname-error";
	}

	kDebug (2, "{}", szHostname);
	return szHostname;

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
uint8_t ksystem (KStringView sCommand, KString& sOutput)
//-----------------------------------------------------------------------------
{
	KString sTmp;
	sTmp.Format ("/tmp/ksystem{}_{}.out", kGetPid(), kGetTid());

	KString sWrapped;
	sWrapped.Format ("({} 2>&1) > {}", sCommand, sTmp);

	kDebug (3, "{}", sWrapped);

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	uint8_t iStatus = std::system (sWrapped.c_str());
	kDebug (3, "exit code: {}", iStatus);

	sOutput.clear();

	if (!kFileExists (sTmp))
	{
		kDebug (1, "outfile is missing: {}", sTmp);
	}
	else
	{
		kReadFile (sTmp, sOutput, true);
		kDebug (3, "output contained {} bytes", sOutput.size());
		kRemoveFile (sTmp);
	}

	return (iStatus);  // 0 => success

} // ksystem

//-----------------------------------------------------------------------------
uint8_t ksystem (KStringView sCommand)
//-----------------------------------------------------------------------------
{
	KString sWrapped;
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "/dev/null");

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	uint8_t iStatus = std::system (sWrapped.c_str());

	return (iStatus);  // 0 => success

} // ksystem

//-----------------------------------------------------------------------------
KString kResolveHost (KStringViewZ sHostname, bool bIPv4, bool bIPv6)
//-----------------------------------------------------------------------------
{
	KString sIPV4;
	KString sIPV6;
	KString sRet;

	boost::asio::io_service IOService;
	boost::asio::ip::tcp::resolver Resolver(IOService);
	boost::asio::ip::tcp::resolver::query query(sHostname.c_str(), "80");
	boost::system::error_code ec;
	auto hosts = Resolver.resolve(query, ec);

	if (ec)
	{
		kDebug (1, "{} --> FAILED : {}", sHostname, ec.message());
	}
	else
	{
#if (BOOST_VERSION < 106600)
		auto it = hosts;
		decltype(it) ie;
#else
		auto it = hosts.begin();
		auto ie = hosts.end();
#endif
		for (; it != ie; ++it)
		{
			if (it->endpoint().protocol() == boost::asio::ip::tcp::v4())
			{
				sIPV4 = it->endpoint().address().to_string();

				if (bIPv4)
				{
					break;
				}
			}
			else
			{
				sIPV6 = it->endpoint().address().to_string();

				if (bIPv6)
				{
					break;
				}
			}
		}

		if (bIPv4 && !sIPV4.empty())
		{
			// success
			kDebug (1, "{} --> {}", sHostname, sIPV4);
			sRet = std::move(sIPV4);
		}
		else if (bIPv6 && !sIPV6.empty())
		{
			// success
			kDebug (1, "{} --> {}", sHostname, sIPV6);
			sRet = std::move(sIPV6);
		}
		else if (sIPV4.empty() && sIPV6.empty())
		{
			// unknown..
			kDebug(1, "{} --> FAILED", sHostname);
		}
		else if (sIPV4.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV6 {}", sHostname, sIPV6);
		}
		else if (sIPV6.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV4 {}", sHostname, sIPV4);
		}
	}

	return sRet;

} // kResolveHostIPV4

//-----------------------------------------------------------------------------
uint64_t kRandom(uint64_t iMin, uint64_t iMax)
//-----------------------------------------------------------------------------
{
	uint64_t iDiff = iMax - iMin;
	return iMin + (rand() % iDiff);

} // kRandom

//-----------------------------------------------------------------------------
void kSleepRandomMilliseconds (uint64_t iMin, uint64_t iMax)
//-----------------------------------------------------------------------------
{
	uint64_t iSleep = iMin;

	if (iMax > iMin)
	{
		iSleep = kRandom(iMin, iMax);
	}

	kDebug (2, "sleeping {} miliseconds...", iSleep);
	std::this_thread::sleep_for(std::chrono::milliseconds(iSleep));

} // kSleepRandomMilliseconds

} // end of namespace dekaf2

