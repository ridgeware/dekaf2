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

#include <openssl/hmac.h>
#include "khmac.h"
#include "klog.h"

namespace dekaf2 {


//---------------------------------------------------------------------------
KHMAC::KHMAC()
//---------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER < 0x010100000
	hmacctx = new HMAC_CTX();
#else
	hmacctx = HMAC_CTX_new();
#endif

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
		HMAC_CTX_cleanup(static_cast<HMAC_CTX*>(hmacctx));
		delete static_cast<HMAC_CTX*>(hmacctx);
#else
		HMAC_CTX_free(static_cast<HMAC_CTX*>(hmacctx));
#endif
		hmacctx = nullptr;
	}

} // Release

//---------------------------------------------------------------------------
bool KHMAC::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!hmacctx)
	{
		return false;
	}

	if (1 != HMAC_Update(static_cast<HMAC_CTX*>(hmacctx), reinterpret_cast<const unsigned char*>(sInput.data()), sInput.size()))
	{
		kDebugLog(1, "HMAC: cannot update HMAC");
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

	enum { BLOCKSIZE = 4096 };
	unsigned char sBuffer[BLOCKSIZE];

	for (;;)
	{
		auto iReadChunk = InputStream.Read(sBuffer, BLOCKSIZE);
		if (1 != HMAC_Update(static_cast<HMAC_CTX*>(hmacctx), sBuffer, iReadChunk))
		{
			kDebugLog(1, "HMAC: cannot update HMAC");
			return false;
		}
		if (iReadChunk < BLOCKSIZE)
		{
			return true;
		}
	}

} // Update


//---------------------------------------------------------------------------
const KString& KHMAC::HMAC() const
//---------------------------------------------------------------------------
{
	static constexpr char s_xDigit[] = "0123456789abcdef";

	if (m_sHMAC.empty())
	{
		unsigned int iDigestLen;
		unsigned char sBuffer[EVP_MAX_MD_SIZE];
		if (1 != HMAC_Final(static_cast<HMAC_CTX*>(hmacctx), sBuffer, &iDigestLen))
		{
			kDebugLog(1, "HMAC: cannot read HMAC");
		}
		else
		{
			m_sHMAC.reserve(iDigestLen * 2);
			for (unsigned int iDigit = 0; iDigit < iDigestLen; ++iDigit)
			{
				int ch = sBuffer[iDigit];
				m_sHMAC += static_cast<char> (s_xDigit[(ch>>4)&0xf]);
				m_sHMAC += static_cast<char> (s_xDigit[(ch   )&0xf]);
			}
		}
	}

	return m_sHMAC;

} // Digest


//---------------------------------------------------------------------------
KHMAC_MD5::KHMAC_MD5(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_md5(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_MD5");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_SHA1::KHMAC_SHA1(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_sha1(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_SHA1");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_SHA224::KHMAC_SHA224(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_sha224(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_SHA224");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_SHA256::KHMAC_SHA256(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_sha256(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_SHA256");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_SHA384::KHMAC_SHA384(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_sha384(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_SHA384");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_SHA512::KHMAC_SHA512(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_sha512(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_SHA512");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

#if OPENSSL_VERSION_NUMBER >= 0x010100000
//---------------------------------------------------------------------------
KHMAC_BLAKE2S::KHMAC_BLAKE2S(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_blake2s256(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_BLAKE2S");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KHMAC_BLAKE2B::KHMAC_BLAKE2B(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (hmacctx)
	{
		if (1 != HMAC_Init_ex(static_cast<HMAC_CTX*>(hmacctx), sKey.data(), sKey.size(), EVP_blake2b512(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize HMAC context", "KHMAC_BLAKE2B");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor
#endif

} // end of namespace dekaf2


