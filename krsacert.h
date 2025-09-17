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

	bool CheckDomain(KStringViewZ sDomain) const;
	bool CheckCountryCode(KStringViewZ sCountryCode) const;
	bool CheckOrganization(KStringViewZ sOrganization) const;

	/// Create a new private key and a self signed certificate - will also check for validity of existing cert
	/// and replace it if valid for less than a week from now or already expired
	/// @param bThrowOnError if true the function will throw on error, else return false
	/// @param sDomain the domain name for the certificate, defaults to "localhost"
	/// @param sCountryCode a 2-letter country code, defaults to "US"
	/// @param sOrganization an organization name, company name, default empty
	/// @param ValidFor a duration for the cert's validity, default 1 year
	/// @param ValidFrom the start time for the cert's validity, default now
	/// @param iKeyLength the key length, defaults to 4096
	/// @param sKeyFilename the file name to store the key at, defaults to "~/.config/{{program name}}/tls/privkey.pem"
	/// @param sCertFilename the file name to store the cert at, defaults to "~/.config/{{program name}}/tls/cert.pem"
	/// @param sPassword a password for the private key, default empty (no password)
	/// @returns an error string, or an empty string if no error occured
	static KString CheckOrCreateKeyAndCert
	(
		bool        bThrowOnError = false,
		KString     sKeyFilename  = KString{},
		KString     sCertFilename = KString{},
		KString     sPassword     = KString{},
		KStringView sDomain       = "localhost",
		KStringView sCountryCode  = "US",
		KStringView sOrganization = "",
		KDuration   ValidFor      = chrono::years(1),
		KUnixTime   ValidFrom     = KUnixTime(),
		uint16_t    iKeyLength    = 4096
	);

	/// @returns the default private key filename "~/.config/{{program name}}/tls/privkey.pem"
	static KString GetDefaultPrivateKeyFilename();
	/// @returns the default cert filename "~/.config/{{program name}}/tls/cert.pem"
	static KString GetDefaultCertFilename();
	/// @returns the default TLS config directory "~/.config/{{program name}}/tls"
	static KString GetDefaultTLSDirectory();

//------
private:
//------

	bool CheckByNID(KStringViewZ sCheckMe, int nid) const;

	x509_st* m_X509Cert { nullptr };

}; // KRSACert

DEKAF2_NAMESPACE_END
