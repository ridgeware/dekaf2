/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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

#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/crypto/hash/bits/kdigest.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	#include <openssl/core_names.h>
	#include <openssl/param_build.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KECKey::KECKey(KECKey&& other) noexcept
//---------------------------------------------------------------------------
: m_EVPPKey(other.m_EVPPKey)
, m_bIsPrivateKey(other.m_bIsPrivateKey)
{
	other.m_EVPPKey       = nullptr;
	other.m_bIsPrivateKey = false;
}

//---------------------------------------------------------------------------
KECKey& KECKey::operator=(KECKey&& other) noexcept
//---------------------------------------------------------------------------
{
	if (this != &other)
	{
		clear();
		m_EVPPKey             = other.m_EVPPKey;
		m_bIsPrivateKey       = other.m_bIsPrivateKey;
		other.m_EVPPKey       = nullptr;
		other.m_bIsPrivateKey = false;
	}
	return *this;
}

//---------------------------------------------------------------------------
void KECKey::clear()
//---------------------------------------------------------------------------
{
	if (m_EVPPKey)
	{
		::EVP_PKEY_free(m_EVPPKey);
		m_EVPPKey = nullptr;
	}

	m_bIsPrivateKey = false;

} // clear

//---------------------------------------------------------------------------
bool KECKey::Generate()
//---------------------------------------------------------------------------
{
	clear();

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

	KUniquePtr<EVP_PKEY_CTX, ::EVP_PKEY_CTX_free> ctx(::EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));

	if (!ctx)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create EC key context"));
	}

	if (::EVP_PKEY_keygen_init(ctx.get()) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("cannot init EC keygen"));
	}

	if (::EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("cannot set EC curve P-256"));
	}

	EVP_PKEY* pRawKey = nullptr;

	if (::EVP_PKEY_keygen(ctx.get(), &pRawKey) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("EC keygen failed"));
	}

	m_EVPPKey = pRawKey;

#else // OpenSSL 1.x

	KUniquePtr<EC_KEY, ::EC_KEY_free> ecKey(::EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	if (!ecKey || ::EC_KEY_generate_key(ecKey.get()) != 1)
	{
		return SetError(KDigest::GetOpenSSLError("EC keygen failed"));
	}

	m_EVPPKey = ::EVP_PKEY_new();

	if (!m_EVPPKey || ::EVP_PKEY_set1_EC_KEY(m_EVPPKey, ecKey.get()) != 1)
	{
		return SetError(KDigest::GetOpenSSLError("cannot wrap EC key in EVP_PKEY"));
	}

#endif

	m_bIsPrivateKey = true;

	kDebug(2, "generated new EC P-256 key pair");

	return true;

} // Generate

//---------------------------------------------------------------------------
bool KECKey::CreateFromPEM(KStringView sPEM, KStringViewZ sPassword)
//---------------------------------------------------------------------------
{
	clear();

	KUniquePtr<BIO, ::BIO_free_all> bio(::BIO_new_mem_buf(sPEM.data(), static_cast<int>(sPEM.size())));

	if (!bio)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create BIO for PEM"));
	}

	// try private key first
	m_EVPPKey = ::PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr,
	                                     sPassword.empty() ? nullptr : const_cast<char*>(sPassword.c_str()));

	if (m_EVPPKey)
	{
		m_bIsPrivateKey = true;
		return true;
	}

	// try public key
	BIO_reset(bio.get());

	m_EVPPKey = ::PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);

	if (m_EVPPKey)
	{
		m_bIsPrivateKey = false;
		return true;
	}

	return SetError(KDigest::GetOpenSSLError("cannot read PEM key"));

} // CreateFromPEM

