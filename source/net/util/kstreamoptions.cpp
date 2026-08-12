/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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
 */

#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/logging/klog.h>

#include <algorithm>

#if !DEKAF2_IS_WINDOWS
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
#else
	#include <Winsock2.h>
	#include <ws2tcpip.h>
#endif

DEKAF2_NAMESPACE_BEGIN

KStreamOptions::Options KStreamOptions::s_DefaultOptions =
#if DEKAF2_HAS_NGHTTP2 && DEKAF2_HAS_NGHTTP3_NOTYET
RequestHTTP2 | FallBackToHTTP1 | RequestHTTP3;
#elif DEKAF2_HAS_NGHTTP2
RequestHTTP2 | FallBackToHTTP1;
#elif DEKAF2_HAS_NGHTTP3
RequestHTTP3;
#else
None;
#endif

//-----------------------------------------------------------------------------
KStreamOptions::KStreamOptions(Options Options, KDuration Timeout)
//-----------------------------------------------------------------------------
: m_Timeout(Timeout)
, m_Options(GetDefaults(Options))
{
} // ctor

//-----------------------------------------------------------------------------
KStreamOptions::KStreamOptions(KDuration Timeout)
//-----------------------------------------------------------------------------
: m_Timeout(Timeout)
, m_Options(Options::None)
{
} // ctor

//-----------------------------------------------------------------------------
KStreamOptions& KStreamOptions::Set(Options Options)
//-----------------------------------------------------------------------------
{
	m_Options |= GetDefaults(Options);
	return *this;

} // Set

//-----------------------------------------------------------------------------
KStreamOptions& KStreamOptions::Unset(Options Options)
//-----------------------------------------------------------------------------
{
	m_Options &= ~GetDefaults(Options);
	return *this;

} // Unset

//-----------------------------------------------------------------------------
KStreamOptions::Options KStreamOptions::GetDefaults(Options Options)
//-----------------------------------------------------------------------------
{
	if (Options & DefaultsForHTTP)
	{
		Options &= ~DefaultsForHTTP;
		Options |= s_DefaultOptions;
	}

	return Options;

} // GetTLSDefaults

//-----------------------------------------------------------------------------
KStreamOptions& KStreamOptions::SetDeadPeerDetection(KDuration tWithin)
//-----------------------------------------------------------------------------
{
	constexpr int64_t iProbes { 3 };

	auto iTotal = std::max<int64_t>(2, tWithin.seconds().count());
	auto iIdle  = std::max<int64_t>(1, iTotal / 2);
	auto iIntvl = std::max<int64_t>(1, (iTotal - iIdle) / iProbes);

	m_KeepAliveInterval      = chrono::seconds(iIdle);
	m_KeepAliveProbeInterval = chrono::seconds(iIntvl);
	m_iKeepAliveProbeCount   = iProbes;
	m_ConnectionDropTimeout  = chrono::seconds(iTotal);

	return *this;

} // SetDeadPeerDetection

//-----------------------------------------------------------------------------
bool KStreamOptions::ApplySocketOptions(int socket, bool bIgnoreIfDefault)
//-----------------------------------------------------------------------------
{
	bool bReturn { true };

	if (!bIgnoreIfDefault || GetKeepAliveInterval() > KDuration::zero())
	{
		if (!kSetTCPKeepAliveInterval(socket, GetKeepAliveInterval()))
		{
			bReturn = false;
		}
	}

	if (GetKeepAliveProbeInterval() > KDuration::zero() || GetKeepAliveProbeCount() > 0)
	{
		if (!kSetTCPKeepAliveProbes(socket, GetKeepAliveProbeInterval(), GetKeepAliveProbeCount()))
		{
			bReturn = false;
		}
	}

	if (GetConnectionDropTimeout() > KDuration::zero())
	{
		if (!kSetTCPConnectionDropTimeout(socket, GetConnectionDropTimeout()))
		{
			bReturn = false;
		}
	}

	if (!bIgnoreIfDefault || GetLingerTimeout() > KDuration::zero())
	{
		if (!kSetLingerTimeout(socket, GetLingerTimeout()))
		{
			bReturn = false;
		}
	}

	return bReturn;

} // ApplySocketOptions

//-----------------------------------------------------------------------------
bool KStreamOptions::SetDefaults(Options Options)
//-----------------------------------------------------------------------------
{
	s_DefaultOptions = GetDefaults(Options);
	return true;

} // SetTLSDefaults

