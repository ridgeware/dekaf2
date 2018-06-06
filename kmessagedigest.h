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
#include "kstringstream.h"
#include "kstringview.h"
#include "kstring.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KMessageDigest gives the interface for all message digest algorithms. The
/// framework allows to calculate digests out of strings and streams.
class KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// copy construction
	KMessageDigest(const KMessageDigest&) = delete;
	/// move construction
	KMessageDigest(KMessageDigest&&);
	~KMessageDigest()
	{
		Release();
	}
	/// copy assignment
	KMessageDigest& operator=(const KMessageDigest&) = delete;
	/// move assignment
	KMessageDigest& operator=(KMessageDigest&&);

	/// appends a string to the digest
	bool Update(KStringView sInput);
	/// appends a string to the digest
	bool Update(KInStream& InputStream);
	/// appends a string to the digest
	KMessageDigest& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	/// returns the message digest
	const KString& Digest() const;
	/// returns the message digest
	operator const KString&() const
	{
		return Digest();
	}

	/// clears the digest and prepares for new computation
	void clear();

//------
protected:
//------

	/// default construction
	KMessageDigest();

	void Release();

	void* mdctx { nullptr }; // is a EVP_MD_CTX
	mutable KString m_sDigest;

}; // KMessageDigest

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMD5 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KMD5(KStringView sMessage = KStringView{});
	KMD5(const KString& sMessage)
	: KMD5(KStringView(sMessage))
	{}
	KMD5(const char* sMessage)
	: KMD5(KStringView(sMessage))
	{}

}; // KMD5

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA1 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA1(KStringView sMessage = KStringView{});
	KSHA1(const KString& sMessage)
	: KSHA1(KStringView(sMessage))
	{}
	KSHA1(const char* sMessage)
	: KSHA1(KStringView(sMessage))
	{}

}; // KSHA1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA224 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA224(KStringView sMessage = KStringView{});
	KSHA224(const KString& sMessage)
	: KSHA224(KStringView(sMessage))
	{}
	KSHA224(const char* sMessage)
	: KSHA224(KStringView(sMessage))
	{}

}; // KSHA224

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA256 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA256(KStringView sMessage = KStringView{});
	KSHA256(const KString& sMessage)
	: KSHA256(KStringView(sMessage))
	{}
	KSHA256(const char* sMessage)
	: KSHA256(KStringView(sMessage))
	{}

}; // KSHA256

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA384 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA384(KStringView sMessage = KStringView{});
	KSHA384(const KString& sMessage)
	: KSHA384(KStringView(sMessage))
	{}
	KSHA384(const char* sMessage)
	: KSHA384(KStringView(sMessage))
	{}

}; // KSHA384

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA512 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA512(KStringView sMessage = KStringView{});
	KSHA512(const KString& sMessage)
	: KSHA512(KStringView(sMessage))
	{}
	KSHA512(const char* sMessage)
	: KSHA512(KStringView(sMessage))
	{}

}; // KSHA512

#if OPENSSL_VERSION_NUMBER >= 0x010100000

#define DEKAF2_HAS_BLAKE2 1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBLAKE2S : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KBLAKE2S(KStringView sMessage = KStringView{});
	KBLAKE2S(const KString& sMessage)
	: KBLAKE2S(KStringView(sMessage))
	{}
	KBLAKE2S(const char* sMessage)
	: KBLAKE2S(KStringView(sMessage))
	{}

}; // KBLAKE2S

using KHASH256 = KBLAKE2S;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBLAKE2B : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KBLAKE2B(KStringView sMessage = KStringView{});
	KBLAKE2B(const KString& sMessage)
	: KBLAKE2B(KStringView(sMessage))
	{}
	KBLAKE2B(const char* sMessage)
	: KBLAKE2B(KStringView(sMessage))
	{}

}; // KBLAKE2B

using KHASH512 = KBLAKE2B;

#else

// With an older version of OpenSSL we use SHA2 as a BLAKE2 replacement.
// It is slower, but has probably the same security
using KHASH256 = KSHA256;
using KHASH512 = KSHA512;

#endif // of OPENSSL_VERSION_NUMBER >= 0x010100000

} // end of namespace dekaf2