//---------------------------------------------------------------------------
bool KECKey::CreateFromRaw(KStringView sRawKey, bool bIsPrivateKey)
//---------------------------------------------------------------------------
{
	clear();

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

	KUniquePtr<OSSL_PARAM_BLD, ::OSSL_PARAM_BLD_free> bld(::OSSL_PARAM_BLD_new());

	if (!bld)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create param builder"));
	}

	::OSSL_PARAM_BLD_push_utf8_string(bld.get(), OSSL_PKEY_PARAM_GROUP_NAME, "prime256v1", 0);

	// declare outside if/else — OSSL_PARAM_BLD_push_BN stores by reference,
	// so the BIGNUM must stay alive until OSSL_PARAM_BLD_to_param is called
	KUniquePtr<BIGNUM, ::BN_free> bn;
	unsigned char aPubFromPriv[65];

	if (bIsPrivateKey)
	{
		if (sRawKey.size() != 32)
		{
			return SetError("EC P-256 private key must be 32 bytes");
		}

		bn.reset(::BN_bin2bn(
			reinterpret_cast<const unsigned char*>(sRawKey.data()),
			static_cast<int>(sRawKey.size()), nullptr));

		if (!bn || ::OSSL_PARAM_BLD_push_BN(bld.get(), OSSL_PKEY_PARAM_PRIV_KEY, bn.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot set private key param"));
		}

		// derive and push the public key — OpenSSL 3.x EVP_PKEY_KEYPAIR needs both
		KUniquePtr<EC_GROUP, ::EC_GROUP_free> group(::EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
		KUniquePtr<EC_POINT, ::EC_POINT_free> pub(::EC_POINT_new(group.get()));

		if (!group || !pub
		    || ::EC_POINT_mul(group.get(), pub.get(), bn.get(), nullptr, nullptr, nullptr) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot derive public key from private key"));
		}

		auto iPubLen = ::EC_POINT_point2oct(group.get(), pub.get(),
		                                    POINT_CONVERSION_UNCOMPRESSED, aPubFromPriv, sizeof(aPubFromPriv), nullptr);

		if (iPubLen != 65
		    || ::OSSL_PARAM_BLD_push_octet_string(bld.get(), OSSL_PKEY_PARAM_PUB_KEY, aPubFromPriv, iPubLen) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot set public key param"));
		}
	}
	else
	{
		if (sRawKey.size() != 65)
		{
			return SetError("EC P-256 uncompressed public key must be 65 bytes");
		}

		::OSSL_PARAM_BLD_push_octet_string(bld.get(), OSSL_PKEY_PARAM_PUB_KEY,
		                                  const_cast<char*>(sRawKey.data()), sRawKey.size());
	}

	KUniquePtr<OSSL_PARAM, ::OSSL_PARAM_free> params(::OSSL_PARAM_BLD_to_param(bld.get()));
	KUniquePtr<EVP_PKEY_CTX, ::EVP_PKEY_CTX_free> ctx(::EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));

	if (!ctx || ::EVP_PKEY_fromdata_init(ctx.get()) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("cannot init key from raw data"));
	}

	if (::EVP_PKEY_fromdata(ctx.get(), &m_EVPPKey,
	                       bIsPrivateKey ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY,
	                       params.get()) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create key from raw data"));
	}

#else // OpenSSL 1.x

	KUniquePtr<EC_KEY, ::EC_KEY_free> ecKey(::EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	if (!ecKey)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create EC key"));
	}

	const EC_GROUP* pGroup = ::EC_KEY_get0_group(ecKey.get());

	if (bIsPrivateKey)
	{
		if (sRawKey.size() != 32)
		{
			return SetError("EC P-256 private key must be 32 bytes");
		}

		KUniquePtr<BIGNUM, ::BN_free> bn(::BN_bin2bn(
			reinterpret_cast<const unsigned char*>(sRawKey.data()),
			static_cast<int>(sRawKey.size()), nullptr));

		if (!bn || ::EC_KEY_set_private_key(ecKey.get(), bn.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot set private key"));
		}

		// derive public key from private
		KUniquePtr<EC_POINT, ::EC_POINT_free> pub(::EC_POINT_new(pGroup));

		if (!pub || ::EC_POINT_mul(pGroup, pub.get(), bn.get(), nullptr, nullptr, nullptr) != 1
		         || ::EC_KEY_set_public_key(ecKey.get(), pub.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot derive public key"));
		}
	}
	else
	{
		if (sRawKey.size() != 65)
		{
			return SetError("EC P-256 uncompressed public key must be 65 bytes");
		}

		KUniquePtr<EC_POINT, ::EC_POINT_free> pub(::EC_POINT_new(pGroup));

		if (!pub || ::EC_POINT_oct2point(pGroup, pub.get(),
		                                reinterpret_cast<const unsigned char*>(sRawKey.data()),
		                                sRawKey.size(), nullptr) != 1
		         || ::EC_KEY_set_public_key(ecKey.get(), pub.get()) != 1)
		{
			return SetError(KDigest::GetOpenSSLError("cannot set public key"));
		}
	}

	m_EVPPKey = ::EVP_PKEY_new();

	if (!m_EVPPKey || ::EVP_PKEY_set1_EC_KEY(m_EVPPKey, ecKey.get()) != 1)
	{
		return SetError(KDigest::GetOpenSSLError("cannot wrap EC key in EVP_PKEY"));
	}

#endif

	m_bIsPrivateKey = bIsPrivateKey;

	return true;

} // CreateFromRaw

//---------------------------------------------------------------------------
KString KECKey::GetPrivateKeyRaw() const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey || !m_bIsPrivateKey)
	{
		return {};
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

	BIGNUM* pPrivBN = nullptr;

	if (::EVP_PKEY_get_bn_param(m_EVPPKey, OSSL_PKEY_PARAM_PRIV_KEY, &pPrivBN) != 1)
	{
		kDebug(1, "cannot extract private key: {}", KDigest::GetOpenSSLError());
		return {};
	}

	KUniquePtr<BIGNUM, ::BN_free> privBN(pPrivBN);
	unsigned char aPriv[32] = {};
	::BN_bn2binpad(privBN.get(), aPriv, 32);

	return KString(reinterpret_cast<const char*>(aPriv), 32);

#else

	const EC_KEY* pEC = ::EVP_PKEY_get0_EC_KEY(m_EVPPKey);

	if (!pEC)
	{
		return {};
	}

	const BIGNUM* pPrivBN = ::EC_KEY_get0_private_key(pEC);
	unsigned char aPriv[32] = {};
	::BN_bn2binpad(pPrivBN, aPriv, 32);

	return KString(reinterpret_cast<const char*>(aPriv), 32);

#endif

} // GetPrivateKeyRaw

