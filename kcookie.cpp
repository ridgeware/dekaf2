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
#include <vector>

namespace dekaf2 {

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
KString KCookie::Serialize(const KURL& URL, bool bForce) const
//-----------------------------------------------------------------------------
{
	KString sCookie;

	if (!bForce && !WouldSerialize(URL))
	{
		return sCookie;
	}

	sCookie = m_sName;

	if (!m_sValue.empty())
	{
		sCookie += '=';
		sCookie += m_sValue;
	}

	return sCookie;

} // Serialize

//-----------------------------------------------------------------------------
bool KCookie::empty() const
//-----------------------------------------------------------------------------
{
	return m_sName.empty();
}

//-----------------------------------------------------------------------------
bool KCookies::Parse(const KURL& URL, KStringView sInput)
//-----------------------------------------------------------------------------
{
	KCookie Cookie;

	if (Cookie.Parse(URL, sInput))
	{
		// check if we have this cookie name already: delete first..
		// erase-remove idiom..
		m_Cookies.erase(std::remove_if(m_Cookies.begin(), m_Cookies.end(),
										  [&Cookie](const KCookie& element)
										  {
											  return element.Name() == Cookie.Name();
										  }),
						m_Cookies.end());
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
			sCookies += Cookie.Serialize(URL, true);
		}
	}

	return sCookies;

} // Serialize


static_assert(std::is_nothrow_move_constructible<KCookie>::value,
			  "KCookie is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2
