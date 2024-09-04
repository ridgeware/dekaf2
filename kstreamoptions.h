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

#pragma once

#include "kdefinitions.h"
#include "kduration.h"
#include "kstring.h"
#include "kstringview.h"
#include "ktime.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// maintaining options for internet streams: TLS settings, ALPN settings, protocol settings, timeout
class KStreamOptions {
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//------
public:
//------

	/// general stream options
	enum Options : uint8_t
	{
		None            = 0,      ///< no options, use for non-HTTP connections, or to restrict to HTTP1 connections
		VerifyCert      = 1 << 0, ///< verify server certificate
		ManualHandshake = 1 << 1, ///< wait for manual TLS handshake (for protocols like SMTP STARTTLS)
		RequestHTTP2    = 1 << 2, ///< request a ALPN negotiation for HTTP2
		FallBackToHTTP1 = 1 << 3, ///< if RequestHTTP2 is set, allow HTTP1 as fallback if 2 is not available
		RequestHTTP3    = 1 << 4, ///< request an ALPN negotiation for HTTP/3, using QUIC. This option cannot fallback to HTTP/2 or 1.1
		ForceIPv4       = 1 << 5, ///< force an IPv4 connection
		ForceIPv6       = 1 << 6, ///< force an IPv6 connection
		DefaultsForHTTP = 1 << 7, ///< use for HTTP, per default tries HTTP2 and allows HTTP1, can be changed through kSetTLSDefaults()
	};

	/// address family, used mainly for resolver setup
	enum Family : uint8_t
	{
		Any, IPv4, IPv6
	};

	/// ctor setting options (default = None) and timeout (default = GetDefaultTimeout())
	KStreamOptions(Options Options = None, KDuration Timeout = GetDefaultTimeout());
	/// ctor setting options and timeout as chrono::seconds
	KStreamOptions(Options Options, chrono::seconds Timeout)
	: KStreamOptions(Options, KDuration(Timeout)) {}
	/// ctor setting timeout. Options will be set to None
	KStreamOptions(KDuration Timeout);
	/// ctor setting timeout as chrono::seconds. Options will be set to None
	KStreamOptions(chrono::seconds Timeout)
	: KStreamOptions(KDuration(Timeout)) {}
	/// ctor setting verify mode. Other options will be set to None, timeout is GetDefaultTimeout()
	// make sure it really only gets called from bool
	template<typename T, typename std::enable_if<std::is_same<T, bool>::value, int>::type = 0>
	KStreamOptions(bool bVerify)
	: KStreamOptions(Options(bVerify ? VerifyCert : None)) {} ///< constructor to help legacy class interfaces

	/// adds the requested Options to the existing options
	void Set(Options Options);

	/// removes the requested Options from the existing options
	void Unset(Options Options);

	/// return the configured options
	Options Get()      const { return m_Options; }

	/// is the requested option (or set of options) set?
	bool IsSet(Options Options) const { return (Get() & Options) == Options; }

	/// returns one of Any, IPv4, IPv6 depending on the set options
	Family GetFamily()    const;

	/// returns the family as native value
	int GetNativeFamily() const;

	/// set the timeout
	void SetTimeout(KDuration Timeout) { m_Timeout = Timeout; }

	/// returns timeout set
	KDuration GetTimeout() const { return m_Timeout; }

	operator Options() const { return Get();     }

	/// transform DefaultsForHTTP so that they will be resolved to
	/// application wide defaults for HTTP (either with HTTP1 and/or HTTP2 and/or verify)
	static Options GetDefaults(Options Options);

	/// set the application wide defaults for HTTP - this function is not thread safe, set it right
	/// at the start of your application before threading out. Initial defaults are RequestHTTP2 | FallBackToHTTP1.
	/// Setting the DefaultsForHTTP bit will expand to the previous default settings and merge with any other
	/// given option.
	static bool SetDefaults(Options Options);

	/// returns the default timeout for streams
	static constexpr KDuration GetDefaultTimeout() { return s_DefaultTimeout; }

	/// create the ALPN data
	/// @param ALPNs a vector of KStringViews for requested protocols
	/// @return a string in the format expected by the ALPN APIs
	static KString CreateALPNString(const std::vector<KStringView> ALPNs);

	/// create the ALPN data
	/// @param sALPN a string view of the requested protocol
	/// @return a string in the format expected by the ALPN APIs
	static KString CreateALPNString(KStringView sALPN);

	/// add ALPN data to an existing ALPN string
	/// @param sALPN the existing ALPN string that shall be extended
	/// @param sAdd the new ALPN value to add to the existing string
	static bool AddALPNString(KStringRef& sResult, KStringView sALPN);

//------
private:
//------

	static Options s_DefaultOptions;
	static constexpr KDuration s_DefaultTimeout { chrono::seconds(15) };

	KDuration m_Timeout { s_DefaultTimeout };
	Options   m_Options { None             };

}; // KStreamOptions

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// options for HTTP streams - they have a different set of default options compared to KStreamOptions - the DefaultsForHTTP
class KHTTPStreamOptions : public KStreamOptions
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// ctor setting options (default = DefaultsForHTTP) and timeout (default = GetDefaultTimeout())
	KHTTPStreamOptions(Options Options = DefaultsForHTTP, KDuration Timeout = GetDefaultTimeout())
	: KStreamOptions(Options, Timeout) {}
	/// ctor setting options and timeout as chrono::seconds
	KHTTPStreamOptions(Options Options, chrono::seconds Timeout)
	: KHTTPStreamOptions(Options, KDuration(Timeout)) {}
	/// ctor setting timeout. Options will be set to DefaultsForHTTP
	KHTTPStreamOptions(KDuration Timeout)
	: KHTTPStreamOptions(DefaultsForHTTP, Timeout) {}
	/// ctor setting timeout as chrono::seconds. Options will be set to DefaultsForHTTP
	KHTTPStreamOptions(chrono::seconds Timeout)
	: KHTTPStreamOptions(KDuration(Timeout)) {}
	/// ctor setting verify mode. Other options will be set to DefaultsForHTTP, timeout is GetDefaultTimeout()
	// make sure it really only gets called from bool
	template<typename T, typename std::enable_if<std::is_same<T, bool>::value, int>::type = 0>
	KHTTPStreamOptions(T bVerify)
	: KHTTPStreamOptions(Options(bVerify ? VerifyCert | DefaultsForHTTP : DefaultsForHTTP)) {} ///< constructor to help legacy class interfaces

}; // KHTTPStreamOptions

DEKAF2_ENUM_IS_FLAG(KStreamOptions::Options)

using TLSOptions = KStreamOptions::Options;

inline bool kSetTLSDefaults(TLSOptions Options) { return KStreamOptions::SetDefaults(Options); }

DEKAF2_NAMESPACE_END
