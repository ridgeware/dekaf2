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

#include "kstream.h"
#include "kstringstream.h"


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
	KMessageDigest(const KMessageDigest&) = default;
	/// move construction
	KMessageDigest(KMessageDigest&&) = default;
	~KMessageDigest();

	/// writes a string into the digest
	bool Update(KStringView sInput);
	/// writes a KInStream into the digest
	bool Update(KInStream& InputStream);

	const KString& Digest() const;
	const KString& Get() const
	{
		return Digest();
	}
	operator const KString&() const
	{
		return Digest();
	}

	/// writes a string into the digest
	KMessageDigest& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	void clear();

//------
protected:
//------

	/// default construction
	KMessageDigest();

	void Release();

	void* mdctx { nullptr }; // is a EVP_MD_CTX
	mutable KString m_sDigest;
	KString::size_type m_iDigestSize { 0 };

}; // KMessageDigest

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMD5 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KMD5(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KMD5

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA1 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA1(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KSHA1

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA224 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA224(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KSHA224

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA256 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA256(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KSHA256

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA384 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA384(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KSHA384

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSHA512 : public KMessageDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSHA512(KStringView sMessage = KStringView{});

//------
private:
//------


}; // KSHA512

} // end of namespace dekaf2

