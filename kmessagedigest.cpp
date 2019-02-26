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
KMessageDigestBase::KMessageDigestBase(ALGORITHM Algorithm, UpdateFunc _Updater)
//---------------------------------------------------------------------------
	: Updater(_Updater)
{
	// 0x000906000 == 0.9.6 dev
	// 0x010100000 == 1.1.0
#if OPENSSL_VERSION_NUMBER < 0x010100000
	evpctx = EVP_MD_CTX_create();
#else
	evpctx = EVP_MD_CTX_new();
#endif

	if (!evpctx)
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
			break;
	}

	if (1 != EVP_SignInit(static_cast<EVP_MD_CTX*>(evpctx), callback()))
	{
		kDebug(1, "cannot initialize algorithm");
		Release();
	}

} // ctor

//---------------------------------------------------------------------------
void KMessageDigestBase::Release()
//---------------------------------------------------------------------------
{
	if (evpctx)
	{
#if OPENSSL_VERSION_NUMBER < 0x010100000
		EVP_MD_CTX_destroy(static_cast<EVP_MD_CTX*>(evpctx));
#else
		EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(evpctx));
#endif
		evpctx = nullptr;
	}

} // Release

//---------------------------------------------------------------------------
void KMessageDigestBase::clear()
//---------------------------------------------------------------------------
{
	if (!evpctx)
	{
		return;
	}

	const EVP_MD* md = EVP_MD_CTX_md(static_cast<EVP_MD_CTX*>(evpctx));

	if (1 != EVP_DigestInit_ex(static_cast<EVP_MD_CTX*>(evpctx), md, nullptr))
	{
		kDebug(1, "failed");
		Release();
		return;
	}

} // clear

//---------------------------------------------------------------------------
KMessageDigestBase::KMessageDigestBase(KMessageDigestBase&& other)
//---------------------------------------------------------------------------
	: evpctx(other.evpctx)
{
	other.evpctx = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KMessageDigestBase& KMessageDigestBase::operator=(KMessageDigestBase&& other)
//---------------------------------------------------------------------------
{
	Release();
	evpctx = other.evpctx;
	other.evpctx = nullptr;
	return *this;

} // move ctor

//---------------------------------------------------------------------------
bool KMessageDigestBase::Update(KStringView sInput)
//---------------------------------------------------------------------------
{
	if (!evpctx)
	{
		return false;
	}

	if (1 != Updater(evpctx, sInput.data(), sInput.size()))
	{
		kDebug(1, "failed");
		return false;
	}

	return true;

} // Update

//---------------------------------------------------------------------------
bool KMessageDigestBase::Update(KInStream& InputStream)
//---------------------------------------------------------------------------
{
	if (!evpctx)
	{
		return false;
	}

	enum { BLOCKSIZE = 4096 };
	unsigned char sBuffer[BLOCKSIZE];

	for (;;)
	{
		auto iReadChunk = InputStream.Read(sBuffer, BLOCKSIZE);
		if (1 != Updater(evpctx, sBuffer, iReadChunk))
		{
			kDebug(1, "failed");
			return false;
		}
		if (iReadChunk < BLOCKSIZE)
		{
			return true;
		}
	}

} // Update

//---------------------------------------------------------------------------
bool KMessageDigestBase::Update(KInStream&& InputStream)
//---------------------------------------------------------------------------
{
	return Update(InputStream);

} // Update

//---------------------------------------------------------------------------
KMessageDigest::KMessageDigest(ALGORITHM Algorithm, KStringView sMessage)
//---------------------------------------------------------------------------
	: KMessageDigestBase(Algorithm, reinterpret_cast<UpdateFunc>(EVP_DigestUpdate))
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}
}

//---------------------------------------------------------------------------
KMessageDigest::KMessageDigest(KMessageDigest&& other)
//---------------------------------------------------------------------------
	: KMessageDigestBase(std::move(other))
	, m_sDigest(std::move(other.m_sDigest))
{
} // move ctor

//---------------------------------------------------------------------------
KMessageDigest& KMessageDigest::operator=(KMessageDigest&& other)
//---------------------------------------------------------------------------
{
	KMessageDigestBase::operator=(std::move(other));
	m_sDigest = std::move(other.m_sDigest);
	return *this;
	
} // move ctor

//---------------------------------------------------------------------------
void KMessageDigest::clear()
//---------------------------------------------------------------------------
{
	KMessageDigestBase::clear();
	m_sDigest.clear();

} // clear

//---------------------------------------------------------------------------
const KString& KMessageDigest::Digest() const
//---------------------------------------------------------------------------
{
	static constexpr char s_xDigit[] = "0123456789abcdef";

	if (m_sDigest.empty())
	{
		unsigned int iDigestLen;
		unsigned char sBuffer[EVP_MAX_MD_SIZE];
		if (1 != EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(evpctx), sBuffer, &iDigestLen))
		{
			kDebug(1, "cannot read digest");
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

} // end of namespace dekaf2


