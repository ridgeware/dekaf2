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
#include <sys/socket.h>

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
void KStreamOptions::Set(Options Options)
//-----------------------------------------------------------------------------
{
	m_Options |= GetDefaults(Options);

} // Set

//-----------------------------------------------------------------------------
void KStreamOptions::Unset(Options Options)
//-----------------------------------------------------------------------------
{
	m_Options &= ~GetDefaults(Options);

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

DEKAF2_NAMESPACE_END
