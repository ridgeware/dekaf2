/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
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

/// @file kcsr.h
/// PKCS#10 certificate signing requests

#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/errors/kerror.h>
#include <vector>

struct X509_req_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_rsa
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Creates a PKCS#10 certificate signing request, e.g. for an ACME order
class DEKAF2_PUBLIC KCSR : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KCSR() = default;

	/// construct with a new signing request
	/// @param Key the key pair the certificate shall be issued for
	/// @param Domains the requested domains, all added as SAN entries, the first one also as CN
	/// @param sCountryCode a 2-letter country code, default empty
	/// @param sOrganization an organization name, default empty
	KCSR
	(
		const KRSAKey&              Key,
		const std::vector<KString>& Domains,
		KStringView                 sCountryCode  = "",
		KStringView                 sOrganization = ""
	)
	{
		Create(Key, Domains, sCountryCode, sOrganization);
	}

	/// construct with a new signing request for an EC key
	KCSR
	(
		const KECKey&               Key,
		const std::vector<KString>& Domains,
		KStringView                 sCountryCode  = "",
		KStringView                 sOrganization = ""
	)
	{
		Create(Key, Domains, sCountryCode, sOrganization);
	}

	KCSR(const KCSR& other) = delete;
	KCSR(KCSR&& other) noexcept;
	// dtor
	~KCSR()
	{
		clear();
	}

	/// copy assignment
	KCSR& operator=(const KCSR&) = delete;
	/// move assignment
	KCSR& operator=(KCSR&& other) noexcept;

	/// reset the request
	void clear();
	/// test if request is set
	bool empty() const { return !m_Request; }

	/// create a new signing request
	/// @param Key the key pair the certificate shall be issued for
	/// @param Domains the requested domains, all added as SAN entries, the first one also as CN
	/// @param sCountryCode a 2-letter country code, default empty
	/// @param sOrganization an organization name, default empty
	/// @returns false on error, true otherwise
	bool Create
	(
		const KRSAKey&              Key,
		const std::vector<KString>& Domains,
		KStringView                 sCountryCode  = "",
		KStringView                 sOrganization = ""
	)
	{
		return Create(Key.GetEVPPKey(), Domains, sCountryCode, sOrganization);
	}

	/// create a new signing request for an EC key
	bool Create
	(
		const KECKey&               Key,
		const std::vector<KString>& Domains,
		KStringView                 sCountryCode  = "",
		KStringView                 sOrganization = ""
	)
	{
		return Create(Key.GetEVPPKey(), Domains, sCountryCode, sOrganization);
	}

	/// get the request, may return nullptr in case of error or default construction
	X509_req_st* GetRequest() const { return m_Request; }
	/// get the request as a PEM string
	KString GetPEM();
	/// get the request in DER encoding (e.g. for an ACME finalize, base64url encoded)
	KString GetDER();

//------
private:
//------

	DEKAF2_PRIVATE
	bool Create
	(
		evp_pkey_st*                pKey,
		const std::vector<KString>& Domains,
		KStringView                 sCountryCode,
		KStringView                 sOrganization
	);

	X509_req_st* m_Request { nullptr };

}; // KCSR

/// @}

DEKAF2_NAMESPACE_END
