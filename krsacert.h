/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

/// @file krsacert.h
/// create an x509 cert with KRSAKey

#include "kstringview.h"
#include "kerror.h"
#include "krsakey.h"
#include "ktime.h"
#include "kduration.h"

struct x509_st;

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RSA key class to create an x509 cert signed by a KRSAKey
class DEKAF2_PUBLIC KRSACert : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KRSACert() = default;

	/// construct with a new cert
	KRSACert
	(
		const KRSAKey& Key,
		KStringView    sDomain,
		KStringView    sCountryCode,
		KStringView    sOrganization = "",
		KDuration      ValidFor      = chrono::years(1),
		KUnixTime      ValidFrom     = KUnixTime()
	)
	{
		Create(Key, sDomain, sCountryCode, sOrganization, ValidFor, ValidFrom);
	}

	/// construct the cert from a PEM string
	/// @param sPEMCert the string containing the PEM cert
	KRSACert(KStringView sPEMCert)
	{
		Create(sPEMCert);
	}

	KRSACert(const KRSACert& other) = delete;
	KRSACert(KRSACert&& other) noexcept;
	// dtor
	~KRSACert()
	{
		clear();
	}

	/// copy assignment
	KRSACert& operator=(const KRSACert&) = delete;
	/// move assignment
	KRSACert& operator=(KRSACert&& other) noexcept;

	/// reset the cert
	void clear();
	/// test if cert is set
	bool empty() const { return !m_X509Cert; }

	/// create a new cert
	/// @param Key a KRSAKey used for signing
	/// @param sDomain the domain validated by the cert
	/// @param sCountryCode a 2-letter country code
	/// @param sOrganization an organization name, company name, default empty
	/// @param ValidFor a duration for the cert's validity, default 1 year
	/// @param ValidFrom the start time for the cert's validity, default now
	/// @returns false on error, true otherwise
	bool Create
	(
		const KRSAKey& Key,
		KStringView    sDomain,
		KStringView    sCountryCode,
		KStringView    sOrganization = "",
		KDuration      ValidFor      = chrono::years(1),
		KUnixTime      ValidFrom     = KUnixTime()
	);
	/// get the cert from a PEM string
	bool Create(KStringView sPEMCert);

	/// get the cert, may return nullptr in case of error or default construction
	x509_st* GetCert() const;
	/// get the cert as a PEM string
	KString GetPEM();

	/// load cert from file (PEM format)
	bool Load(KStringViewZ sFilename);
	/// save cert to file (PEM format)
	bool Save(KStringViewZ sFilename);

	/// return time from which on this cert is valid
	KUnixTime ValidFrom() const;
	/// return time until which this cert is valid
	KUnixTime ValidUntil() const;
	/// checks if a cert is valid at a certain timepoint
	bool IsValidAt(KUnixTime Time) const;
	/// checks if a cert is valid now
	bool IsValidNow() const;

//------
private:
//------

	x509_st* m_X509Cert { nullptr };

}; // KRSAKey

DEKAF2_NAMESPACE_END
