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

#include "krsakey.h"
#include "kbase64.h"
#include "kstring.h"
#include "kformat.h"
#include "klog.h"
#include "bits/kunique_deleter.h"
#include "bits/kdigest.h" // for Digest::GetOpenSSLError()
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER >= 0x030000000
	#include <openssl/param_build.h>
#endif

#include <openssl/bio.h>
#include <openssl/x509.h>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KRSAKey::KRSAKey(KRSAKey&& other) noexcept
//---------------------------------------------------------------------------
: m_EVPPKey(other.m_EVPPKey)
{
	other.m_EVPPKey = nullptr;
}

//---------------------------------------------------------------------------
KRSAKey& KRSAKey::operator=(KRSAKey&& other) noexcept
//---------------------------------------------------------------------------
{
	m_EVPPKey = other.m_EVPPKey;
	other.m_EVPPKey = nullptr;
	return *this;
}

//---------------------------------------------------------------------------
BIGNUM* Base64ToBignum(KStringView sBase64)
//---------------------------------------------------------------------------
{
	if (sBase64.empty())
	{
		return nullptr;
	}

	auto sBin = KBase64Url::Decode(sBase64);

	return BN_bin2bn(reinterpret_cast<unsigned char*>(sBin.data()), static_cast<int>(sBin.size()), nullptr);

} // Base64ToBignum

//---------------------------------------------------------------------------
void KRSAKey::clear()
//---------------------------------------------------------------------------
{
	if (m_EVPPKey)
	{
		EVP_PKEY_free(m_EVPPKey);
		m_EVPPKey = nullptr;
	}

	m_bIsPrivateKey = false;

} // clear

