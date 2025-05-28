/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

/// @file krsa.h
/// RSA encryption/decryption

#include "kstringview.h"
#include "kstring.h"
#include "krsakey.h"
#include "kerror.h"

struct evp_pkey_ctx_st;

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KRSAEncrypt : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// construct an RSA encryptor
	/// @param Key the (public) RSA key to use for encryption
	/// @param bEME_OAP use secure EME_OAP padding, or vulnerable
	/// non-OAEP padding (for older implementations). Defaults to true for EME_OAP.
	KRSAEncrypt(const KRSAKey& Key, bool bEME_OAEP = true);
	~KRSAEncrypt();

	/// encrypt with RSA
	/// @param sInput the input data to encrypt
	/// @return the ciphertext
	KString Encrypt(KStringView sInput) const;

	/// call operator returns the encrypted input
	KString operator()(KStringView sInput) const
	{
		return Encrypt(sInput);
	}

//------
private:
//------

	evp_pkey_ctx_st* m_ctx { nullptr };

}; // KRSAEncrypt

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KRSADecrypt : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// construct an RSA decryptor
	/// @param Key the (private) RSA key to use for decryption
	/// @param bEME_OAP use secure EME_OAP padding, or vulnerable
	/// non-OAEP padding (for older implementations). Defaults to true for EME_OAP.
	KRSADecrypt(const KRSAKey& Key, bool bEME_OAEP = true);
	~KRSADecrypt();

	/// decrypt with RSA
	/// @param sInput the ciphertext data to decrypt
	/// @return the plaintext
	KString Decrypt(KStringView sInput) const;

	/// call operator returns the decrypted input
	KString operator()(KStringView sInput) const
	{
		return Decrypt(sInput);
	}

//------
private:
//------

	evp_pkey_ctx_st* m_ctx { nullptr };

}; // KRSADecrypt


DEKAF2_NAMESPACE_END
