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

#include "kblockcipher.h"

#if DEKAF2_HAS_AES

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES encryption/decryption. See class KBlockCipher for full documentation
///
/// A typical encryption setup may look like:
/// @code
/// KString sPlainText = "this is a secret message for your eyes only";
/// KStringView sPassword = "MySecretPassword";
///
/// KString sEncrypted;               ///< will take the ciphertext
/// KAES Encryptor(KAES::Encrypt);    ///< uses default mode GCM with 256 bits
/// Encryptor.SetThrowOnError(true);  ///< make sure we throw in case of errors
/// Encryptor.SetPassword(sPassword); ///< set key derived from password
/// Encryptor.SetOutput(sEncrypted);  ///< set the output
/// Encryptor.Add(sPlainText);        ///< add the plaintext
/// Encryptor.Finalize();             ///< finalize the encryption (also called by destructor)
/// @endcode
///
class DEKAF2_PUBLIC KAES : public KBlockCipher
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// AES encryption/decryption
	/// @param direction either Decrypt or Encrypt
	/// @param mode either ECB, CBC, OFB, CFB1, CFB8, CFB128, CTR, CCM, GCM, OCB or XTS - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KAES(KBlockCipher::Direction direction, KBlockCipher::Mode mode = KBlockCipher::GCM, KBlockCipher::Bits bits = KBlockCipher::B256)
	: KBlockCipher(direction, KBlockCipher::AES, mode, bits)
	{
	}

}; // KAES

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES encryption. See class KBlockCipher for full documentation
///
/// A typical encryption setup may look like:
/// @code
/// KString sPlainText = "this is a secret message for your eyes only";
/// KStringView sPassword = "MySecretPassword";
///
/// KString sEncrypted;               ///< will take the ciphertext
/// KToAES Encryptor;                 ///< uses default mode GCM with 256 bits
/// Encryptor.SetThrowOnError(true);  ///< make sure we throw in case of errors
/// Encryptor.SetPassword(sPassword); ///< set key derived from password
/// Encryptor.SetOutput(sEncrypted);  ///< set the output
/// Encryptor.Add(sPlainText);        ///< add the plaintext
/// Encryptor.Finalize();             ///< finalize the encryption (also called by destructor)
/// @endcode
///
class DEKAF2_PUBLIC KToAES : public KAES
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// AES encryption
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, OFB, CFB1, CFB8, CFB128, CTR, CCM, GCM, OCB or XTS - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KToAES(KBlockCipher::Mode mode = KBlockCipher::GCM, KBlockCipher::Bits bits = KBlockCipher::B256)
	: KAES(KBlockCipher::Encrypt, mode, bits)
	{
	}

}; // KToAES

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES decryption. See class KBlockCipher for full documentation
///
/// A typical decryption setup may look like:
/// @code
/// KString sEncrypted;               ///< fill it with the ciphertext
/// KStringView sPassword = "MySecretPassword";
///
/// KString sDecrypted;               ///< will take the plaintext
/// KFromAES Decryptor(sDecrypted);   ///< uses default mode GCM with 256 bits
/// Decryptor.SetThrowOnError(true);  ///< make sure we throw in case of errors
/// Decryptor.SetPassword(sPassword); ///< set key derived from password
/// Decryptor.Add(sEncrypted);        ///< add the ciphertext
/// Decryptor.Finalize();             ///< finalize the decryption (also called by destructor)
/// @endcode
///
class DEKAF2_PUBLIC KFromAES : public KAES
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// AES decryption
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, OFB, CFB1, CFB8, CFB128, CTR, CCM, GCM, OCB or XTS - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KFromAES(KBlockCipher::Mode mode = KBlockCipher::GCM, KBlockCipher::Bits bits = KBlockCipher::B256)
	: KAES(KBlockCipher::Decrypt, mode, bits)
	{
	}

}; // KFromAES

DEKAF2_NAMESPACE_END

#endif