//---------------------------------------------------------------------------
bool KRSAKey::Create(uint16_t iKeylen)
//---------------------------------------------------------------------------
{
	clear();

#if OPENSSL_VERSION_NUMBER >= 0x030000000

	m_EVPPKey = ::EVP_RSA_gen(iKeylen);

#else

	{
		KUniquePtr<RSA,    ::RSA_free> rsa(RSA_new());
		KUniquePtr<BIGNUM, ::BN_free > bn (BN_new() );

		m_EVPPKey = EVP_PKEY_new();

		if (::BN_set_word(bn.get(), RSA_F4) != 1
		 || ::RSA_generate_key_ex(rsa.get(), iKeylen, bn.get(), nullptr) != 1
		 || ::EVP_PKEY_set1_RSA(m_EVPPKey, rsa.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot generate new key"));
		}
	}

#endif

	if (!m_EVPPKey)
	{
		return SetError(KDigest::GetOpenSSLError("cannot generate new key"));
	}

	m_bIsPrivateKey = true;

	return true;

} // Create

//---------------------------------------------------------------------------
bool KRSAKey::Create(const Parameters& parms)
//---------------------------------------------------------------------------
{
	clear();

	if (parms.n.empty() || parms.e.empty())
	{
		return SetError("empty parameters");
	}

#if OPENSSL_VERSION_NUMBER >= 0x030000000

	KUniquePtr<OSSL_PARAM, ::OSSL_PARAM_free> params;

	{
		// build params to create params array
		KUniquePtr<OSSL_PARAM_BLD, ::OSSL_PARAM_BLD_free> params_build(::OSSL_PARAM_BLD_new());

		if (!params_build)
		{
			return SetError(KDigest::GetOpenSSLError("cannot build key"));
		}

		// the public part
		KUniquePtr<BIGNUM, ::BN_free> bnN  (Base64ToBignum (parms.n ));
		KUniquePtr<BIGNUM, ::BN_free> bnE  (Base64ToBignum (parms.e ));
		KUniquePtr<BIGNUM, ::BN_free> bnD  (Base64ToBignum (parms.d ));
		// the private part, if available
		KUniquePtr<BIGNUM, ::BN_free> bnP  (Base64ToBignum (parms.p ));
		KUniquePtr<BIGNUM, ::BN_free> bnQ  (Base64ToBignum (parms.q ));
		KUniquePtr<BIGNUM, ::BN_free> bnDP (Base64ToBignum (parms.dp));
		KUniquePtr<BIGNUM, ::BN_free> bnDQ (Base64ToBignum (parms.dq));
		KUniquePtr<BIGNUM, ::BN_free> bnQI (Base64ToBignum (parms.qi));

		if (::OSSL_PARAM_BLD_push_BN(params_build.get(), "n", bnN.get()) != 1 ||
			::OSSL_PARAM_BLD_push_BN(params_build.get(), "e", bnE.get()) != 1 ||
			::OSSL_PARAM_BLD_push_BN(params_build.get(), "d", bnD.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot build key"));
		}

		if (bnP && bnQ)
		{
			if (::OSSL_PARAM_BLD_push_BN(params_build.get(), "rsa-factor1", bnP.get()) != 1 ||
				::OSSL_PARAM_BLD_push_BN(params_build.get(), "rsa-factor2", bnQ.get()) != 1)
			{
				return SetError(KDigest::GetOpenSSLError("cannot build key"));
			}

			m_bIsPrivateKey = true;
		}

		if (bnDP && bnDQ && bnQI)
		{
			if (::OSSL_PARAM_BLD_push_BN(params_build.get(), "rsa-exponent1"   , bnDP.get()) != 1 ||
				::OSSL_PARAM_BLD_push_BN(params_build.get(), "rsa-exponent2"   , bnDQ.get()) != 1 ||
				::OSSL_PARAM_BLD_push_BN(params_build.get(), "rsa-coefficient1", bnQI.get()) != 1)
			{
				return SetError(KDigest::GetOpenSSLError("cannot build key"));
			}

			m_bIsPrivateKey = true;
		}

		// create params array
		params.reset(::OSSL_PARAM_BLD_to_param(params_build.get()));
	}

	// Create RSA key from params
	auto ctx = ::EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr);

	if (!ctx
		|| ::EVP_PKEY_fromdata_init(ctx) != 1
		|| ::EVP_PKEY_fromdata(ctx, &m_EVPPKey, EVP_PKEY_KEYPAIR, params.get()) != 1)
	{
		return SetError(KDigest::GetOpenSSLError("cannot build key"));
	}

#else // !OPENSSL_VERSION_NUMBER >= 0x030000000

	KUniquePtr<RSA, ::RSA_free> rsa(RSA_new());

	if (!rsa)
	{
		return SetError(KDigest::GetOpenSSLError("cannot build key"));
	}

#if OPENSSL_VERSION_NUMBER >= 0x010100000

	RSA_set0_key(rsa.get(), Base64ToBignum(parms.n), Base64ToBignum(parms.e), Base64ToBignum(parms.d));

	if (!parms.p.empty() && !parms.q.empty())
	{
		RSA_set0_factors(rsa.get(), Base64ToBignum(parms.p), Base64ToBignum(parms.q));
		m_bIsPrivateKey = true;
	}

	if (!parms.dp.empty() && !parms.dq.empty() && !parms.qi.empty())
	{
		RSA_set0_crt_params(rsa.get(), Base64ToBignum(parms.dp), Base64ToBignum(parms.dq), Base64ToBignum(parms.qi));
		m_bIsPrivateKey = true;
	}

#else

	rsa->n = Base64ToBignum(parms.n);
	rsa->e = Base64ToBignum(parms.e);
	rsa->d = Base64ToBignum(parms.d);

	if (!parms.p.empty() && !parms.q.empty())
	{
		rsa->p = Base64ToBignum(parms.p);
		rsa->q = Base64ToBignum(parms.q);
		m_bIsPrivateKey = true;
	}

	if (!parms.dp.empty() && !parms.dq.empty() && !parms.qi.empty())
	{
		rsa->dmp1 = Base64ToBignum(parms.dp);
		rsa->dmq1 = Base64ToBignum(parms.dq);
		rsa->iqmp = Base64ToBignum(parms.qi);
		m_bIsPrivateKey = true;
	}

#endif // !OPENSSL_VERSION_NUMBER >= 0x030000000

	m_EVPPKey = EVP_PKEY_new();

	if (!m_EVPPKey)
	{
		return SetError(KDigest::GetOpenSSLError("cannot build key"));
	}

	EVP_PKEY_assign(m_EVPPKey, EVP_PKEY_RSA, rsa.get());

#endif

	return true;

} // Create

//---------------------------------------------------------------------------
bool KRSAKey::Create(KStringView sPEMKey, KStringViewZ sPassword)
//---------------------------------------------------------------------------
{
	clear();

	// check if this is a private key PEM string
	bool bIsPrivateKey { false };

	{
		auto iPosPriv = sPEMKey.find(" PRIVATE KEY--");

		if (iPosPriv != KStringView::npos)
		{
			auto iPosBegin = sPEMKey.rfind("---BEGIN ", iPosPriv);

			if (iPosBegin != KStringView::npos && iPosPriv - iPosBegin < 10)
			{
				bIsPrivateKey = true;
			}
		}
	}

	KUniquePtr<BIO, ::BIO_free_all> key_bio(::BIO_new(::BIO_s_mem()));

	if (static_cast<size_t>(::BIO_write(key_bio.get(), sPEMKey.data(), static_cast<int>(sPEMKey.size()))) != sPEMKey.size())
	{
		return SetError(KDigest::GetOpenSSLError("cannot load key"));
	}

	if (!bIsPrivateKey)
	{
		m_EVPPKey = ::PEM_read_bio_PUBKEY(key_bio.get(), nullptr, nullptr,
		                                  sPassword.empty() ? nullptr
		                                                    : const_cast<KStringViewZ::value_type*>(&sPassword[0]));
	}
	else // bIsPrivateKey
	{
		m_EVPPKey = ::PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr,
		                                      sPassword.empty() ? nullptr
		                                                        : const_cast<KStringViewZ::value_type*>(&sPassword[0]));
		m_bIsPrivateKey = true;
	}

	if (!m_EVPPKey)
	{
		return SetError(KDigest::GetOpenSSLError("cannot load key"));
	}

	return true;

} // Create

