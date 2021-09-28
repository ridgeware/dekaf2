/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2019, Ridgeware, Inc.
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

#pragma once

#include "kstring.h"
#include "kurl.h"
#include "kjson.h"
#include "krsakey.h"
#include "ktimer.h"
#include "dekaf2.h"
#include <unordered_map>
#include <vector>
#include <atomic>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all keys from a validated OpenID provider
class KOpenIDKeys
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KOpenIDKeys () = default;
	/// query all known information about an OpenID provider
	KOpenIDKeys (const KURL& URL);

	/// return a reference to the RSA key that matches the given parameters
	const KRSAKey& GetRSAKey(KStringView sKeyID, KStringView sAlgorithm, KStringView sKeyDigest, KStringView sUseType = "sig") const;

	/// return error string
	const KString& Error() const { return m_sError; }
	/// are all info valid?
	bool IsValid() const { return Error().empty(); }

	bool empty() const { return WebKeys.empty(); }
	std::size_t size() const { return WebKeys.size(); }

//----------
private:
//----------

	static const KRSAKey s_EmptyKey;

	struct WebKey
	{
		WebKey(const KJSON& parms);

		KRSAKey RSAKey;
		KString Algorithm;
		KString Digest;
		KString UseType;
	};

	std::unordered_map<KString, WebKey> WebKeys;

	bool Validate(const KJSON& Keys) const;
	bool SetError(KString sError) const;

	mutable KString m_sError;

}; // KOpenIDKeys

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all data from a validated OpenID provider
class KOpenIDProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KOpenIDProvider () = default;
	/// query all known information about an OpenID provider, check if scope is
	/// provided if not empty
	KOpenIDProvider (KURL URL,
					 KStringView sScope = KStringView{},
					 KTimer::Interval RefreshInterval = std::chrono::hours(24));

	/// return error string
	const KString& Error() const { return m_sError; }
	/// are all info valid?
	bool IsValid() const { return Error().empty(); }

	KTimer::Timepoint LastRefresh() const;

	struct KeysAndIssuer
	{
		KOpenIDKeys Keys;
		KString     sIssuer;
	};

	const KeysAndIssuer& Get() const { return *m_CurrentKeys.get()->load(std::memory_order_relaxed); }

	void Refresh(KTimer::Timepoint Now = Dekaf::getInstance().GetCurrentTimepoint());

//----------
private:
//----------

	bool Validate(const KJSON& Configuration, const KURL& URL, KStringView sScope) const;
	bool SetError(KString sError) const;

	std::unique_ptr<KeysAndIssuer>               m_Keys;
	std::unique_ptr<KeysAndIssuer>               m_DecayingKeys;
	std::unique_ptr<std::atomic<KeysAndIssuer*>> m_CurrentKeys;

	mutable KString   m_sError;
	KString           m_sScope;
	KURL              m_URL;
	KTimer::Interval  m_RefreshInterval;
	KTimer::Timepoint m_LastRefresh;

}; // KOpenIDProvider

using KOpenIDProviderList = std::vector<KOpenIDProvider>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds an authentication token and validates it
class KJWT
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default ctor
	KJWT() = default;

	/// construct with a token
	KJWT(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, time_t tClockLeeway = 5)
	{
		Check(sBase64Token, Providers, sScope, tClockLeeway);
	}

	/// check a new token
	bool Check(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, time_t tClockLeeway = 5);

	/// return error string
	const KString& Error() const { return m_sError; }

	/// is all info valid?
	bool IsValid() const { return Error().empty(); }

	/// get user id ("subject")
	const KString& GetUser() const;

	KJSON Header;
	KJSON Payload;

	/// clear all data
	void clear();

//----------
private:
//----------

	bool Validate(KStringView sIssuer, KStringView sScope, time_t tClockLeeway);
	bool SetError(KString sError);
	void ClearJSON();

	mutable KString m_sError;

}; // KJWT

} // end of namespace dekaf2

