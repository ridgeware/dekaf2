/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include "kcookie.h"
#include "klog.h"
#include "keraseremove.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KCookie::IsValidName(KStringView sName)
//-----------------------------------------------------------------------------
{
	// https://stackoverflow.com/a/1969339
	for (auto ch : sName)
	{
		if (!KASCII::kIsAlNum(ch))
		{
			switch (ch)
			{
				case '!':
				case '#':
				case '$':
				case '%':
				case '&':
				case '*':
				case '+':
				case '-':
				case '.':
				case '\'':
				case '^':
				case '_':
				case '`':
				case '|':
				case '~':
					break;
				default:
					return false;
			}
		}
	}

	return true;

} // IsValidName

//-----------------------------------------------------------------------------
bool KCookie::IsValidValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	// https://stackoverflow.com/a/1969339
	for (auto ch : sValue)
	{
		if (!KASCII::kIsAlNum(ch))
		{
			switch (ch)
			{
				case '!':
				case '#':
				case '$':
				case '%':
				case '&':
				case '*':
				case '+':
				case '-':
				case '.':
				case '\'':
				case '^':
				case '_':
				case '`':
				case '|':
				case '~':
				case '(':
				case ')':
				case '/':
				case ':':
				case '<':
				case '=':
				case '>':
				case '?':
				case '@':
				case '[':
				case ']':
				case '{':
				case '}':
					break;
				default:
					return false;
			}
		}
	}

	return true;

} // IsValidValue

//-----------------------------------------------------------------------------
void KCookie::clear()
//-----------------------------------------------------------------------------
{
	m_sName.clear();
	m_sValue.clear();
	m_sPath.clear();
	m_Domain.clear();
	m_bOnlyHTTPS = false;

} // clear

//-----------------------------------------------------------------------------
bool KCookie::Parse(const KURL& URL, KStringView sInput)
//-----------------------------------------------------------------------------
{
	clear();
	
	bool bFirst             { true  };
	bool bNeedSecureKeyword { false };
	bool bMustNotHaveDomain { false };
	bool bHadDomain         { false };

	m_Domain = URL.Domain;

	for (auto Pair : sInput.Split<std::vector<KStringViewPair>>(";", "="))
	{
		if (bFirst)
		{
			// this is the name-value pair for the cookie itself
			bFirst = false;

			if (Pair.first.remove_prefix("__Secure-"))
			{
				bNeedSecureKeyword = true;
			}
			else if (Pair.first.remove_prefix("__Host-"))
			{
				bNeedSecureKeyword = true;
				bMustNotHaveDomain = true;
			}

			if (!IsValidName(Pair.first))
			{
				kDebug(1, "invalid name: {}", Pair.first);
				return false;
			}

			if (!IsValidValue(Pair.second))
			{
				kDebug(1, "invalid value: {}", Pair.second);
				return false;
			}

			m_sName  = Pair.first;
			m_sValue = Pair.second;
		}
		else
		{
			switch (Pair.first.Hash())
			{
				case "Expires"_hash:
					// we ignore it as we only take session cookies
					break;

				case "Max-Age"_hash:
					// we ignore it as we only take session cookies
					break;

				case "Domain"_hash:
					// we treat all cookies as to be sent to the current domain only..
					if (kGetBaseDomain(m_Domain) == kGetBaseDomain(Pair.second))
					{
						m_bAllowSubDomains = true;
						m_Domain = Pair.second;
						kDebug(2, "allowing subdomains for {}", Pair.second);
					}
					else
					{
						kDebug(2, "rejecting cookie set for different domain than origin: {}", Pair.second);
						clear();
						return false;
					}
					bHadDomain = true;
					break;

				case "Path"_hash:
					if (Pair.second.front() == '/')
					{
						m_sPath = Pair.second;
					}
					else
					{
						kDebug(1, "invalid path: {}", Pair.second);
						clear();
						return false;
					}
					break;

				case "Secure"_hash:
					if (URL.Protocol != url::KProtocol::HTTPS)
					{
						// a Secure cookie may only be set with a secure connection
						kDebug(1, "rejecting cookie with Secure flag set via insecure connection: {}", Pair.second);
						clear();
						return false;
					}
					bNeedSecureKeyword = false;
					m_bOnlyHTTPS = true;
					break;

				case "SameSite"_hash:
					// we treat all cookies as Strict..
					break;

				default:
					// HttpOnly ..
					break;
			}
		}
	}

	if (bNeedSecureKeyword)
	{
		kDebug(1, "missing secure keyword: {}", sInput);
		clear();
		return false;
	}

	if (bMustNotHaveDomain && bHadDomain)
	{
		kDebug(1, "Domain is not allowed: {}", sInput);
		clear();
		return false;
	}

	if (!m_sName.empty())
	{
		kDebug(2, "accepting cookie: {}={} for {}{}", m_sName, m_sValue, m_Domain.Serialize(), m_sPath);
		return true;
	}
	else
	{
		return false;
	}

} // Parse

