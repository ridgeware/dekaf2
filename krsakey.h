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

/// @file krsakey.h
/// RSA key conversions

#include "kstringview.h"
#include "kjson.h"
#include "kerror.h"

struct evp_pkey_st;

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RSA key class to transform formats, holds either a private or a public key
class DEKAF2_PUBLIC KRSAKey : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Parameter struct to create an RSA key
	struct Parameters
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// constructs the parameter set from a json node
		Parameters(const KJSON& json);

		KStringView n;
		KStringView e;
		KStringView d;
		KStringView p;
		KStringView q;
		KStringView dp;
		KStringView dq;
		KStringView qi;
	};

	/// default ctor
	KRSAKey() = default;
	/// construct a new key with the given keylen - choose values from 1024 to 16384, probably as power of 2
	KRSAKey(uint16_t iKeylen)
	{
		Create(iKeylen);
	}
	/// construct the key from parameters
	KRSAKey(const Parameters& parms)
	{
		Create(parms);
	}
	/// construct the key from JSON
	KRSAKey(const KJSON& json)
	{
		Create(json);
	}
	/// construct the key from a PEM string
	/// @param sPEMKey the string containing the PEM key
	/// @param sPassword the password to read the PEM key, defaults to no password
	KRSAKey(KStringView sPEMKey, KStringViewZ sPassword = KStringViewZ{})
	{
		Create(sPEMKey, sPassword);
	}
	/// copy construction
	KRSAKey(const KRSAKey&) = delete;
	/// move construction
	KRSAKey(KRSAKey&& other) noexcept;
	// dtor
	~KRSAKey()
	{
		clear();
	}

	/// copy assignment
	KRSAKey& operator=(const KRSAKey&) = delete;
	/// move assignment
	KRSAKey& operator=(KRSAKey&& other) noexcept;

	/// reset the key
	void clear();
	/// test if key is set
	bool empty() const { return !m_EVPPKey; }
	/// create a new key with len keylen - choose values from 1024 to 16384, probably as power of 2
	bool Create(uint16_t iKeylen);
	/// create the key from parameters
	bool Create(const Parameters& parms);
	/// create the key from JSON
	bool Create(const KJSON& json)
	{
		return Create(Parameters(json));
	}
	/// get the key from a PEM string
	/// @param sPEMKey the string containing the PEM key
	/// @param sPassword the password to read the PEM key, defaults to no password
	/// @return true if the key could be read, false otherwise
	bool Create(KStringView sPEMKey, KStringViewZ sPassword = KStringViewZ{});
	/// get the key, may return nullptr in case of error or default construction
	evp_pkey_st* GetEVPPKey() const;
	/// is this a private key?
	bool IsPrivateKey() const { return m_bIsPrivateKey; }
	/// get either the public or private key as a PEM string
	KString GetPEM(bool bPrivateKey, KStringView sPassword = KStringView{});
	/// load key from file (PEM format)
	bool Load(KStringViewZ sFilename, KStringViewZ sPassword = KStringViewZ{});
	/// save key to file (PEM format)
	bool Save(KStringViewZ sFilename, bool bPrivateKey, KStringView sPassword = KStringView{});

//------
private:
//------

	evp_pkey_st* m_EVPPKey       { nullptr }; // is a EVP_PKEY
	bool         m_bIsPrivateKey { false   };

}; // KRSAKey

DEKAF2_NAMESPACE_END
