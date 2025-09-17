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
#include "ksystem.h"
#include "bits/kunique_deleter.h"
#include "bits/kdigest.h" // for Digest::GetOpenSSLError()
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

//---------------------------------------------------------------------------
KUnixTime from_ASN1_time(const ASN1_TIME* asn1_time)
//---------------------------------------------------------------------------
{
	if (!asn1_time || !asn1_time->data || asn1_time->length < 13) return {};

	KStringView sTime(reinterpret_cast<const char*>(asn1_time->data), asn1_time->length);

	KStringView sFormat = "YYYYMMDDhhmmss";

	if (sTime.size() > 14 && sTime[14] == 'Z')
	{
		sTime = sTime.Left(14);
	}
	else if (sTime[12] == 'Z')
	{
		sFormat.remove_prefix(2);
		sTime = sTime.Left(12);
	}
	else
	{
		// error
		return {};
	}

	return kParseTimestamp(sFormat, sTime);

} // from_ASN1_time

#if 0

// this only works starting with openssl 1.1.1, and it outputs into struct tm
// so we go with our own code atm

//---------------------------------------------------------------------------
KUnixTime from_ASN1_time(const ASN1_TIME* asn1_time)
//---------------------------------------------------------------------------
{
	if (asn1_time)
	{
		struct tm tm;

		if (ASN1_TIME_to_tm(asn1_time, &tm))
		{
			return KUnixTime::from_tm(tm);
		}
	}

	return {};

} // from_ASN1_time

#endif

} // end of anonymous namespace

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
		if (!::X509_gmtime_adj(X509_getm_notBefore(m_X509Cert), 0))
		{
			return SetError(KDigest::GetOpenSSLError("error setting start time to now"));
		}
	}
	else
	{
		// validity starts at a certain point in time
		if (!::X509_gmtime_adj(X509_getm_notBefore(m_X509Cert), (ValidFrom - KUnixTime::now()).seconds().count()))
		{
			return SetError(KDigest::GetOpenSSLError(kFormat("error setting start time to {:%F %T}", ValidFrom)));
		}
	}

	// validity ends at timepoint
	if (!::X509_gmtime_adj(X509_getm_notAfter(m_X509Cert), ValidFor.seconds().count()))
	{
		return SetError(KDigest::GetOpenSSLError(kFormat("error setting validity to {}", ValidFor)));
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

	// sign the certificate with the private key
	if (!::X509_sign(m_X509Cert, Key.GetEVPPKey(),
#if OPENSSL_VERSION_NUMBER >= 0x010101000
		EVP_sha3_256()
#else
		EVP_sha1()
#endif
	))
	{
		return SetError(KDigest::GetOpenSSLError("error signing cert"));
	}

	return true;

} // Create