//-----------------------------------------------------------------------------
KStreamOptions::Family KStreamOptions::GetFamily() const
//-----------------------------------------------------------------------------
{
	if      (IsSet(ForceIPv4)) return Family::IPv4;
	else if (IsSet(ForceIPv6)) return Family::IPv6;
	else                       return Family::Any;

} // GetFamily

//-----------------------------------------------------------------------------
int KStreamOptions::GetNativeFamily() const
//-----------------------------------------------------------------------------
{
	switch (GetFamily())
	{
		case Family::IPv4:
			return AF_INET;
		case Family::IPv6:
			return AF_INET6;
		case Family::Any:
			return AF_UNSPEC;
	}

	return AF_UNSPEC;

} // GetNativeFamily

//-----------------------------------------------------------------------------
bool KStreamOptions::AddALPNString(KStringRef& sResult, KStringView sALPN)
//-----------------------------------------------------------------------------
{
	if (sALPN.size() < 256)
	{
		sResult.append(1, sALPN.size());
		sResult.append(sALPN);
		return true;
	}
	else
	{
		kDebug(2, "dropping ALPN value > 255 chars: {}..", sALPN.LeftUTF8(30));
		return false;
	}

} // AddALPNString

//-----------------------------------------------------------------------------
KString KStreamOptions::CreateALPNString(const std::vector<KStringView> ALPNs)
//-----------------------------------------------------------------------------
{
	KString sResult;

	for (auto sOne : ALPNs)
	{
		AddALPNString(sResult, sOne);
	}

	return sResult;

} // CreateALPNString

//-----------------------------------------------------------------------------
KString KStreamOptions::CreateALPNString(KStringView sALPN)
//-----------------------------------------------------------------------------
{
	KString sResult;

	AddALPNString(sResult, sALPN);

	return sResult;

} // CreateALPNString

//-----------------------------------------------------------------------------
KDuration kGetTCPKeepAliveInterval(int socket)
//-----------------------------------------------------------------------------
{
	// first check if keepalive is enabled
	int iInt { 0 };
	::socklen_t iSize { sizeof(iInt) };

#if DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&iInt), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &iInt, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get SO_KEEPALIVE from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	if (!iInt)
	{
		return KDuration::zero();
	}

	iInt = 0;
	iSize = sizeof(iInt);

#if DEKAF2_IS_MACOS
	constexpr int iOption { TCP_KEEPALIVE };
#else
	constexpr int iOption { TCP_KEEPIDLE };
#endif

#if DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, iOption, reinterpret_cast<char*>(&iInt), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, iOption, &iInt, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get TCP_KEEPIDLE from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	return chrono::seconds(iInt);

} // kGetTCPKeepAliveInterval

//-----------------------------------------------------------------------------
KDuration kGetLingerTimeout(int socket)
//-----------------------------------------------------------------------------
{
	struct ::linger linger;
	::socklen_t iSize { sizeof(linger) };

#if DEKAF2_IS_MACOS
	constexpr int iOption { SO_LINGER_SEC };
#else
	constexpr int iOption { SO_LINGER };
#endif

#ifdef DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, SOL_SOCKET, iOption, reinterpret_cast<char*>(&linger), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, SOL_SOCKET, iOption, &linger, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get SO_LINGER from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	if (!linger.l_onoff)
	{
		return KDuration::zero();
	}

	return chrono::seconds(linger.l_linger);

} // kGetLingerTimeout

//-----------------------------------------------------------------------------
bool kSetTCPKeepAliveInterval(int socket, KDuration tKeepaliveInterval)
//-----------------------------------------------------------------------------
{
	int iSeconds { static_cast<int>(tKeepaliveInterval.seconds().count()) };
	int iOnOff   { iSeconds > 0 ? 1 : 0 };

#ifdef DEKAF2_IS_WINDOWS
	if (-1 == ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&iOnOff), sizeof(iOnOff)))
#else
	if (-1 == ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &iOnOff, sizeof(iOnOff)))
