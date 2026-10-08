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

#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/core/logging/klog.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <array>

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
	evpctx.reset(::EVP_MD_CTX_create());
#else
	evpctx.reset(::EVP_MD_CTX_new());
#endif

	if (!evpctx)
	{
		SetError(GetOpenSSLError("cannot create context"));
		return;
	}

	if (1 != ::EVP_SignInit(evpctx.get(), GetMessageDigest(digest)))
	{
		Release();
		SetError(GetOpenSSLError("cannot initialize algorithm"));
	}

} // ctor

#if OPENSSL_VERSION_NUMBER >= 0x010100000L
//---------------------------------------------------------------------------
void KMessageDigestBase::FreeContext(evp_md_ctx_st* pContext)
//---------------------------------------------------------------------------
{
	::EVP_MD_CTX_free(pContext);

} // FreeContext
#else
//---------------------------------------------------------------------------
void KMessageDigestBase::FreeContext(env_md_ctx_st* pContext)
//---------------------------------------------------------------------------
{
	::EVP_MD_CTX_destroy(pContext);

} // FreeContext
#endif

//---------------------------------------------------------------------------
void KMessageDigestBase::Release() noexcept
//---------------------------------------------------------------------------
{
	evpctx.reset();

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
	const EVP_MD* md = ::EVP_MD_CTX_md(evpctx.get());
#else
	const EVP_MD* md = ::EVP_MD_CTX_get0_md(evpctx.get());
#endif

	if (1 != ::EVP_DigestInit_ex(evpctx.get(), md, nullptr))
	{
		Release();
		SetError("failed");
		return;
	}

} // clear

//---------------------------------------------------------------------------
bool KMessageDigestBase::Update(const void* pAddress, std::size_t iSize)
//---------------------------------------------------------------------------
{
	if (!evpctx)
	{
		return false;
	}

	if (1 != Updater(evpctx.get(), pAddress, iSize))
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

		if (1 != Updater(evpctx.get(), Buffer.data(), iReadChunk))
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
	// without a context (after a failed construction, or after a move) the digest
	// stays empty
	if (m_sDigest.empty() && evpctx)
	{
		std::array<unsigned char, EVP_MAX_MD_SIZE> Buffer;
		unsigned int iDigestLen;

		if (1 != ::EVP_DigestFinal_ex(evpctx.get(), Buffer.data(), &iDigestLen))
		{
			SetError(GetOpenSSLError("cannot read digest"));
		}
		else
		{
			m_sDigest.append(reinterpret_cast<const char*>(Buffer.data()), iDigestLen);
		}

		OPENSSL_cleanse(Buffer.data(), Buffer.size());
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

static_assert(std::is_nothrow_move_assignable<detail::KMessageDigestBase>::value,
			  "KMessageDigestBase is intended to be nothrow move assignable, but is not!");

static_assert(std::is_nothrow_move_constructible<KMessageDigest>::value,
			  "KMessageDigest is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<KMD5>::value,
			  "KMD5 is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