//---------------------------------------------------------------------------
bool KRSACert::Create(KStringView sPEMCert)
//---------------------------------------------------------------------------
{
	clear();

	KUniquePtr<BIO, ::BIO_free_all> cert_bio(::BIO_new(::BIO_s_mem()));

	if (static_cast<size_t>(::BIO_write(cert_bio.get(), sPEMCert.data(), static_cast<int>(sPEMCert.size()))) != sPEMCert.size())
	{
		return SetError(KDigest::GetOpenSSLError("cannot load cert"));
	}

	m_X509Cert = ::PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr);

	if (!m_X509Cert)
	{
		return SetError(KDigest::GetOpenSSLError("cannot load cert"));
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

//---------------------------------------------------------------------------
bool KRSACert::Load(KStringViewZ sFilename)
//---------------------------------------------------------------------------
{
	return Create(kReadAll(sFilename));

} // Load

//---------------------------------------------------------------------------
bool KRSACert::Save(KStringViewZ sFilename)
//---------------------------------------------------------------------------
{
	return kWriteFile(sFilename, GetPEM());

} // Save

//---------------------------------------------------------------------------
KUnixTime KRSACert::ValidFrom() const
//---------------------------------------------------------------------------
{
	if (m_X509Cert)
	{
		return from_ASN1_time(X509_get0_notBefore(m_X509Cert));
	}

	return {};

} // ValidFrom

//---------------------------------------------------------------------------
KUnixTime KRSACert::ValidUntil() const
//---------------------------------------------------------------------------
{
	if (m_X509Cert)
	{
		return from_ASN1_time(X509_get0_notAfter(m_X509Cert));
	}

	return {};

} // ValidUntil

//---------------------------------------------------------------------------
bool KRSACert::IsValidAt(KUnixTime Time) const
//---------------------------------------------------------------------------
{
	return (ValidFrom() <= Time && ValidUntil() >= Time);
}

//---------------------------------------------------------------------------
bool KRSACert::IsValidNow() const
//---------------------------------------------------------------------------
{
	return IsValidAt(KUnixTime::now());
}

//---------------------------------------------------------------------------
bool KRSACert::CheckByNID(KStringViewZ sCheckme, int nid) const
//---------------------------------------------------------------------------
{
	if (!m_X509Cert)
	{
		return false;
	}

	auto* name = ::X509_get_subject_name(m_X509Cert);

	if (!name)
	{
		return SetError(KDigest::GetOpenSSLError("cannot read subject"));
	}

	int idx = -1;

	for (;;)
	{
		idx = ::X509_NAME_get_index_by_NID(name, nid, idx);

		if (idx < 0)
		{
			return false;
		}

		auto* name_entry = ::X509_NAME_get_entry(name, idx);

		if (name_entry == nullptr)
		{
			return SetError(KDigest::GetOpenSSLError("cannot read name entry"));
		}

		auto* domain = ::X509_NAME_ENTRY_get_data(name_entry);

		if (domain == nullptr)
		{
			return SetError(KDigest::GetOpenSSLError("cannot read data"));
		}

		unsigned char* utf8 { nullptr };

		auto ec = ::ASN1_STRING_to_UTF8(&utf8, domain);

		if (utf8)
		{
			if (ec >= 0)
			{
				if (sCheckme == reinterpret_cast<char*>(utf8))
				{
					OPENSSL_free(utf8);
					return true;
				}
			}
			OPENSSL_free(utf8);
		}

		if (ec < 0)
		{
			return SetError(KDigest::GetOpenSSLError());
		}
	}

	return false;

} // CheckDomain

//---------------------------------------------------------------------------
bool KRSACert::CheckDomain(KStringViewZ sDomain) const
//---------------------------------------------------------------------------
{
	return CheckByNID(sDomain, NID_commonName);
}

//---------------------------------------------------------------------------
bool KRSACert::CheckCountryCode(KStringViewZ sCountryCode) const
//---------------------------------------------------------------------------
{
	return CheckByNID(sCountryCode, NID_countryName);
}

//---------------------------------------------------------------------------
bool KRSACert::CheckOrganization(KStringViewZ sOrganization) const
//---------------------------------------------------------------------------
{
	return CheckByNID(sOrganization, NID_organizationName);
}

//---------------------------------------------------------------------------
KString KRSACert::GetDefaultPrivateKeyFilename()
//---------------------------------------------------------------------------
{
	return kFormat("{}{}privkey.pem", GetDefaultTLSDirectory(), kDirSep);
}

//---------------------------------------------------------------------------
KString KRSACert::GetDefaultCertFilename()
//---------------------------------------------------------------------------
{
	return kFormat("{}{}cert.pem", GetDefaultTLSDirectory(), kDirSep);
}

//---------------------------------------------------------------------------
KString KRSACert::GetDefaultTLSDirectory()
//---------------------------------------------------------------------------
{
	return kFormat("{}{}tls", kGetConfigPath(), kDirSep);
}

//---------------------------------------------------------------------------
KString KRSACert::CheckOrCreateKeyAndCert
(
	bool        bThrowOnError,
	KString     sKeyFilename,
	KString     sCertFilename,
	KString     sPassword,
	KStringView sDomain,
	KStringView sCountryCode,
	KStringView sOrganization,
	KDuration   ValidFor,
	KUnixTime   ValidFrom,
	uint16_t    iKeyLength
)
//---------------------------------------------------------------------------
{
	//---------------------------------------------------------------------------
	auto SetError = [bThrowOnError](KString sError) -> KString
	//---------------------------------------------------------------------------
	{
		kDebug(1, sError);

		if (bThrowOnError)
		{
			throw KError(sError);
		}

		return sError;
	};

	//---------------------------------------------------------------------------
	auto CreateDirForFile = [](KStringViewZ sFilename) -> KString
	//---------------------------------------------------------------------------
	{
		KString sDirname = kDirname(sFilename, false);

		if (!kDirExists(sDirname))
		{
			if (!kCreateDir(sDirname, 0700, true))
			{
				return kFormat("cannot create tls config dir: {}", sDirname);
			}
		}

		return {};
	};

	if (sKeyFilename.empty())
	{
		sKeyFilename  = GetDefaultPrivateKeyFilename();
	}

	if (sCertFilename.empty())
	{
		sCertFilename = GetDefaultCertFilename();
	}

	auto sError = CreateDirForFile(sKeyFilename);

	if (!sError.empty())
	{
		return SetError(sError);
	}

	sError = CreateDirForFile(sCertFilename);

	if (!sError.empty())
	{
		return SetError(sError);
	}

	// if the key file does not exist, create a new key and store it
	if (!kFileExists(sKeyFilename))
	{
		// create a new key if TLS was requested but no key given
		KRSAKey Key(iKeyLength);

		Key.SetThrowOnError(bThrowOnError);

		// save key on disk for next run
		if (!Key.Save(sKeyFilename, true, sPassword))
		{
			return SetError(Key.GetLastError());
		}

		if (kFileExists(sCertFilename))
		{
			// we have to invalidate any existing cert with a new key
			if (kRename(sCertFilename, kFormat("{}.bak", sCertFilename)))
			{
				kDebug(1, "renamed old cert to {}.bak because we had to create a new key", sCertFilename);
			}
			else
			{
				return SetError(kFormat("cannot rename cert file: {}", sCertFilename));
			}
		}
	}

	// if a cert exists, check its validity
	if (kFileExists(sCertFilename))
	{
		KRSACert Cert;
		Cert.SetThrowOnError(bThrowOnError);

		if (!Cert.Load(sCertFilename))
		{
			return SetError(Cert.GetLastError());
		}

		if (!Cert.CheckDomain(KString(sDomain)))
		{
			return SetError(kFormat("cert '{}' is not valid for domain '{}'", sCertFilename, sDomain));
		}

		if (!Cert.CheckCountryCode(KString(sCountryCode)))
		{
			return SetError(kFormat("cert '{}' is not valid for country code '{}'", sCertFilename, sCountryCode));
		}

		if (!sOrganization.empty() && !Cert.CheckOrganization(KString(sOrganization)))
		{
			return SetError(kFormat("cert '{}' is not valid for organization '{}'", sCertFilename, sOrganization));
		}

		auto now = KUnixTime::now();

		if (Cert.ValidFrom() > now)
		{
			return SetError(kFormat("cert '{}' not yet valid: {:%F %T}", sCertFilename, Cert.ValidFrom()));
		}

		if (Cert.ValidUntil() < (now + chrono::weeks(1)))
		{
			if (Cert.ValidUntil() <= now)
			{
				kDebug(1, "cert expired at {:%F %T}. We will renew it.", Cert.ValidUntil());
			}
			else
			{
				kDebug(1, "cert expires at {:%F %T}, which is in less than a week. We will renew it.", Cert.ValidUntil());
			}

			if (kRename(sCertFilename, kFormat("{}.bak", sCertFilename)))
			{
				kDebug(3, "renamed old cert to {}.bak", sCertFilename);
			}
			else
			{
				return SetError(kFormat("cannot rename cert file: {}", sCertFilename));
			}
		}
	}

	// generate a self-signed cert if the file does not exist
	if (!kFileExists(sCertFilename))
	{
		KRSACert Cert;
		Cert.SetThrowOnError(bThrowOnError);

		KRSAKey Key;

		Key.SetThrowOnError(bThrowOnError);

		if (!Key.Load(sKeyFilename, sPassword))
		{
			SetError(Key.GetLastError());
		}

		// create a self signed cert
		if (!Cert.Create(Key, sDomain, sCountryCode, sOrganization, ValidFor, ValidFrom))
		{
			return SetError(Cert.GetLastError());
		}

		// save cert on disk
		if (!Cert.Save(sCertFilename))
		{
			return SetError(Cert.GetLastError());
		}
	}

	// success
	return {};

} // CreateKeyAndSelfSignedCertFiles

DEKAF2_NAMESPACE_END
