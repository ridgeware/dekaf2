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

#include <openssl/evp.h>
#include "kmessagedigest.h"
#include "klog.h"

namespace dekaf2 {


//---------------------------------------------------------------------------
KMessageDigest::KMessageDigest()
//---------------------------------------------------------------------------
{
	// 0x000906000 == 0.9.6 dev
	// 0x010100000 == 1.1.0
#if OPENSSL_VERSION_NUMBER < 0x010100000
	mdctx = EVP_MD_CTX_create();
#else
	mdctx = EVP_MD_CTX_new();
#endif

} // ctor

//---------------------------------------------------------------------------
KMessageDigest::KMessageDigest(KMessageDigest&& other)
//---------------------------------------------------------------------------
	: mdctx(other.mdctx)
	, m_sDigest(std::move(other.m_sDigest))
{
	other.mdctx = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KMessageDigest& KMessageDigest::operator=(KMessageDigest&& other)
//---------------------------------------------------------------------------
{
	Release();
	mdctx = other.mdctx;
	other.mdctx = nullptr;
	m_sDigest = std::move(other.m_sDigest);
	return *this;
	
} // move ctor

//---------------------------------------------------------------------------
void KMessageDigest::clear()
//---------------------------------------------------------------------------
{
	m_sDigest.clear();

	if (!mdctx)
	{
		return;
	}

	const EVP_MD* md = EVP_MD_CTX_md(static_cast<EVP_MD_CTX*>(mdctx));

	if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), md, nullptr))
	{
		kDebugLog(1, "MessageDigest: cannot clear digest");
		Release();
		return;
	}

} // clear

//---------------------------------------------------------------------------
void KMessageDigest::Release()
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
#if OPENSSL_VERSION_NUMBER < 0x010100000
		EVP_MD_CTX_destroy(static_cast<EVP_MD_CTX*>(mdctx));
#else
		EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(mdctx));
#endif
		mdctx = nullptr;
	}

} // Release

//---------------------------------------------------------------------------
bool KMessageDigest::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!mdctx)
	{
		return false;
	}

	if (1 != EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(mdctx), sInput.data(), sInput.size()))
	{
		kDebugLog(1, "MessageDigest: cannot update digest");
		return false;
	}

	return true;

} // Update

//---------------------------------------------------------------------------
bool KMessageDigest::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	if (!mdctx)
	{
		return false;
	}

	enum { BLOCKSIZE = 4096 };
	unsigned char sBuffer[BLOCKSIZE];

	for (;;)
	{
		auto iReadChunk = InputStream.Read(sBuffer, BLOCKSIZE);
		if (1 != EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(mdctx), sBuffer, iReadChunk))
		{
			kDebugLog(1, "MessageDigest: cannot update digest");
			return false;
		}
		if (iReadChunk < BLOCKSIZE)
		{
			return true;
		}
	}

} // Update


//---------------------------------------------------------------------------
const KString& KMessageDigest::Digest() const
//---------------------------------------------------------------------------
{
	static constexpr char s_xDigit[] = "0123456789abcdef";

	if (m_sDigest.empty())
	{
		unsigned int iDigestLen;
		unsigned char sBuffer[EVP_MAX_MD_SIZE];
		if (1 != EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(mdctx), sBuffer, &iDigestLen))
		{
			kDebugLog(1, "MessageDigest: cannot read digest");
		}
		else
		{
			m_sDigest.reserve(iDigestLen * 2);
			for (unsigned int iDigit = 0; iDigit < iDigestLen; ++iDigit)
			{
				int ch = sBuffer[iDigit];
				m_sDigest += static_cast<char> (s_xDigit[(ch>>4)&0xf]);
				m_sDigest += static_cast<char> (s_xDigit[(ch   )&0xf]);
			}
		}
	}

	return m_sDigest;

} // Digest


//---------------------------------------------------------------------------
KMD5::KMD5(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_md5(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "MD5");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KSHA1::KSHA1(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_sha1(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "SHA1");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KSHA224::KSHA224(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_sha224(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "SHA224");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KSHA256::KSHA256(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_sha256(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "SHA256");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KSHA384::KSHA384(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_sha384(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "SHA384");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KSHA512::KSHA512(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_sha512(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "SHA512");
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
KBLAKE2S::KBLAKE2S(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_blake2s256(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "BLAKE2S");
			Release();
		}
		else if (!sMessage.empty())
		{
			Update(sMessage);
		}
	}

} // ctor

//---------------------------------------------------------------------------
KBLAKE2B::KBLAKE2B(KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (mdctx)
	{
		if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(mdctx), EVP_blake2b512(), nullptr))
		{
			kDebugLog(1, "{}: cannot initialize digest context", "BLAKE2B");
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


