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
#include <openssl/hmac.h>

// OpenSSL 3.0 introduces a new HMAC interface and makes the
// old one deprecated. For now simply ignore the deprecation.
#if OPENSSL_VERSION_NUMBER >= 0x030000000
#ifdef DEKAF2_IS_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef DEKAF2_IS_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

namespace dekaf2 {

//---------------------------------------------------------------------------
KHMAC::KHMAC(ALGORITHM Algorithm, KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER < 0x010100000
	hmacctx = new HMAC_CTX();
#else
	hmacctx = HMAC_CTX_new();
#endif

	if (!hmacctx)
	{
		kDebug(1, "cannot create context");
		Release();
		return;
	}

	const EVP_MD*(*callback)(void) = nullptr;

	switch (Algorithm)
	{
		case MD5:
			callback = EVP_md5;
			break;

		case SHA1:
			callback = EVP_sha1;
			break;

		case SHA224:
			callback = EVP_sha224;
			break;

		case SHA256:
			callback = EVP_sha256;
			break;

		case SHA384:
			callback = EVP_sha384;
			break;

		case SHA512:
			callback = EVP_sha512;
			break;

#if OPENSSL_VERSION_NUMBER >= 0x010100000
		case BLAKE2S:
			callback = EVP_blake2s256;
			break;

		case BLAKE2B:
			callback = EVP_blake2b512;
			break;
#endif
		case NONE:
            kDebug(1, "no algorithm selected");
            Release();
            return;
	}

	if (1 != HMAC_Init_ex(hmacctx, sKey.data(), static_cast<int>(sKey.size()), callback(), nullptr))
	{
		kDebug(1, "cannot initialize algorithm");
		Release();
	}
	else if (!sMessage.empty())
	{
		Update(sMessage);
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC::KHMAC(KHMAC&& other)
//---------------------------------------------------------------------------
	: hmacctx(other.hmacctx)
	, m_sHMAC(std::move(other.m_sHMAC))
{
	other.hmacctx = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KHMAC& KHMAC::operator=(KHMAC&& other)
//---------------------------------------------------------------------------
{
	Release();
	hmacctx = other.hmacctx;
	other.hmacctx = nullptr;
	m_sHMAC = std::move(other.m_sHMAC);
	return *this;
	
} // move ctor

//---------------------------------------------------------------------------
void KHMAC::Release()
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
#if OPENSSL_VERSION_NUMBER < 0x010100000
		HMAC_CTX_cleanup(hmacctx);
		delete hmacctx;
#else
		HMAC_CTX_free(hmacctx);
#endif
		hmacctx = nullptr;
	}
	m_sHMAC.clear();

} // Release

//---------------------------------------------------------------------------
bool KHMAC::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!hmacctx)
	{
		return false;
	}

	if (1 != HMAC_Update(hmacctx, reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()))
	{
		kDebug(1, "failed");
		return false;
	}

	return true;

} // Update

//---------------------------------------------------------------------------
bool KHMAC::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	if (!hmacctx)
	{
		return false;
	}

	std::array<unsigned char, KDefaultCopyBufSize> Buffer;

	for (;;)
	{
		auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());

		if (1 != HMAC_Update(hmacctx, Buffer.data(), iReadChunk))
		{
			kDebug(1, "failed");
			return false;
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
const KString& KHMAC::HMAC() const
//---------------------------------------------------------------------------
{
	if (m_sHMAC.empty())
	{
		std::array<unsigned char, EVP_MAX_MD_SIZE> Buffer;
		unsigned int iDigestLen;

		if (1 != HMAC_Final(hmacctx, Buffer.data(), &iDigestLen))
		{
			kDebug(1, "cannot read HMAC");
		}
		else
		{
			m_sHMAC.reserve(iDigestLen * 2);

			for (unsigned int iDigit = 0; iDigit < iDigestLen; ++iDigit)
			{
				KEncode::HexAppend(m_sHMAC, Buffer[iDigit]);
			}
		}
	}

	return m_sHMAC;

} // Digest

} // end of namespace dekaf2

#if OPENSSL_VERSION_NUMBER >= 0x030000000
#ifdef DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif
#ifdef DEKAF2_IS_CLANG
#pragma clang diagnostic pop
#endif
#endif