//-----------------------------------------------------------------------------
bool KCookie::WouldSerialize(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	if (m_bOnlyHTTPS && URL.Protocol != url::KProtocol::HTTPS)
	{
		return false;
	}

	if (!m_Domain.empty())
	{
		if (URL.Domain != m_Domain)
		{
			if (!m_bAllowSubDomains || !kIsSubDomainOf(m_Domain, URL.Domain))
			{
				return false;
			}
		}
	}

	if (!m_sPath.empty())
	{
		if (!URL.Path.get().starts_with(m_sPath))
		{
			return false;
		}
	}

	return !m_sName.empty();

} // WouldSerialize

//-----------------------------------------------------------------------------
KString KCookie::Serialize() const
//-----------------------------------------------------------------------------
{
	// a cookie with empty name just emits the value, not the = in front of it
	KString sCookie { m_sName };

	if (!m_sValue.empty())
	{
		if (!sCookie.empty())
		{
			sCookie += '=';
		}
		sCookie += m_sValue;
	}

	return sCookie;

} // Serialize

//-----------------------------------------------------------------------------
KString KCookie::Serialize(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	if (!WouldSerialize(URL))
	{
		return {};
	}

	return Serialize();

} // Serialize

//-----------------------------------------------------------------------------
bool KCookie::empty() const
//-----------------------------------------------------------------------------
{
	// name OR value may be empty
	return m_sName.empty() && m_sValue.empty();
}

//-----------------------------------------------------------------------------
bool KCookie::Write(KOutStream& Out) const
//-----------------------------------------------------------------------------
{
	Out.FormatLine(
		"{}\t{}\t{}\t{}\t{}\t{}\t{}",
		m_Domain.Serialize(),
		m_bAllowSubDomains ? "TRUE" : "FALSE",
		m_sPath,
		m_bOnlyHTTPS ? "TRUE" : "FALSE",
		0, // TBD, expiry
		m_sName,
		m_sValue
	);

	return Out.Good();

} // Write

//-----------------------------------------------------------------------------
bool KCookie::Read(KInStream& In)
//-----------------------------------------------------------------------------
{
	for (auto sLine : In)
	{
		sLine.Trim();

		if (sLine.empty() || sLine.front() == '#')
		{
			continue;
		}

		auto Parts = sLine.Split("\t", "");

		if (Parts.size() != 7)
		{
			kDebug(1, "invalid input: {}", sLine);
			return false;
		}

		m_Domain.Parse(Parts[0]);
		m_bAllowSubDomains = Parts[1] == "TRUE";
		m_sPath            = Parts[2];
		m_bOnlyHTTPS       = Parts[3] != "FALSE";
		// TBD expiry
		m_sName            = Parts[5];
		m_sValue           = Parts[6];

		if (!IsValidName(m_sName) || !IsValidValue(m_sValue))
		{
			return false;
		}

		break;
	}

	return !empty();

} // Read

//-----------------------------------------------------------------------------
bool KCookies::Parse(const KURL& URL, KStringView sInput)
//-----------------------------------------------------------------------------
{
	KCookie Cookie;

	if (Cookie.Parse(URL, sInput))
	{
		// check if we have this cookie name already: delete first..
		// erase-remove idiom..
		kEraseRemoveIf(m_Cookies, [&Cookie](const KCookie& element)
		{
			return element.Name() == Cookie.Name();
		});
		// add the new cookie
		m_Cookies.push_back(Cookie);
		return true;
	}

	return false;

} // Parse

//-----------------------------------------------------------------------------
KString KCookies::Serialize(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	KString sCookies;

	for (auto& Cookie : m_Cookies)
	{
		if (Cookie.WouldSerialize(URL))
		{
			if (!sCookies.empty())
			{
				sCookies += "; ";
			}
			sCookies += Cookie.Serialize();
		}
	}

	return sCookies;

} // Serialize

//-----------------------------------------------------------------------------
bool KCookies::Write(KOutStream& Out) const
//-----------------------------------------------------------------------------
{
	Out.WriteLine("# Netscape HTTP Cookie File");
	Out.WriteLine("# This file was generated by dekaf2");
	Out.WriteLine();

	for (auto& Cookie : m_Cookies)
	{
		Cookie.Write(Out);
	}

	return Out.Good();

} // Write

//-----------------------------------------------------------------------------
bool KCookies::Read(KInStream& In)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		KCookie Cookie;

		if (!Cookie.Read(In))
		{
			break;
		}

		m_Cookies.push_back(std::move(Cookie));
	}

	// if we are at eof we return true
	return !In.Good();

} // Read

//-----------------------------------------------------------------------------
bool KCookies::Add(KCookie Cookie)
//-----------------------------------------------------------------------------
{
	if (Cookie.empty())
	{
		return false;
	}

	m_Cookies.push_back(std::move(Cookie));

	return true;

} // Add

//-----------------------------------------------------------------------------
bool KCookies::Add(KCookies Cookies)
//-----------------------------------------------------------------------------
{
	for (auto& Cookie : Cookies)
	{
		if (!Add(std::move(Cookie)))
		{
			return false;
		}
	}

	return true;

} // Add

static_assert(std::is_nothrow_move_constructible<KCookie>::value,
			  "KCookie is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