#endif
	{
		kDebug(1, "cannot set SO_KEEPALIVE to {} on fd {}: {}", iOnOff, socket, strerror(errno));
		return false;
	}

	if (iOnOff)
	{
#if DEKAF2_IS_MACOS
		constexpr int iOption { TCP_KEEPALIVE };
#else
		constexpr int iOption { TCP_KEEPIDLE };
#endif

		// set interval
#ifdef DEKAF2_IS_WINDOWS
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, iOption, reinterpret_cast<const char*>(&iSeconds), sizeof(iSeconds)))
#else
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, iOption, &iSeconds, sizeof(iSeconds)))
#endif
		{
			kDebug(1, "cannot set TCP_KEEPIDLE to {} on fd {}: {}", iSeconds, socket, strerror(errno));
			return false;
		}
	}

	kDebug(3, "set TCP_KEEPIDLE to {}s on fd {}", iSeconds, socket);

	return true;

} // kSetTCPKeepAliveInterval

//-----------------------------------------------------------------------------
KDuration kGetTCPKeepAliveProbeInterval(int socket)
//-----------------------------------------------------------------------------
{
#ifdef TCP_KEEPINTVL
	int iInt { 0 };
	::socklen_t iSize { sizeof(iInt) };

#ifdef DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<char*>(&iInt), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &iInt, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get TCP_KEEPINTVL from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	return chrono::seconds(iInt);
#else
	kDebug(2, "TCP_KEEPINTVL is not available on this platform");
	return KDuration::zero();
#endif

} // kGetTCPKeepAliveProbeInterval

//-----------------------------------------------------------------------------
uint16_t kGetTCPKeepAliveProbeCount(int socket)
//-----------------------------------------------------------------------------
{
#ifdef TCP_KEEPCNT
	int iInt { 0 };
	::socklen_t iSize { sizeof(iInt) };

#ifdef DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<char*>(&iInt), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &iInt, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get TCP_KEEPCNT from fd {}: {}", socket, strerror(errno));
		return 0;
	}

	return static_cast<uint16_t>(iInt);
#else
	kDebug(2, "TCP_KEEPCNT is not available on this platform");
	return 0;
#endif

} // kGetTCPKeepAliveProbeCount

//-----------------------------------------------------------------------------
bool kSetTCPKeepAliveProbes(int socket, KDuration tProbeInterval, uint16_t iProbeCount)
//-----------------------------------------------------------------------------
{
	bool bReturn { true };

	int iSeconds { static_cast<int>(tProbeInterval.seconds().count()) };

	if (iSeconds > 0)
	{
#ifdef TCP_KEEPINTVL
#ifdef DEKAF2_IS_WINDOWS
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const char*>(&iSeconds), sizeof(iSeconds)))
#else
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &iSeconds, sizeof(iSeconds)))
#endif
		{
			kDebug(1, "cannot set TCP_KEEPINTVL to {} on fd {}: {}", iSeconds, socket, strerror(errno));
			bReturn = false;
		}
		else
		{
			kDebug(3, "set TCP_KEEPINTVL to {}s on fd {}", iSeconds, socket);
		}
#else
		kDebug(2, "TCP_KEEPINTVL is not available on this platform");
#endif
	}

	if (iProbeCount > 0)
	{
#ifdef TCP_KEEPCNT
		int iCount { iProbeCount };

#ifdef DEKAF2_IS_WINDOWS
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const char*>(&iCount), sizeof(iCount)))
#else
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &iCount, sizeof(iCount)))
#endif
		{
			kDebug(1, "cannot set TCP_KEEPCNT to {} on fd {}: {}", iCount, socket, strerror(errno));
			bReturn = false;
		}
		else
		{
			kDebug(3, "set TCP_KEEPCNT to {} on fd {}", iCount, socket);
		}
#else
		kDebug(2, "TCP_KEEPCNT is not available on this platform");
#endif
	}

	return bReturn;

} // kSetTCPKeepAliveProbes

//-----------------------------------------------------------------------------
KDuration kGetTCPConnectionDropTimeout(int socket)
//-----------------------------------------------------------------------------
{
#if defined(TCP_USER_TIMEOUT)
	// linux, in milliseconds
	unsigned int iValue { 0 };
	::socklen_t  iSize  { sizeof(iValue) };

	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &iValue, &iSize) || !iSize)
	{
		kDebug(1, "cannot get TCP_USER_TIMEOUT from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	return chrono::milliseconds(iValue);
#elif defined(TCP_RXT_CONNDROPTIME)
	// macOS, in seconds
	int iValue { 0 };
	::socklen_t iSize { sizeof(iValue) };

	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_RXT_CONNDROPTIME, &iValue, &iSize) || !iSize)
	{
		kDebug(1, "cannot get TCP_RXT_CONNDROPTIME from fd {}: {}", socket, strerror(errno));
		return KDuration::zero();
	}

	return chrono::seconds(iValue);
