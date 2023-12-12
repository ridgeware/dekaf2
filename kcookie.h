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

#pragma once

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"
#include "kurl.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a (HTTP) Cookie type
class DEKAF2_PUBLIC KCookie
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KCookie() = default;

	/// parse sInput for cookie content
	/// @param URL the URL from which this cookie is set
	/// @param sInput the value part of the Set-Cookie header
	KCookie(const KURL& URL, KStringView sInput)
	{
		Parse(URL, sInput);
	}

	/// parse sInput for cookie content
	/// @param URL the URL from which this cookie is set
	/// @param sInput the value part of the Set-Cookie header
	bool Parse(const KURL& URL, KStringView sInput);

	/// serialize the value part of the Cookie header
	/// @param URL the URL for which this cookie is serialized
	KString Serialize(const KURL& URL) const;

	/// serialize the value part of the Cookie header unconditionally -
	/// only use this after testing with WouldSerialize() that this
	/// cookie is used in a valid context
	KString Serialize() const;

	/// returns true if the cookie is valid in context of the URL
	/// @param URL the URL for which this cookie is serialized
	bool WouldSerialize(const KURL& URL) const;

	/// clear the cookie
	void clear();

	/// is this cookie empty?
	bool empty() const;

	/// returns the name of the cookie
	const KString& Name() const
	{
		return m_sName;
	}

	/// returns the value of the cookie
	const KString& Value() const
	{
		return m_sValue;
	}

//------
private:
//------

	KString      m_sName;
	KString      m_sValue;
	KString      m_sPath;
	url::KDomain m_Domain;
	bool         m_bOnlyHTTPS { false };
	bool         m_bAllowSubDomains { false };

}; // KCookie


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// collection of cookies
class DEKAF2_PUBLIC KCookies
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// parse sInput for cookie content and generate a new
	/// cookie or overwrite an existing one
	/// @param URL the URL from which this cookie is set
	/// @param sInput the value part of the Set-Cookie header
	bool Parse(const KURL& URL, KStringView sInput);

	/// serialize the value part of the Cookie header
	/// @param URL the URL for which this cookie is serialized
	KString Serialize(const KURL& URL) const;

	/// remove all cookies
	void clear()
	{
		m_Cookies.clear();
	}

	/// returns count of cookies
	std::size_t size() const
	{
		return m_Cookies.size();
	}

	/// returns true when we do not have cookies
	bool empty() const
	{
		return m_Cookies.empty();
	}

//------
private:
//------

	std::vector<KCookie> m_Cookies;

}; // KCookies

DEKAF2_NAMESPACE_END
