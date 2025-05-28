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

#include "khmac.h"
#include "kencode.h"
#include "klog.h"

#if OPENSSL_VERSION_NUMBER >= 0x030000000
	#include <openssl/evp.h>
#else
	#include <openssl/hmac.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//---------------------------------------------------------------------------
KHMAC::KHMAC(enum Digest digest, KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER < 0x010100000L
	m_hmacctx = new ::HMAC_CTX();
#elif OPENSSL_VERSION_NUMBER < 0x030000000L
	m_hmacctx = ::HMAC_CTX_new();
#else
	m_hmac  = ::EVP_MAC_fetch(nullptr, "hmac", nullptr);

	if (!m_hmac)
	{
		Release();
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

	m_hmacctx = ::EVP_MAC_CTX_new(m_hmac);
#endif

	if (!m_hmacctx)
	{
		Release();
		SetError(GetOpenSSLError("cannot create context"));
		return;
	}

#if OPENSSL_VERSION_NUMBER < 0x030000000L
	if (1 != ::HMAC_Init_ex(m_hmacctx, sKey.data(), static_cast<int>(sKey.size()), GetMessageDigest(digest), nullptr))
#else
	if (!::EVP_MAC_init(m_hmacctx, reinterpret_cast<const unsigned char*>(sKey.data()), sKey.size(), params))
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

//---------------------------------------------------------------------------
KHMAC::KHMAC(KHMAC&& other)
//---------------------------------------------------------------------------
: m_hmacctx(other.m_hmacctx)
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
, m_hmac(other.m_hmac)
#endif
, m_sHMAC(std::move(other.m_sHMAC))
{
	other.m_hmacctx = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x030000000L
	other.m_hmac = nullptr;
#endif
} // move ctor

//---------------------------------------------------------------------------
void KHMAC::Release()
//---------------------------------------------------------------------------
{
	if (m_hmacctx)
	{
#if OPENSSL_VERSION_NUMBER < 0x010100000L
		::HMAC_CTX_cleanup(m_hmacctx);
		delete hmacctx;
#elif OPENSSL_VERSION_NUMBER < 0x030000000L
		::HMAC_CTX_free(m_hmacctx);
#else
		::EVP_MAC_CTX_free(m_hmacctx);
		::EVP_MAC_free(m_hmac);
		m_hmac = nullptr;
#endif
		m_hmacctx = nullptr;
	}
	m_sHMAC.clear();

} // Release

//---------------------------------------------------------------------------
bool KHMAC::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!m_hmacctx)
	{
		return false;
	}

#if OPENSSL_VERSION_NUMBER < 0x030000000L
	if (1 != ::HMAC_Update(m_hmacctx, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()))
#else
	if (!::EVP_MAC_update(m_hmacctx, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()))
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
		if (1 != ::HMAC_Update(m_hmacctx, Buffer.data(), iReadChunk))
#else
		if (!::EVP_MAC_update(m_hmacctx, Buffer.data(), iReadChunk))
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
	if (m_sHMAC.empty())
	{
		std::array<unsigned char, EVP_MAX_MD_SIZE> Buffer;

#if OPENSSL_VERSION_NUMBER < 0x030000000L
		unsigned int iDigestLen;
		if (1 != ::HMAC_Final(m_hmacctx, Buffer.data(), &iDigestLen))
#else
		std::size_t iDigestLen;
		if (!::EVP_MAC_final(m_hmacctx, Buffer.data(), &iDigestLen, Buffer.size()))
#endif
		{
			SetError(GetOpenSSLError("cannot read HMAC"));
		}
		else
		{
			m_sHMAC.reserve(iDigestLen);

			for (unsigned int iDigit = 0; iDigit < iDigestLen; ++iDigit)
			{
				m_sHMAC += Buffer[iDigit];
			}
		}
	}

	return m_sHMAC;

} // Digest

//---------------------------------------------------------------------------
KString KHMAC::HexDigest() const
//---------------------------------------------------------------------------
{
	return KEncode::Hex(Digest());

} // HexDigest

DEKAF2_NAMESPACE_END