//---------------------------------------------------------------------------
evp_pkey_st* KRSAKey::GetEVPPKey() const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey)
	{
		kDebug(1, "EVP Key not yet created..");
	}

	return m_EVPPKey;

} // GetEVPPKey

//---------------------------------------------------------------------------
KString KRSAKey::GetPEM(bool bPrivateKey, KStringViewZ sPassword)
//---------------------------------------------------------------------------
{
	KString sPEMKey;

	if (!GetEVPPKey())
	{
		return sPEMKey;
	}

	KUniquePtr<BIO, ::BIO_free_all> key_bio(::BIO_new(::BIO_s_mem()));

	int ec;

	if (bPrivateKey)
	{
		unsigned char* keystr { nullptr };
		int keylen { 0 };

		if (!sPassword.empty())
		{
			if (sPassword.size() <= std::numeric_limits<int>::max())
			{
				keystr = reinterpret_cast<unsigned char*>(const_cast<KStringViewZ::value_type*>(&sPassword[0]));
				keylen = static_cast<int>(sPassword.size());
			}
		}

		ec = ::PEM_write_bio_PrivateKey(key_bio.get(), GetEVPPKey(), nullptr, keystr, keylen, nullptr, nullptr);
	}
	else
	{
		ec = ::PEM_write_bio_PUBKEY(key_bio.get(), GetEVPPKey());
	}

	if (ec != 1)
	{
		SetError(KDigest::GetOpenSSLError());
	}
	else
	{
		auto iKeylen = BIO_pending(key_bio.get());
		sPEMKey.resize(iKeylen);

		if (::BIO_read(key_bio.get(), &sPEMKey[0], iKeylen) != iKeylen)
		{
			sPEMKey.clear();
			SetError(KDigest::GetOpenSSLError("cannot read key"));
		}
	}

	return sPEMKey;

} // GetPEM

//---------------------------------------------------------------------------
KRSAKey::Parameters::Parameters(const KJSON& json)
//---------------------------------------------------------------------------
	:  n(kjson::GetStringRef(json, "n" ))
	,  e(kjson::GetStringRef(json, "e" ))
	,  d(kjson::GetStringRef(json, "d" ))
	,  p(kjson::GetStringRef(json, "p" ))
	,  q(kjson::GetStringRef(json, "q" ))
	, dp(kjson::GetStringRef(json, "dp"))
	, dq(kjson::GetStringRef(json, "dq"))
	, qi(kjson::GetStringRef(json, "qi"))
{
}

DEKAF2_NAMESPACE_END
