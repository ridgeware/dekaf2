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
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |\|
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

#include <dekaf2/crypto/rsa/kcsr.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/crypto/hash/bits/kdigest.h> // for Digest::GetOpenSSLError()
#include <dekaf2/net/address/kipaddress.h>   // for kIsIPv4Address(), kIsIPv6Address()
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KCSR::KCSR(KCSR&& other) noexcept
//---------------------------------------------------------------------------
: KErrorBase(std::move(other))
, m_Request(other.m_Request)
{
	other.m_Request = nullptr;
}

//---------------------------------------------------------------------------
KCSR& KCSR::operator=(KCSR&& other) noexcept
//---------------------------------------------------------------------------
{
	if (this != &other)
	{
		clear();
		KErrorBase::operator=(std::move(other));
		m_Request       = other.m_Request;
		other.m_Request = nullptr;
	}
	return *this;
}

//---------------------------------------------------------------------------
void KCSR::clear()
//---------------------------------------------------------------------------
{
	ClearError();

	if (m_Request)
	{
		::X509_REQ_free(m_Request);
		m_Request = nullptr;
	}

} // clear

//---------------------------------------------------------------------------
bool KCSR::Create
(
	evp_pkey_st*                pKey,
	const std::vector<KString>& Domains,
	KStringView                 sCountryCode,
	KStringView                 sOrganization
)
//---------------------------------------------------------------------------
{
	clear();

	if (pKey == nullptr)
	{
		return SetError("no key");
	}

	if (Domains.empty())
	{
		return SetError("no domains");
	}

	m_Request = ::X509_REQ_new();

	if (!m_Request)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create X509_REQ struct"));
	}

	if (!::X509_REQ_set_version(m_Request, 0))
	{
		return SetError(KDigest::GetOpenSSLError("cannot set version"));
	}

	// subject name
	auto* name = ::X509_REQ_get_subject_name(m_Request);

	auto sUpperCountryCode = sCountryCode.ToUpperASCII();
	if (!sUpperCountryCode.empty() && !::X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(sUpperCountryCode.data()), static_cast<int>(sUpperCountryCode.size()), -1, 0))
	{
		return SetError(KDigest::GetOpenSSLError("error setting country code"));
	}

	if (!sOrganization.empty() && !::X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(sOrganization.data()), static_cast<int>(sOrganization.size()), -1, 0))
	{
		return SetError(KDigest::GetOpenSSLError("error setting organization"));
	}

	const auto& sCN = Domains.front();
	if (!::X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(sCN.data()), static_cast<int>(sCN.size()), -1, 0))
	{
		return SetError(KDigest::GetOpenSSLError("error setting common name"));
	}

	// all domains as Subject Alternative Names, IP: for IP addresses, DNS: for hostnames
	{
		KString sSAN;

		for (const auto& sDomain : Domains)
		{
			if (!sSAN.empty())
			{
				sSAN += ',';
			}

			if (kIsIPv4Address(sDomain) || kIsIPv6Address(sDomain, false))
			{
				sSAN += kFormat("IP:{}", sDomain);
			}
			else
			{
				sSAN += kFormat("DNS:{}", sDomain);
			}
		}

		X509V3_CTX ctx;
		X509V3_set_ctx_nodb(&ctx);
		X509V3_set_ctx(&ctx, nullptr, nullptr, m_Request, nullptr, 0);

		auto* ext = ::X509V3_EXT_nconf_nid(nullptr, &ctx, NID_subject_alt_name, sSAN.c_str());

		if (!ext)
		{
			return SetError(KDigest::GetOpenSSLError("error creating SAN extension"));
		}

		auto* exts = sk_X509_EXTENSION_new_null();

		if (!exts || !sk_X509_EXTENSION_push(exts, ext))
		{
			::X509_EXTENSION_free(ext);
			sk_X509_EXTENSION_free(exts);
			return SetError(KDigest::GetOpenSSLError("error creating extension stack"));
		}

		auto iAdded = ::X509_REQ_add_extensions(m_Request, exts);

		sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);

		if (!iAdded)
		{
			return SetError(KDigest::GetOpenSSLError("error adding SAN extension"));
		}
	}

	if (!::X509_REQ_set_pubkey(m_Request, pKey))
	{
		return SetError(KDigest::GetOpenSSLError("error setting public key"));
	}

	// SHA-256, as the CA must be able to verify this signature as proof of possession
	if (!::X509_REQ_sign(m_Request, pKey, ::EVP_sha256()))
	{
		return SetError(KDigest::GetOpenSSLError("error signing request"));
	}

	return true;

} // Create

//---------------------------------------------------------------------------
KString KCSR::GetPEM()
//---------------------------------------------------------------------------
{
	KString sPEM;

	if (!m_Request)
	{
		SetError("no request");
		return sPEM;
	}

	KUniquePtr<BIO, ::BIO_free_all> bio(::BIO_new(::BIO_s_mem()));

	if (::PEM_write_bio_X509_REQ(bio.get(), m_Request) != 1)
	{
		SetError(KDigest::GetOpenSSLError("cannot write request"));
	}
	else
	{
		auto iLen = BIO_pending(bio.get());
		sPEM.resize(iLen);

		if (::BIO_read(bio.get(), &sPEM[0], iLen) != iLen)
		{
			sPEM.clear();
			SetError(KDigest::GetOpenSSLError("cannot read request"));
		}
	}

	return sPEM;

} // GetPEM

//---------------------------------------------------------------------------
KString KCSR::GetDER()
//---------------------------------------------------------------------------
{
	KString sDER;

	if (!m_Request)
	{
		SetError("no request");
		return sDER;
	}

	unsigned char* pDER { nullptr };

	auto iLen = ::i2d_X509_REQ(m_Request, &pDER);

	if (iLen < 0)
	{
		SetError(KDigest::GetOpenSSLError("cannot encode request"));
	}
	else
	{
		sDER.assign(reinterpret_cast<const char*>(pDER), static_cast<std::size_t>(iLen));
	}

	::OPENSSL_free(pDER);

	return sDER;

} // GetDER

DEKAF2_NAMESPACE_END
