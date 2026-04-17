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

#include <dekaf2/crypto/ec/ked25519sign.h>

#if DEKAF2_HAS_ED25519

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/crypto/hash/bits/kdigest.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <array>

DEKAF2_NAMESPACE_BEGIN

// KEd25519Key

//---------------------------------------------------------------------------
KEd25519Key::KEd25519Key(KEd25519Key&& other) noexcept
//---------------------------------------------------------------------------
: m_EVPPKey(other.m_EVPPKey)
, m_bIsPrivateKey(other.m_bIsPrivateKey)
{
	other.m_EVPPKey       = nullptr;
	other.m_bIsPrivateKey = false;

} // ctor

//---------------------------------------------------------------------------
KEd25519Key& KEd25519Key::operator=(KEd25519Key&& other) noexcept
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

} // operator=

//---------------------------------------------------------------------------
void KEd25519Key::clear()
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
bool KEd25519Key::Generate()
//---------------------------------------------------------------------------
{
	clear();

	KUniquePtr<EVP_PKEY_CTX, ::EVP_PKEY_CTX_free> ctx(::EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr));

	if (!ctx)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create Ed25519 key context"));
	}

	if (::EVP_PKEY_keygen_init(ctx.get()) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("cannot init Ed25519 keygen"));
	}

	EVP_PKEY* pRawKey = nullptr;

	if (::EVP_PKEY_keygen(ctx.get(), &pRawKey) <= 0)
	{
		return SetError(KDigest::GetOpenSSLError("Ed25519 keygen failed"));
	}

	m_EVPPKey       = pRawKey;
	m_bIsPrivateKey = true;

	kDebug(2, "generated new Ed25519 key pair");

	return true;

} // Generate

//---------------------------------------------------------------------------
bool KEd25519Key::CreateFromPEM(KStringView sPEM, KStringViewZ sPassword)
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
		if (::EVP_PKEY_id(m_EVPPKey) != EVP_PKEY_ED25519)
		{
			::EVP_PKEY_free(m_EVPPKey);
			m_EVPPKey = nullptr;
			return SetError("PEM key is not Ed25519");
		}

		m_bIsPrivateKey = true;
		return true;
	}

	// try public key
	BIO_reset(bio.get());

	m_EVPPKey = ::PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);

	if (m_EVPPKey)
	{
		if (::EVP_PKEY_id(m_EVPPKey) != EVP_PKEY_ED25519)
		{
			::EVP_PKEY_free(m_EVPPKey);
			m_EVPPKey = nullptr;
			return SetError("PEM key is not Ed25519");
		}

		m_bIsPrivateKey = false;
		return true;
	}

	return SetError(KDigest::GetOpenSSLError("cannot read PEM key"));

} // CreateFromPEM

//---------------------------------------------------------------------------
bool KEd25519Key::CreateFromRaw(KStringView sRawKey, bool bIsPrivateKey)
//---------------------------------------------------------------------------
{
	clear();

	if (sRawKey.size() != 32)
	{
		return SetError("Ed25519 key must be 32 bytes");
	}

	if (bIsPrivateKey)
	{
		m_EVPPKey = ::EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
		                                           reinterpret_cast<const unsigned char*>(sRawKey.data()),
		                                           sRawKey.size());
	}
	else
	{
		m_EVPPKey = ::EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
		                                          reinterpret_cast<const unsigned char*>(sRawKey.data()),
		                                          sRawKey.size());
	}

	if (!m_EVPPKey)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create Ed25519 key from raw data"));
	}

	m_bIsPrivateKey = bIsPrivateKey;

	return true;

} // CreateFromRaw

//---------------------------------------------------------------------------
KString KEd25519Key::GetPrivateKeyRaw() const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey || !m_bIsPrivateKey)
	{
		return {};
	}

	std::array<unsigned char, 32> aKey;
	std::size_t iLen = aKey.size();

	if (::EVP_PKEY_get_raw_private_key(m_EVPPKey, aKey.data(), &iLen) != 1)
	{
		kDebug(1, "cannot extract Ed25519 private key: {}", KDigest::GetOpenSSLError());
		return {};
	}

	return KString(reinterpret_cast<const char*>(aKey.data()), iLen);

} // GetPrivateKeyRaw

