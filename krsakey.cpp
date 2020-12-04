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

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include "krsakey.h"
#include "kbase64.h"
#include "klog.h"

namespace dekaf2 {

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

} // clear

//---------------------------------------------------------------------------
bool KRSAKey::Create(const Parameters& parms)
//---------------------------------------------------------------------------
{
	clear();

	if (parms.n.empty() || parms.e.empty())
	{
		return false;
	}

	auto rsa = RSA_new();

	if (!rsa)
	{
		return false;
	}

#if OPENSSL_VERSION_NUMBER >= 0x010100000

	RSA_set0_key(rsa, Base64ToBignum(parms.n), Base64ToBignum(parms.e), Base64ToBignum(parms.d));

	if (!parms.p.empty() && !parms.q.empty())
	{
		RSA_set0_factors(rsa, Base64ToBignum(parms.p), Base64ToBignum(parms.q));
	}

	if (!parms.dp.empty() && !parms.dq.empty() && !parms.qi.empty())
	{
		RSA_set0_crt_params(rsa, Base64ToBignum(parms.dp), Base64ToBignum(parms.dq), Base64ToBignum(parms.qi));
	}

#else

	rsa->n = Base64ToBignum(parms.n);
	rsa->e = Base64ToBignum(parms.e);
	rsa->d = Base64ToBignum(parms.d);

	if (!parms.p.empty() && !parms.q.empty())
	{
		rsa->p = Base64ToBignum(parms.p);
		rsa->q = Base64ToBignum(parms.q);
	}

	if (!parms.dp.empty() && !parms.dq.empty() && !parms.qi.empty())
	{
		rsa->dmp1 = Base64ToBignum(parms.dp);
		rsa->dmq1 = Base64ToBignum(parms.dq);
		rsa->iqmp = Base64ToBignum(parms.qi);
	}

#endif

	m_EVPPKey = EVP_PKEY_new();
	if (!m_EVPPKey)
	{
		RSA_free(rsa);
		return false;
	}

	EVP_PKEY_assign(m_EVPPKey, EVP_PKEY_RSA, rsa);

	return true;

} // Create

//---------------------------------------------------------------------------
bool KRSAKey::Create(KStringView sPubKey, KStringView sPrivKey)
//---------------------------------------------------------------------------
{
	std::unique_ptr<BIO, decltype(&BIO_free_all)> pubkey_bio(BIO_new(BIO_s_mem()), BIO_free_all);

	if (static_cast<size_t>(BIO_write(pubkey_bio.get(), sPubKey.data(), static_cast<int>(sPubKey.size()))) != sPubKey.size())
	{
		kWarning("cannot load public key");
		return false;
	}

	m_EVPPKey = PEM_read_bio_PUBKEY(pubkey_bio.get(), nullptr, nullptr, nullptr); // last nullptr would be pubkey password
	if (!m_EVPPKey)
	{
		kWarning("cannot load public key");
		return false;
	}

	if (!sPrivKey.empty())
	{
		std::unique_ptr<BIO, decltype(&BIO_free_all)> privkey_bio(BIO_new(BIO_s_mem()), BIO_free_all);
		if (static_cast<size_t>(BIO_write(privkey_bio.get(), sPrivKey.data(), static_cast<int>(sPrivKey.size()))) != sPrivKey.size())
		{
			kWarning("cannot load private key");
			return false;
		}

		RSA* privkey = PEM_read_bio_RSAPrivateKey(privkey_bio.get(), nullptr, nullptr, nullptr); // last parm would be privkey password
		if (privkey == nullptr)
		{
			kWarning("cannot load private key");
			return false;
		}

		if (EVP_PKEY_assign_RSA(m_EVPPKey, privkey) == 0)
		{
			RSA_free(privkey);
			kWarning("cannot load private key");
			return false;
		}
	}

	return true;

} // ctor

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
KRSAKey::Parameters::Parameters(const KJSON& json)
//---------------------------------------------------------------------------
	:  n(kjson::GetStringRef(json, "n"))
	,  e(kjson::GetStringRef(json, "e"))
	,  d(kjson::GetStringRef(json, "d"))
	,  p(kjson::GetStringRef(json, "p"))
	,  q(kjson::GetStringRef(json, "q"))
	, dp(kjson::GetStringRef(json, "dp"))
	, dq(kjson::GetStringRef(json, "dq"))
	, qi(kjson::GetStringRef(json, "qi"))
{
}

} // end of namespace dekaf2


