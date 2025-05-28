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

#include "kmessagedigest.h"
#include "kencode.h"
#include "klog.h"
#include <openssl/evp.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//---------------------------------------------------------------------------
KMessageDigestBase::KMessageDigestBase(Digest digest, UpdateFunc _Updater)
//---------------------------------------------------------------------------
: Updater(_Updater)
{
	// 0x000906000 == 0.9.6 dev
	// 0x010100000 == 1.1.0
#if OPENSSL_VERSION_NUMBER < 0x010100000
	evpctx = ::EVP_MD_CTX_create();
#else
	evpctx = ::EVP_MD_CTX_new();
#endif

	if (!evpctx)
	{
		Release();
		SetError(GetOpenSSLError("cannot create context"));
		return;
	}

	if (1 != ::EVP_SignInit(evpctx, GetMessageDigest(digest)))
	{
		Release();
		SetError(GetOpenSSLError("cannot initialize algorithm"));
	}

} // ctor

//---------------------------------------------------------------------------
void KMessageDigestBase::Release() noexcept
//---------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION

	if (evpctx)
	{
#if OPENSSL_VERSION_NUMBER < 0x010100000
		::EVP_MD_CTX_destroy(evpctx);
#else
		::EVP_MD_CTX_free(evpctx);
#endif
		evpctx = nullptr;
	}

	DEKAF2_LOG_EXCEPTION

} // Release

//---------------------------------------------------------------------------
void KMessageDigestBase::clear()
//---------------------------------------------------------------------------
{
	if (!evpctx)
	{
		return;
	}

#if OPENSSL_VERSION_NUMBER < 0x030000000
	const EVP_MD* md = ::EVP_MD_CTX_md(evpctx);
#else
	const EVP_MD* md = ::EVP_MD_CTX_get0_md(evpctx);
#endif

	if (1 != ::EVP_DigestInit_ex(evpctx, md, nullptr))
	{
		Release();
		SetError("failed");
		return;
	}

} // clear

//---------------------------------------------------------------------------
KMessageDigestBase::KMessageDigestBase(KMessageDigestBase&& other) noexcept
//---------------------------------------------------------------------------
: evpctx(other.evpctx)
{
	other.evpctx = nullptr;

} // move ctor

//---------------------------------------------------------------------------
KMessageDigestBase& KMessageDigestBase::operator=(KMessageDigestBase&& other) noexcept
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
		return SetError(GetOpenSSLError("update failed"));
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

	std::array<unsigned char, KDefaultCopyBufSize> Buffer;

	for (;;)
	{
		auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());

		if (1 != Updater(evpctx, Buffer.data(), iReadChunk))
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
bool KMessageDigestBase::Update(KInStream&& InputStream)
//---------------------------------------------------------------------------
{
	return Update(InputStream);

} // Update

} // end of namespace detail

//---------------------------------------------------------------------------
KMessageDigest::KMessageDigest(enum Digest digest, KStringView sMessage)
//---------------------------------------------------------------------------
	: KMessageDigestBase(digest, reinterpret_cast<UpdateFunc>(::EVP_DigestUpdate))
{
	if (!sMessage.empty())
	{
		Update(sMessage);
	}
}

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
	if (m_sDigest.empty())
	{
		std::array<unsigned char, EVP_MAX_MD_SIZE> Buffer;
		unsigned int iDigestLen;

		if (1 != ::EVP_DigestFinal_ex(evpctx, Buffer.data(), &iDigestLen))
		{
			SetError(GetOpenSSLError("cannot read digest"));
		}
		else
		{
			m_sDigest.reserve(iDigestLen);
			
			for (unsigned int iDigit = 0; iDigit < iDigestLen; ++iDigit)
			{
				m_sDigest += Buffer[iDigit];
			}
		}
	}

	return m_sDigest;

} // Digest

//---------------------------------------------------------------------------
KString KMessageDigest::HexDigest() const
//---------------------------------------------------------------------------
{
	return KEncode::Hex(Digest());

} // HexDigest

static_assert(std::is_nothrow_move_constructible<detail::KMessageDigestBase>::value,
			  "KMessageDigestBase is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<KMessageDigest>::value,
			  "KMessageDigest is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<KMD5>::value,
			  "KMD5 is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