#else
	kDebug(2, "no connection drop timeout on this platform");
	return KDuration::zero();
#endif

} // kGetTCPConnectionDropTimeout

//-----------------------------------------------------------------------------
bool kSetTCPConnectionDropTimeout(int socket, KDuration tDrop)
//-----------------------------------------------------------------------------
{
	if (tDrop <= KDuration::zero()) return true;

#if defined(TCP_USER_TIMEOUT)
	// linux, in milliseconds
	unsigned int iValue { static_cast<unsigned int>(tDrop.milliseconds().count()) };

	if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &iValue, sizeof(iValue)))
	{
		kDebug(1, "cannot set TCP_USER_TIMEOUT to {} on fd {}: {}", iValue, socket, strerror(errno));
		return false;
	}

	kDebug(3, "set TCP_USER_TIMEOUT to {}ms on fd {}", iValue, socket);
#elif defined(TCP_RXT_CONNDROPTIME)
	// macOS, in seconds
	int iValue { static_cast<int>(tDrop.seconds().count()) };

	if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_RXT_CONNDROPTIME, &iValue, sizeof(iValue)))
	{
		kDebug(1, "cannot set TCP_RXT_CONNDROPTIME to {} on fd {}: {}", iValue, socket, strerror(errno));
		return false;
	}

	kDebug(3, "set TCP_RXT_CONNDROPTIME to {}s on fd {}", iValue, socket);
#else
	// best effort: there is no equivalent option on this platform (Windows)
	kDebug(2, "no connection drop timeout on this platform");
#endif

	return true;

} // kSetTCPConnectionDropTimeout

//-----------------------------------------------------------------------------
bool kSetLingerTimeout(int socket, KDuration tLingerTimeout)
//-----------------------------------------------------------------------------
{
	int iSeconds { static_cast<int>(tLingerTimeout.seconds().count()) };
	int iOnOff   { iSeconds > 0 ? 1 : 0 };

	struct ::linger linger;
	linger.l_onoff  = iOnOff;
	linger.l_linger = iSeconds;

#if DEKAF2_IS_MACOS
	constexpr int iOption { SO_LINGER_SEC };
#else
	constexpr int iOption { SO_LINGER };
#endif

#if DEKAF2_IS_WINDOWS
	if (-1 == ::setsockopt(socket, SOL_SOCKET, iOption, reinterpret_cast<const char*>(&linger), sizeof(linger)))
#else
	if (-1 == ::setsockopt(socket, SOL_SOCKET, iOption, &linger, sizeof(linger)))
#endif
	{
		kDebug(1, "cannot set SO_LINGER to {} with {}s on fd {}: {}", iOnOff, iSeconds, socket, strerror(errno));
		return false;
	}

	kDebug(3, "set SO_LINGER to {}s on fd {}", iSeconds, socket);

	return true;

} // kSetLingerTimeout

//-----------------------------------------------------------------------------
bool kGetTCPNoDelay(int socket)
//-----------------------------------------------------------------------------
{
	int iInt { 0 };
	::socklen_t iSize { sizeof(iInt) };

#if DEKAF2_IS_WINDOWS
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&iInt), &iSize) || !iSize)
#else
	if (-1 == ::getsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &iInt, &iSize) || !iSize)
#endif
	{
		kDebug(1, "cannot get TCP_NODELAY from fd {}: {}", socket, strerror(errno));
		return false;
	}

	return iInt != 0;

} // kGetTCPNoDelay

//-----------------------------------------------------------------------------
bool kSetTCPNoDelay(int socket, bool bYesNo)
//-----------------------------------------------------------------------------
{
	int iOnOff { bYesNo ? 1 : 0 };

#if DEKAF2_IS_WINDOWS
	if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&iOnOff), sizeof(iOnOff)))
#else
	if (-1 == ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &iOnOff, sizeof(iOnOff)))
#endif
	{
		kDebug(1, "cannot set TCP_NODELAY to {} on fd {}: {}", iOnOff, socket, strerror(errno));
		return false;
	}

	kDebug(3, "set TCP_NODELAY to {} on fd {}", iOnOff, socket);

	return true;

} // kSetTCPNoDelay

#if DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KDuration KStreamOptions::s_DefaultTimeout;
constexpr KDuration KStreamOptions::s_DefaultKeepAliveInterval;
constexpr KDuration KStreamOptions::s_DefaultLingerTimeout;
#endif

DEKAF2_NAMESPACE_END