//---------------------------------------------------------------------------
KString KECKey::GetPublicKeyRaw() const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey)
	{
		return {};
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

	std::size_t iPubLen = 0;

	if (::EVP_PKEY_get_octet_string_param(m_EVPPKey, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &iPubLen) != 1)
	{
		kDebug(1, "cannot get public key size: {}", KDigest::GetOpenSSLError());
		return {};
	}

	KString sPublic(iPubLen, '\0');

	if (::EVP_PKEY_get_octet_string_param(m_EVPPKey, OSSL_PKEY_PARAM_PUB_KEY,
	                                     reinterpret_cast<unsigned char*>(sPublic.data()),
	                                     sPublic.size(), &iPubLen) != 1)
	{
		kDebug(1, "cannot extract public key: {}", KDigest::GetOpenSSLError());
		return {};
	}

	sPublic.resize(iPubLen);

	return sPublic;

#else

	const EC_KEY* pEC = ::EVP_PKEY_get0_EC_KEY(m_EVPPKey);

	if (!pEC)
	{
		return {};
	}

	const EC_POINT* pPub  = ::EC_KEY_get0_public_key(pEC);
	const EC_GROUP* pGrp  = ::EC_KEY_get0_group(pEC);
	unsigned char aPub[65];
	auto iPubLen = ::EC_POINT_point2oct(pGrp, pPub, POINT_CONVERSION_UNCOMPRESSED, aPub, 65, nullptr);

	return KString(reinterpret_cast<const char*>(aPub), iPubLen);

#endif

} // GetPublicKeyRaw

//---------------------------------------------------------------------------
KString KECKey::GetPEM(bool bPrivateKey) const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey)
	{
		return {};
	}

	KUniquePtr<BIO, ::BIO_free_all> bio(::BIO_new(::BIO_s_mem()));

	if (!bio)
	{
		return {};
	}

	int iResult;

	if (bPrivateKey && m_bIsPrivateKey)
	{
		iResult = ::PEM_write_bio_PrivateKey(bio.get(), m_EVPPKey, nullptr, nullptr, 0, nullptr, nullptr);
	}
	else
	{
		iResult = ::PEM_write_bio_PUBKEY(bio.get(), m_EVPPKey);
	}

	if (iResult != 1)
	{
		kDebug(1, "cannot write PEM: {}", KDigest::GetOpenSSLError());
		return {};
	}

	char* pPEM = nullptr;
	long  iLen = BIO_get_mem_data(bio.get(), &pPEM);

	return KString(pPEM, static_cast<std::size_t>(iLen));

} // GetPEM

//---------------------------------------------------------------------------
KString KECKey::DeriveSharedSecret(KStringView sPeerPublicRaw) const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey || !m_bIsPrivateKey)
	{
		kDebug(1, "ECDH requires a private key");
		return {};
	}

	if (sPeerPublicRaw.size() != 65)
	{
		kDebug(1, "peer public key must be 65 bytes (uncompressed P-256), got {}", sPeerPublicRaw.size());
		return {};
	}

	// build peer EVP_PKEY from raw public key
	KECKey PeerKey;

	if (!PeerKey.CreateFromRaw(sPeerPublicRaw, false))
	{
		kDebug(1, "cannot create peer key: {}", PeerKey.Error());
		return {};
	}

	// ECDH derive
	KUniquePtr<EVP_PKEY_CTX, ::EVP_PKEY_CTX_free> ctx(::EVP_PKEY_CTX_new(m_EVPPKey, nullptr));

	if (!ctx || ::EVP_PKEY_derive_init(ctx.get()) <= 0)
	{
		kDebug(1, "ECDH derive_init failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	if (::EVP_PKEY_derive_set_peer(ctx.get(), PeerKey.GetEVPPKey()) <= 0)
	{
		kDebug(1, "ECDH derive_set_peer failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	std::size_t iSecretLen = 0;

	if (::EVP_PKEY_derive(ctx.get(), nullptr, &iSecretLen) <= 0)
	{
		kDebug(1, "ECDH derive (size) failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	KString sSecret(iSecretLen, '\0');

	if (::EVP_PKEY_derive(ctx.get(), reinterpret_cast<unsigned char*>(sSecret.data()), &iSecretLen) <= 0)
	{
		kDebug(1, "ECDH derive failed: {}", KDigest::GetOpenSSLError());
		return {};
	}

	sSecret.resize(iSecretLen);

	return sSecret;

} // DeriveSharedSecret

DEKAF2_NAMESPACE_END
