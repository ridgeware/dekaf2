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

#pragma once

/// @file kmessagedigest.h
/// (cryptographic) message digests

#include <openssl/opensslv.h>
#include "kstream.h"
#include "kstringview.h"
#include "kstring.h"


#if OPENSSL_VERSION_NUMBER < 0x010100000
struct env_md_ctx_st;
#else
struct evp_md_ctx_st;
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMessageDigestBase constructs the basic algorithms for message digest
/// computations, used by KMessageDigest and KRSASign
class DEKAF2_PUBLIC KMessageDigestBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum ALGORITHM
	{
		NONE,
		MD5,
		SHA1,
		SHA224,
		SHA256,
		SHA384,
		SHA512,
#if OPENSSL_VERSION_NUMBER >= 0x010100000
		BLAKE2S,
		BLAKE2B,
#endif
	};

	/// copy construction
	KMessageDigestBase(const KMessageDigestBase&) = delete;
	/// move construction
	KMessageDigestBase(KMessageDigestBase&&) noexcept;
	// destruction
	~KMessageDigestBase()
	{
		Release();
	}
	/// copy assignment
	KMessageDigestBase& operator=(const KMessageDigestBase&) = delete;
	/// move assignment
	KMessageDigestBase& operator=(KMessageDigestBase&&) noexcept;

	/// appends a string to the digest
	bool Update(KStringView sInput);
	/// appends a stream to the digest
	bool Update(KInStream& InputStream);
	/// appends a stream to the digest
	bool Update(KInStream&& InputStream);
	/// appends a string to the digest
	KMessageDigestBase& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}
	/// appends a string to the digest
	void operator()(KStringView sInput)
	{
		Update(sInput);
	}
	/// appends a stream to the digest
	void operator()(KInStream& InputStream)
	{
		Update(InputStream);
	}

//------
protected:
//------

	// typedef for an EVP_Update function
	using UpdateFunc = int(*)(void*, const void*, size_t);

	/// construction
	KMessageDigestBase(ALGORITHM Algorithm, UpdateFunc _Updater);

	/// prepares for new computation
	void clear();

	/// releases context
	void Release() noexcept;

#if OPENSSL_VERSION_NUMBER < 0x010100000
	env_md_ctx_st* evpctx { nullptr }; // is a ENV_MD_CTX
#else
	evp_md_ctx_st* evpctx { nullptr }; // is a EVP_MD_CTX
#endif
	UpdateFunc Updater { nullptr }; // is a EVP_Update function

}; // KMessageDigest

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMessageDigest gives the interface for all message digest algorithms. The
/// framework allows to calculate digests out of strings and streams.
class DEKAF2_PUBLIC KMessageDigest : public KMessageDigestBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// construction
	KMessageDigest(ALGORITHM Algorithm, KStringView sMessage = KStringView{});

	/// copy construction
	KMessageDigest(const KMessageDigest&) = delete;
	/// move construction
	KMessageDigest(KMessageDigest&&) = default;

	/// copy assignment
	KMessageDigest& operator=(const KMessageDigest&) = delete;
	/// move assignment
	KMessageDigest& operator=(KMessageDigest&&) = default;

	/// returns the message digest in binary encoding
	const KString& Digest() const;

	/// returns the message digest in hexadecimal encoding
	KString HexDigest() const;

	/// clears the digest and prepares for new computation
	void clear();

//------
protected:
//------

	mutable KString m_sDigest;

}; // KMessageDigest

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KMD5 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KMD5(KStringView sMessage = KStringView{})
	: KMessageDigest(MD5, sMessage)
	{}

	KMD5& operator=(KStringView sMessage)
	{
		*this = KMD5(sMessage);
		return *this;
	}

}; // KMD5

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSHA1 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA1(KStringView sMessage = KStringView{})
	: KMessageDigest(SHA1, sMessage)
	{}

	KSHA1& operator=(KStringView sMessage)
	{
		*this = KSHA1(sMessage);
		return *this;
	}

}; // KSHA1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSHA224 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA224(KStringView sMessage = KStringView{})
	: KMessageDigest(SHA224, sMessage)
	{}

	KSHA224& operator=(KStringView sMessage)
	{
		*this = KSHA224(sMessage);
		return *this;
	}

}; // KSHA224

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSHA256 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA256(KStringView sMessage = KStringView{})
	: KMessageDigest(SHA256, sMessage)
	{}

	KSHA256& operator=(KStringView sMessage)
	{
		*this = KSHA256(sMessage);
		return *this;
	}

}; // KSHA256

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSHA384 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA384(KStringView sMessage = KStringView{})
	: KMessageDigest(SHA384, sMessage)
	{}

	KSHA384& operator=(KStringView sMessage)
	{
		*this = KSHA384(sMessage);
		return *this;
	}

}; // KSHA384

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSHA512 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA512(KStringView sMessage = KStringView{})
	: KMessageDigest(SHA512, sMessage)
	{}

	KSHA512& operator=(KStringView sMessage)
	{
		*this = KSHA512(sMessage);
		return *this;
	}

}; // KSHA512

#if OPENSSL_VERSION_NUMBER >= 0x010100000

#define DEKAF2_HAS_BLAKE2 1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBLAKE2S : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KBLAKE2S(KStringView sMessage = KStringView{})
	: KMessageDigest(BLAKE2S, sMessage)
	{}

	KBLAKE2S& operator=(KStringView sMessage)
	{
		*this = KBLAKE2S(sMessage);
		return *this;
	}

}; // KBLAKE2S

using KHASH256 = KBLAKE2S;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBLAKE2B : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KBLAKE2B(KStringView sMessage = KStringView{})
	: KMessageDigest(BLAKE2B, sMessage)
	{}

	KBLAKE2B& operator=(KStringView sMessage)
	{
		*this = KBLAKE2B(sMessage);
		return *this;
	}

}; // KBLAKE2B

using KHASH512 = KBLAKE2B;

#else

// With an older version of OpenSSL we use SHA2 as a BLAKE2 replacement.
// It is slower, but has probably the same security
using KHASH256 = KSHA256;
using KHASH512 = KSHA512;

#endif // of OPENSSL_VERSION_NUMBER >= 0x010100000

DEKAF2_NAMESPACE_END