//---------------------------------------------------------------------------
KString KEd25519Key::GetPublicKeyRaw() const
//---------------------------------------------------------------------------
{
	if (!m_EVPPKey)
	{
		return {};
	}

	std::array<unsigned char, 32> aKey;
	std::size_t iLen = aKey.size();

	if (::EVP_PKEY_get_raw_public_key(m_EVPPKey, aKey.data(), &iLen) != 1)
	{
		kDebug(1, "cannot extract Ed25519 public key: {}", KDigest::GetOpenSSLError());
		return {};
	}

	return KString(reinterpret_cast<const char*>(aKey.data()), iLen);

} // GetPublicKeyRaw

//---------------------------------------------------------------------------
KString KEd25519Key::GetPEM(bool bPrivateKey) const
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

// KEd25519Sign

//---------------------------------------------------------------------------
KString KEd25519Sign::Sign(const KEd25519Key& Key, KStringView sData) const
//---------------------------------------------------------------------------
{
	if (Key.empty() || !Key.IsPrivateKey())
	{
		SetError("signing requires a private key");
		return {};
	}

	// Ed25519 uses a one-shot DigestSign (no separate Update step)
	KUniquePtr<EVP_MD_CTX, ::EVP_MD_CTX_free> mdCtx(::EVP_MD_CTX_new());

	if (!mdCtx)
	{
		SetError(KDigest::GetOpenSSLError("cannot create MD context"));
		return {};
	}

	// Ed25519: digest is NULL (hash is built into the algorithm)
	if (::EVP_DigestSignInit(mdCtx.get(), nullptr, nullptr, nullptr, Key.GetEVPPKey()) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSignInit failed"));
		return {};
	}

	// one-shot sign
	std::size_t iSigLen = 0;

	if (::EVP_DigestSign(mdCtx.get(), nullptr, &iSigLen,
	                    reinterpret_cast<const unsigned char*>(sData.data()),
	                    sData.size()) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSign (size) failed"));
		return {};
	}

	KString sSig(iSigLen, '\0');

	if (::EVP_DigestSign(mdCtx.get(),
	                    reinterpret_cast<unsigned char*>(sSig.data()), &iSigLen,
	                    reinterpret_cast<const unsigned char*>(sData.data()),
	                    sData.size()) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSign failed"));
		return {};
	}

	sSig.resize(iSigLen);

	return sSig;

} // Sign

// KEd25519Verify

//---------------------------------------------------------------------------
bool KEd25519Verify::Verify(const KEd25519Key& Key, KStringView sData, KStringView sSignature) const
//---------------------------------------------------------------------------
{
	if (Key.empty())
	{
		return SetError("verification requires a key");
	}

	if (sSignature.size() != 64)
	{
		return SetError("Ed25519 signature must be 64 bytes");
	}

	KUniquePtr<EVP_MD_CTX, ::EVP_MD_CTX_free> mdCtx(::EVP_MD_CTX_new());

	if (!mdCtx)
	{
		return SetError(KDigest::GetOpenSSLError("cannot create MD context"));
	}

	// Ed25519: digest is NULL
	if (::EVP_DigestVerifyInit(mdCtx.get(), nullptr, nullptr, nullptr, Key.GetEVPPKey()) != 1)
	{
		return SetError(KDigest::GetOpenSSLError("DigestVerifyInit failed"));
	}

	// one-shot verify
	int iResult = ::EVP_DigestVerify(mdCtx.get(),
	                                reinterpret_cast<const unsigned char*>(sSignature.data()),
	                                sSignature.size(),
	                                reinterpret_cast<const unsigned char*>(sData.data()),
	                                sData.size());

	if (iResult != 1)
	{
		return SetError("signature verification failed");
	}

	return true;

} // Verify

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_ED25519
