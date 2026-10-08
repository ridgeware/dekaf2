/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

#include <dekaf2/crypto/hash/khmac.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/core/logging/klog.h>
#include <openssl/crypto.h>
#include <array>

#if OPENSSL_VERSION_NUMBER >= 0x030000000
	#include <openssl/evp.h>
#else
	#include <openssl/hmac.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KHMAC::KHMAC(enum Digest digest, KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
: m_Digest(digest)
{
#if OPENSSL_VERSION_NUMBER < 0x010100000L
	m_hmacctx.reset(new ::HMAC_CTX());
#elif OPENSSL_VERSION_NUMBER < 0x030000000L
	m_hmacctx.reset(::HMAC_CTX_new());
#else
	// EVP_MAC_CTX_new() takes its own reference of the MAC, therefore the MAC is
	// released at the end of the constructor
	KUniquePtr<EVP_MAC, ::EVP_MAC_free> MAC(::EVP_MAC_fetch(nullptr, "hmac", nullptr));

	if (!MAC)
	{
		SetError(GetOpenSSLError("cannot create MAC"));
		return;
	}

	::OSSL_PARAM params[3];
	size_t params_n = 0;

//  we could also select a cipher like "aes-128-cbc"
//	params[params_n++] = ::OSSL_PARAM_construct_utf8_string("cipher", const_cast<char*>(sCipher.data()), sCipher.size());
	auto sDigest = ToString(digest);
	params[params_n++] = ::OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>(sDigest.data()), sDigest.size());
	params[params_n]   = ::OSSL_PARAM_construct_end();

	m_hmacctx.reset(::EVP_MAC_CTX_new(MAC.get()));
#endif

	if (!m_hmacctx)
	{
		SetError(GetOpenSSLError("cannot create context"));
		return;
	}

#if OPENSSL_VERSION_NUMBER < 0x030000000L
	if (1 != ::HMAC_Init_ex(m_hmacctx.get(), sKey.data(), static_cast<int>(sKey.size()), GetMessageDigest(digest), nullptr))
#else
	if (!::EVP_MAC_init(m_hmacctx.get(), reinterpret_cast<const unsigned char*>(sKey.data()), sKey.size(), params))
#endif
	{
		Release();
		SetError(GetOpenSSLError("cannot initialize algorithm"));
		return;
	}

	if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

#if OPENSSL_VERSION_NUMBER < 0x030000000L
//---------------------------------------------------------------------------
void KHMAC::FreeContext(hmac_ctx_st* pContext)
//---------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER < 0x010100000L
	::HMAC_CTX_cleanup(pContext);
	delete pContext;
#else
	::HMAC_CTX_free(pContext);
#endif

} // FreeContext
#else
//---------------------------------------------------------------------------
void KHMAC::FreeContext(evp_mac_ctx_st* pContext)
//---------------------------------------------------------------------------
{
	::EVP_MAC_CTX_free(pContext);

} // FreeContext
#endif

//---------------------------------------------------------------------------
void KHMAC::Release() noexcept
//---------------------------------------------------------------------------
{
	m_hmacctx.reset();
	m_sHMAC.clear();

} // Release

//---------------------------------------------------------------------------
bool KHMAC::Update(const void* pAddress, std::size_t iSize)
//---------------------------------------------------------------------------
{
	if (!m_hmacctx)
	{
		return false;
	}

#if OPENSSL_VERSION_NUMBER < 0x030000000L
	if (1 != ::HMAC_Update(m_hmacctx.get(), static_cast<const unsigned char*>(pAddress), iSize))
#else
	if (!::EVP_MAC_update(m_hmacctx.get(), static_cast<const unsigned char*>(pAddress), iSize))
#endif
	{
		return SetError(GetOpenSSLError("update failed"));
	}

	return true;

} // Update

//---------------------------------------------------------------------------
bool KHMAC::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	if (!m_hmacctx)
	{
		return false;
	}

	std::array<unsigned char, KDefaultCopyBufSize> Buffer;

	for (;;)
	{
		auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());

#if OPENSSL_VERSION_NUMBER < 0x030000000L
		if (1 != ::HMAC_Update(m_hmacctx.get(), Buffer.data(), iReadChunk))
#else
		if (!::EVP_MAC_update(m_hmacctx.get(), Buffer.data(), iReadChunk))
#endif
		{
			return SetError(GetOpenSSLError("update failed"));
		}

		if (iReadChunk < Buffer.size())
		{
			return true;
		}
	}

} // Update

//---------------------------------------------------------------------------
bool KHMAC::Update(KInStream&& InputStream)
//---------------------------------------------------------------------------
{
	return Update(InputStream);

} // Update

//---------------------------------------------------------------------------
const KString& KHMAC::Digest() const
//---------------------------------------------------------------------------
{
	// without a context (after a failed construction, or after a move) the HMAC
	// stays empty
	if (m_sHMAC.empty() && m_hmacctx)
	{
		std::array<unsigned char, EVP_MAX_MD_SIZE> Buffer;

#if OPENSSL_VERSION_NUMBER < 0x030000000L
		unsigned int iDigestLen;
		if (1 != ::HMAC_Final(m_hmacctx.get(), Buffer.data(), &iDigestLen))
#else
		std::size_t iDigestLen;
		if (!::EVP_MAC_final(m_hmacctx.get(), Buffer.data(), &iDigestLen, Buffer.size()))
#endif
		{
			SetError(GetOpenSSLError("cannot read HMAC"));
		}
		else
		{
			m_sHMAC.append(reinterpret_cast<const char*>(Buffer.data()), iDigestLen);
		}

		OPENSSL_cleanse(Buffer.data(), Buffer.size());
	}

	return m_sHMAC;

} // Digest

//---------------------------------------------------------------------------
KString KHMAC::HexDigest() const
//---------------------------------------------------------------------------
{
	return KEncode::Hex(Digest());

} // HexDigest

static_assert(std::is_nothrow_move_constructible<KHMAC>::value,
			  "KHMAC is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_assignable<KHMAC>::value,
			  "KHMAC is intended to be nothrow move assignable, but is not!");

DEKAF2_NAMESPACE_END
