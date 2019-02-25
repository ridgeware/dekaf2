/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2019, Ridgeware, Inc.
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

/// @file krsasign.h
/// RSA signatures

#include "kstream.h"
#include "kstringview.h"
#include "kstring.h"
#include "krsakey.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RSA base class to init context and keys
class KRSABase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum ALGORITHM
	{
		NONE,
		SHA256,
		SHA384,
		SHA512
	};

	/// copy construction
	KRSABase(const KRSABase&) = delete;
	/// move construction
	KRSABase(KRSABase&&);
	~KRSABase()
	{
		Release();
	}
	/// copy assignment
	KRSABase& operator=(const KRSABase&) = delete;
	/// move assignment
	KRSABase& operator=(KRSABase&&);

//------
protected:
//------

	KRSABase(ALGORITHM Algorithm, KStringView sPubKey, KStringView sPrivKey = KStringView{});
	KRSABase(ALGORITHM Algorithm, KRSAKey& Key);

	void Release();

	void* evpctx { nullptr }; // is a EVP_MD_CTX
	void* evppkey { nullptr }; // is a EVP_PKEY

//------
private:
//------

	void InitAlgorithm(ALGORITHM Algorithm);

	bool m_bOwnPointers { false };

}; // KRSABase

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KRSASign gives the interface for all RSA signature algorithms. The
/// framework allows to calculate signatures out of strings and streams.
class KRSASign : public KRSABase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KRSASign(ALGORITHM Algorithm, KStringView sPubKey, KStringView sPrivKey, KStringView sMessage = KStringView{});

	KRSASign(ALGORITHM Algorithm, KRSAKey& Key, KStringView sMessage = KStringView{});

	/// copy construction
	KRSASign(const KRSASign&) = delete;
	/// move construction
	KRSASign(KRSASign&&);
	/// copy assignment
	KRSASign& operator=(const KRSASign&) = delete;
	/// move assignment
	KRSASign& operator=(KRSASign&&);

	/// appends a string to the digest
	bool Update(KStringView sInput);
	/// appends a stream to the digest
	bool Update(KInStream& InputStream);
	/// appends a stream to the digest
	bool Update(KInStream&& InputStream);
	/// appends a string to the digest
	KRSASign& operator+=(KStringView sInput)
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

	/// returns the signature
	const KString& Signature() const;
	/// returns the signature
	operator const KString&() const
	{
		return Signature();
	}
	/// returns the signature
	const KString& operator()() const
	{
		return Signature();
	}

//------
protected:
//------

	mutable KString m_sSignature;

}; // KRSASign

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KRSAVerify gives the interface for all RSA signature verification algorithms. The
/// framework allows to calculate signatures out of strings and streams.
class KRSAVerify : public KRSABase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KRSAVerify(ALGORITHM Algorithm, KStringView sPubKey, KStringView sMessage = KStringView{});

	KRSAVerify(ALGORITHM Algorithm, KRSAKey& Key, KStringView sMessage = KStringView{});

	/// copy construction
	KRSAVerify(const KRSAVerify&) = delete;
	/// move construction
	KRSAVerify(KRSAVerify&&);
	/// copy assignment
	KRSAVerify& operator=(const KRSAVerify&) = delete;
	/// move assignment
	KRSAVerify& operator=(KRSAVerify&&);

	/// appends a string to the digest
	bool Update(KStringView sInput);
	/// appends a stream to the digest
	bool Update(KInStream& InputStream);
	/// appends a stream to the digest
	bool Update(KInStream&& InputStream);
	/// appends a string to the digest
	KRSAVerify& operator+=(KStringView sInput)
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

	/// verifies the signature
	bool Verify(KStringView sSignature) const;

//------
protected:
//------

	mutable KString m_sSignature;

}; // KRSASign

} // end of namespace dekaf2

