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

#include "kstreamoptions.h"
#include "klog.h"

#if !DEKAF2_IS_WINDOWS
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
#else
	#include <Winsock2.h>
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

	if (-1 == ::getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &iInt, &iSize) || !iSize)
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

	if (-1 == ::getsockopt(socket, IPPROTO_TCP, iOption, &iInt, &iSize) || !iSize)
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

	if (-1 == ::getsockopt(socket, SOL_SOCKET, iOption, &linger, &iSize) || !iSize)
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

	if (-1 == ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &iOnOff, sizeof(iOnOff)))
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
		if (-1 == ::setsockopt(socket, IPPROTO_TCP, iOption, &iSeconds, sizeof(iSeconds)))
		{
			kDebug(1, "cannot set TCP_KEEPIDLE to {} on fd {}: {}", iSeconds, socket, strerror(errno));
			return false;
		}
	}

	kDebug(3, "set TCP_KEEPIDLE to {}s on fd {}", iSeconds, socket);

	return true;

} // kSetTCPKeepAliveInterval

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

	if (-1 == ::setsockopt(socket, SOL_SOCKET, iOption, &linger, sizeof(linger)))
	{
		kDebug(1, "cannot set SO_LINGER to {} with {}s on fd {}: {}", iOnOff, iSeconds, socket, strerror(errno));
		return false;
	}

	kDebug(3, "set SO_LINGER to {}s on fd {}", iSeconds, socket);

	return true;

} // kSetLingerTimeout

DEKAF2_NAMESPACE_END
