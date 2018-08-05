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

/// @file khmac.h
/// HMAC message digests

#include <openssl/opensslv.h>
#include "kstream.h"
#include "kstringview.h"
#include "kstring.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KHMAC gives the interface for all HMAC algorithms. The
/// framework allows to calculate HMACs out of strings and streams.
class KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// copy construction
	KHMAC(const KHMAC&) = delete;
	/// move construction
	KHMAC(KHMAC&&);
	~KHMAC()
	{
		Release();
	}
	/// copy assignment
	KHMAC& operator=(const KHMAC&) = delete;
	/// move assignment
	KHMAC& operator=(KHMAC&&);

	/// appends a string to the digest
	bool Update(KStringView sInput);
	/// appends a string to the digest
	bool Update(KInStream& InputStream);
	/// appends a string to the digest
	KHMAC& operator+=(KStringView sInput)
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

	/// returns the HMAC
	const KString& HMAC() const;
	/// returns the HMAC
	operator const KString&() const
	{
		return HMAC();
	}
	/// returns the HMAC
	const KString& operator()() const
	{
		return HMAC();
	}

//------
protected:
//------

	/// default construction
	KHMAC();

	void Release();

	void* hmacctx { nullptr }; // is a HMAC_CTX
	mutable KString m_sHMAC;

}; // KHMAC

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_MD5 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_MD5(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_MD5(const KString& sKey, const KString& sMessage)
	: KHMAC_MD5(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_MD5(const char* sKey, const char* sMessage)
	: KHMAC_MD5(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_MD5

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_SHA1 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_SHA1(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_SHA1(const KString& sKey, const KString& sMessage)
	: KHMAC_SHA1(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_SHA1(const char* sKey, const char* sMessage)
	: KHMAC_SHA1(KStringView(sKey), KStringView(sMessage))
	{}

}; // KSHA1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_SHA224 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_SHA224(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_SHA224(const KString& sKey, const KString& sMessage)
	: KHMAC_SHA224(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_SHA224(const char* sKey, const char* sMessage)
	: KHMAC_SHA224(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_SHA224

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_SHA256 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_SHA256(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_SHA256(const KString& sKey, const KString& sMessage)
	: KHMAC_SHA256(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_SHA256(const char* sKey, const char* sMessage)
	: KHMAC_SHA256(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_SHA256

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_SHA384 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	//------
public:
	//------

	KHMAC_SHA384(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_SHA384(const KString& sKey, const KString& sMessage)
	: KHMAC_SHA384(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_SHA384(const char* sKey, const char* sMessage)
	: KHMAC_SHA384(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_SHA384

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_SHA512 : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_SHA512(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_SHA512(const KString& sKey, const KString& sMessage)
	: KHMAC_SHA512(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_SHA512(const char* sKey, const char* sMessage)
	: KHMAC_SHA512(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_SHA512

#if OPENSSL_VERSION_NUMBER >= 0x010100000

#define DEKAF2_HAS_KHMAC_BLAKE2 1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_BLAKE2S : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_BLAKE2S(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_BLAKE2S(const KString& sKey, const KString& sMessage)
	: KHMAC_BLAKE2S(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_BLAKE2S(const char* sKey, const char* sMessage)
	: KHMAC_BLAKE2S(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_BLAKE2S

using KHMAC256 = KHMAC_BLAKE2S;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHMAC_BLAKE2B : public KHMAC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHMAC_BLAKE2B(KStringView sKey, KStringView sMessage = KStringView{});
	KHMAC_BLAKE2B(const KString& sKey, const KString& sMessage)
	: KHMAC_BLAKE2B(KStringView(sKey), KStringView(sMessage))
	{}
	KHMAC_BLAKE2B(const char* sKey, const char* sMessage)
	: KHMAC_BLAKE2B(KStringView(sKey), KStringView(sMessage))
	{}

}; // KHMAC_BLAKE2B

using KHMAC512 = KHMAC_BLAKE2B;

#else

// With an older version of OpenSSL we use KHMAC_SHA2 as a KHMAC_BLAKE2 replacement.
// It is slower, but has probably the same security
using KHMAC256 = KHMAC_SHA256;
using KHMAC512 = KHMAC_SHA512;

#endif // of OPENSSL_VERSION_NUMBER >= 0x010100000

} // end of namespace dekaf2

