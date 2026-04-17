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

#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/crypto/hash/bits/kdigest.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KString KECSign::Sign(const KECKey& Key, KStringView sData) const
//---------------------------------------------------------------------------
{
	if (Key.empty() || !Key.IsPrivateKey())
	{
		SetError("signing requires a private key");
		return {};
	}

	KUniquePtr<EVP_MD_CTX, ::EVP_MD_CTX_free> mdCtx(::EVP_MD_CTX_new());

	if (!mdCtx)
	{
		SetError(KDigest::GetOpenSSLError("cannot create MD context"));
		return {};
	}

	if (::EVP_DigestSignInit(mdCtx.get(), nullptr, ::EVP_sha256(), nullptr, Key.GetEVPPKey()) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSignInit failed"));
		return {};
	}

	if (::EVP_DigestSignUpdate(mdCtx.get(),
	                         reinterpret_cast<const unsigned char*>(sData.data()),
	                         sData.size()) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSignUpdate failed"));
		return {};
	}

	std::size_t iDERLen = 0;

	if (::EVP_DigestSignFinal(mdCtx.get(), nullptr, &iDERLen) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSignFinal (size) failed"));
		return {};
	}

	std::vector<unsigned char> aDER(iDERLen);

	if (::EVP_DigestSignFinal(mdCtx.get(), aDER.data(), &iDERLen) != 1)
	{
		SetError(KDigest::GetOpenSSLError("DigestSignFinal failed"));
		return {};
	}

	aDER.resize(iDERLen);

	// convert DER-encoded ECDSA signature to raw r||s (64 bytes)
	const unsigned char* pDER = aDER.data();
	ECDSA_SIG* pSig = ::d2i_ECDSA_SIG(nullptr, &pDER, static_cast<long>(iDERLen));

	if (!pSig)
	{
		SetError(KDigest::GetOpenSSLError("cannot decode ECDSA signature"));
		return {};
	}

	const BIGNUM* pR = nullptr;
	const BIGNUM* pS = nullptr;
	::ECDSA_SIG_get0(pSig, &pR, &pS);

	unsigned char aRawSig[64] = {};
	::BN_bn2binpad(pR, aRawSig,      32);
	::BN_bn2binpad(pS, aRawSig + 32, 32);

	::ECDSA_SIG_free(pSig);

	return KString(reinterpret_cast<const char*>(aRawSig), 64);

} // Sign

//---------------------------------------------------------------------------
bool KECVerify::Verify(const KECKey& Key, KStringView sData, KStringView sSignature) const
//---------------------------------------------------------------------------
{
	if (Key.empty())
	{
		return SetError("verification requires a key");
	}

	if (sSignature.size() != 64)
	{
		return SetError("ES256 signature must be 64 bytes (raw r||s)");
	}

	// reconstruct DER from raw r||s
	KUniquePtr<BIGNUM, ::BN_free> pR(::BN_bin2bn(
		reinterpret_cast<const unsigned char*>(sSignature.data()), 32, nullptr));
	KUniquePtr<BIGNUM, ::BN_free> pS(::BN_bin2bn(
		reinterpret_cast<const unsigned char*>(sSignature.data() + 32), 32, nullptr));

	ECDSA_SIG* pSig = ::ECDSA_SIG_new();

	if (!pSig || ::ECDSA_SIG_set0(pSig, pR.release(), pS.release()) != 1)
	{
		if (pSig) ::ECDSA_SIG_free(pSig);
		return SetError(KDigest::GetOpenSSLError("cannot build ECDSA_SIG"));
	}

	unsigned char* pDER    = nullptr;
	int            iDERLen = ::i2d_ECDSA_SIG(pSig, &pDER);
	::ECDSA_SIG_free(pSig);

	if (iDERLen <= 0 || !pDER)
	{
		return SetError(KDigest::GetOpenSSLError("cannot encode ECDSA_SIG to DER"));
	}

	// verify
	KUniquePtr<EVP_MD_CTX, ::EVP_MD_CTX_free> mdCtx(::EVP_MD_CTX_new());

	if (!mdCtx)
	{
		OPENSSL_free(pDER);
		return SetError(KDigest::GetOpenSSLError("cannot create MD context"));
	}

	if (::EVP_DigestVerifyInit(mdCtx.get(), nullptr, ::EVP_sha256(), nullptr, Key.GetEVPPKey()) != 1)
	{
		OPENSSL_free(pDER);
		return SetError(KDigest::GetOpenSSLError("DigestVerifyInit failed"));
	}

	if (::EVP_DigestVerifyUpdate(mdCtx.get(),
	                            reinterpret_cast<const unsigned char*>(sData.data()),
	                            sData.size()) != 1)
	{
		OPENSSL_free(pDER);
		return SetError(KDigest::GetOpenSSLError("DigestVerifyUpdate failed"));
	}

	int iResult = ::EVP_DigestVerifyFinal(mdCtx.get(),
	                                     reinterpret_cast<const unsigned char*>(pDER),
	                                     static_cast<std::size_t>(iDERLen));
	OPENSSL_free(pDER);

	if (iResult != 1)
	{
		return SetError("signature verification failed");
	}

	return true;

} // Verify

DEKAF2_NAMESPACE_END
