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
#include "kerror.h"
#include <unordered_map>
#include <vector>
#include <atomic>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all keys from a validated OpenID provider
class DEKAF2_PUBLIC KOpenIDKeys : public KErrorBase
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

	/// are all info valid?
	bool IsValid() const { return !HasError(); }

	bool empty() const { return WebKeys.empty(); }
	std::size_t size() const { return WebKeys.size(); }

//----------
private:
//----------

	static const KRSAKey s_EmptyKey;

	struct DEKAF2_PRIVATE WebKey
	{
		WebKey(const KJSON& parms);

		KRSAKey RSAKey;
		KString Algorithm;
		KString Digest;
		KString UseType;
	};

	std::unordered_map<KString, WebKey> WebKeys;

	DEKAF2_PRIVATE
	bool Validate(const KJSON& Keys) const;

}; // KOpenIDKeys

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all data from a validated OpenID provider
class DEKAF2_PUBLIC KOpenIDProvider : public KErrorBase
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
					 KDuration RefreshInterval = std::chrono::hours(24));

	/// are all info valid?
	bool IsValid() const { return !HasError(); }

	struct KeysAndIssuer
	{
		KOpenIDKeys Keys;
		KString     sIssuer;
	};

	const KeysAndIssuer& Get() const { return *m_CurrentKeys->load(std::memory_order_relaxed); }

	void Refresh(KUnixTime Now = Dekaf::getInstance().GetCurrentTimepoint());

//----------
private:
//----------

	DEKAF2_PRIVATE
	bool Validate(const KJSON& Configuration, const KURL& URL, KStringView sScope) const;

	std::unique_ptr<KeysAndIssuer>               m_Keys;
	std::unique_ptr<KeysAndIssuer>               m_DecayingKeys;
	std::unique_ptr<std::atomic<KeysAndIssuer*>> m_CurrentKeys;

	KString           m_sScope;
	KURL              m_URL;
	KDuration         m_RefreshInterval {};
	KUnixTime         m_LastRefresh;

}; // KOpenIDProvider

using KOpenIDProviderList = std::vector<KOpenIDProvider>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds an authentication token and validates it
class DEKAF2_PUBLIC KJWT : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default ctor
	KJWT() = default;

	/// construct with a token
	KJWT(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, KDuration tClockLeeway = chrono::seconds(5))
	{
		Check(sBase64Token, Providers, sScope, tClockLeeway);
	}

	/// check a new token
	bool Check(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, KDuration tClockLeeway = chrono::seconds(5));

	/// is all info valid?
	bool IsValid() const { return !HasError(); }

	/// get user id ("subject")
	const KString& GetUser() const;

	KJSON Header;
	KJSON Payload;

	/// clear all data
	void clear();

//----------
private:
//----------

	DEKAF2_PRIVATE
	bool Validate(KStringView sIssuer, KStringView sScope, KDuration tClockLeeway);
	DEKAF2_PRIVATE
	bool SetError(KStringView sError);
	DEKAF2_PRIVATE
	void ClearJSON();

	bool            m_bSignatureIsValid { false };

}; // KJWT

DEKAF2_NAMESPACE_END
