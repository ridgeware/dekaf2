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

#include "krsacert.h"
#include "kstring.h"
#include "kformat.h"
#include "klog.h"
#include "bits/kunique_deleter.h"
#include "bits/kdigest.h" // for Digest::GetOpenSSLError()
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KRSACert::KRSACert(KRSACert&& other) noexcept
//---------------------------------------------------------------------------
: m_X509Cert(other.m_X509Cert)
{
	other.m_X509Cert = nullptr;
}

//---------------------------------------------------------------------------
KRSACert& KRSACert::operator=(KRSACert&& other) noexcept
//---------------------------------------------------------------------------
{
	m_X509Cert = other.m_X509Cert;
	other.m_X509Cert = nullptr;
	return *this;
}

//---------------------------------------------------------------------------
void KRSACert::clear()
//---------------------------------------------------------------------------
{
	ClearError();

	if (m_X509Cert)
	{
		::X509_free(m_X509Cert);
		m_X509Cert = nullptr;
	}

} // clear

//---------------------------------------------------------------------------
bool KRSACert::Create
(
	const KRSAKey& Key,
	KStringView    sDomain,
	KStringView    sCountryCode,
	KStringView    sOrganization,
	KDuration      ValidFor,
	KUnixTime      ValidFrom
)
//---------------------------------------------------------------------------
{
	clear();

	if (Key.GetEVPPKey() == nullptr)
	{
		return SetError("no key");
	}

	m_X509Cert = ::X509_new();

	if (!m_X509Cert)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create X509 struct"));
	}

	// set serial number
	if (!::ASN1_INTEGER_set(X509_get_serialNumber(m_X509Cert), 1))
	{
		return SetError(KDigest::GetOpenSSLError("error setting serial"));
	}

	if (ValidFrom.time_since_epoch().count() == 0)
	{
		// validity starts now
		if (!::X509_gmtime_adj(X509_get_notBefore(m_X509Cert), 0))
		{
			return SetError(KDigest::GetOpenSSLError("error setting start time to now"));
		}
	}
	else
	{
		// validity starts at a certain point in time
		if (!::X509_gmtime_adj(X509_get_notBefore(m_X509Cert), (ValidFrom - KUnixTime::now()).seconds().count()))
		{
			return SetError(KDigest::GetOpenSSLError("error setting start time"));
		}
	}

	// validity ends at timepoint
	if (!::X509_gmtime_adj(X509_get_notAfter(m_X509Cert), ValidFor.seconds().count()))
	{
		return SetError(KDigest::GetOpenSSLError("error setting validity"));
	}

	// public key for cert
	if (!::X509_set_pubkey(m_X509Cert, Key.GetEVPPKey()))
	{
		return SetError(KDigest::GetOpenSSLError("error setting public key"));
	}

	// subject name = issuer name
	::X509_NAME* name = ::X509_get_subject_name(m_X509Cert);

	if (!sOrganization.empty() && !::X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_UTF8, reinterpret_cast<unsigned char*>(const_cast<KStringView::value_type*>(&sOrganization[0]))    , static_cast<int>(sOrganization.size())    , -1, 0))
	{
		return SetError(KDigest::GetOpenSSLError("error setting organization"));
	}

	// country code, org name, and common name (domain)
	auto sUpperCountryCode = sCountryCode.ToUpperASCII();
	if (!::X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_UTF8, reinterpret_cast<unsigned char*>(const_cast<KStringView::value_type*>(&sUpperCountryCode[0])), static_cast<int>(sUpperCountryCode.size()), -1, 0) ||
		!::X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, reinterpret_cast<unsigned char*>(const_cast<KStringView::value_type*>(&sDomain[0]))          , static_cast<int>(sDomain.size())          , -1, 0))
	{
		return SetError(KDigest::GetOpenSSLError("error creating name"));
	}

	// issuer name
	if (!::X509_set_issuer_name(m_X509Cert, name))
	{
		return SetError(KDigest::GetOpenSSLError("error setting name"));
	}

	// sign the certificate
	if (!::X509_sign(m_X509Cert, Key.GetEVPPKey(), EVP_sha1()))
	{
		SetError(KDigest::GetOpenSSLError("error signing cert"));
		clear();
		return false;
	}

	return true;

} // Create

//---------------------------------------------------------------------------
x509_st* KRSACert::GetCert() const
//---------------------------------------------------------------------------
{
	if (!m_X509Cert)
	{
		SetError("X509 cert not yet created..");
	}

	return m_X509Cert;

} // GetCert

//---------------------------------------------------------------------------
KString KRSACert::GetPEM()
//---------------------------------------------------------------------------
{
	KString sPEMCert;

	KUniquePtr<BIO, ::BIO_free_all> key_bio(::BIO_new(::BIO_s_mem()));

	int ec = ::PEM_write_bio_X509(key_bio.get(), GetCert());

	if (ec != 1)
	{
		SetError(KDigest::GetOpenSSLError());
	}
	else
	{
		auto iCertLen = BIO_pending(key_bio.get());
		sPEMCert.resize(iCertLen);

		if (::BIO_read(key_bio.get(), &sPEMCert[0], iCertLen) != iCertLen)
		{
			sPEMCert.clear();
			SetError(KDigest::GetOpenSSLError("cannot read cert"));
		}
	}

	return sPEMCert;

} // GetPEM

DEKAF2_NAMESPACE_END
