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

/// @file kaes.h
/// symmetrical AES encryption/decryption

#include "kstream.h"
#include "kstringview.h"
#include "kstring.h"
#include "kerror.h"
#include "bits/kdigest.h"

// PKCS5_PBKDF2_HMAC was introduced with OpenSSL v1.0.2
#if OPENSSL_VERSION_NUMBER >= 0x010002000L

#define DEKAF2_HAS_AES 1

struct evp_cipher_st;
struct evp_cipher_ctx_st;

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES encryption.
///
/// Supported modes are ECB, CBC, CCM and GCM:
/// - Avoid ECB if you do not know about its limitations
/// - CCM mode allows only one single input round, and is slower than GCM - only choose it if the
///   protocol requires you to
/// - If you do not need message integrity checks you may want to use CBC, this saves you some
///   bytes per cyphertext (when IV and tag are included, as there is no tag of size 12-16 bytes)
///   - otherwise use GCM
/// - Prefer GCM if you want to make sure the message has not been tampered with, or if you simply
///   want to make a safe choice
///
/// Per default, KAES computes needed initialization vectors itself, and inserts these, as well as possible
/// authentication tags in CCM and GCM modes, at the start of the ciphertext and extracts these there
/// on decryption. You can suppress this, but then you have to transport these over another channel
/// to the recipient of the ciphertext.
///
/// Currently KAES does not support AAD data (for CCM and GCM modes).
///
/// After instantiation you have to set either a password and optional salt, or a key of the right size, see
/// SetPassword() and SetKey().
/// Please note that when you use a salted password you have to send both the password and the salt
/// to the decryptor via another channel, as the salt is not included in the ciphertext.
///
/// You can compute keys with password based key derivation functions, see CreateKeyFromPasswordHKDF()
/// and CreateKeyFromPasswordPKCS5(). The former is are also internally used when you set a password
/// instead of a key.
///
/// If you chose to _not_ inline the initialization vector and possible authentication tag into the ciphertext
/// you have to send both (or only the IV for CBC mode, or neither for ECB) to the decryptor via another
/// channel, and use SetInitializationVector() and SetAuthenticationTag() to feed them into KAES for
/// decryption directly after construction.
///
/// A typical encryption - decryption setup may look like:
/// @code
/// KString sPlainText = "this is a secret message for your eyes only";
/// KStringView sPassword = "MySecretPassword";
///
/// KString sEncrypted;               ///< will take the ciphertext
/// KToAES Encryptor(sEncrypted);     ///< uses default mode GCM with 256 bits
/// Encryptor.SetThrowOnError(true);  ///< make sure we throw in case of errors
/// Encryptor.SetPassword(sPassword); ///< set key derived from password
/// Encryptor.Add(sPlainText);        ///< add the plaintext
/// Encryptor.Finalize();             ///< finalize the encryption (also called by destructor)
///
/// KString sDecrypted;
/// KFromAES Decryptor(sDecrypted);
/// Decryptor.SetThrowOnError(true);
/// Decryptor.SetPassword(sPassword);
/// Decryptor.Add(sEncrypted);
/// Decryptor.Finalize();
/// @endcode
/// please note that we used the derived classes KToAES and KFromAES instead of KAES for ease of typing.
///
class DEKAF2_PUBLIC KAES : public KDigest, public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Algorithm { AES };
	enum Direction { Decrypt = 0, Encrypt = 1 };
	enum Bits      { B128, B192, B256 };
	enum Mode      { ECB, CBC, CCM, GCM };

	/// AES encryption/decryption with a string to output into - always call Finalize() or destructor to terminate the string
	/// @param sOutput the string to output into
	/// @param direction either Decrypt or Encrypt
	/// @param algorithm always AES for now
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	/// @param bInlineIVandTag whether the IV and possible tag should for encryption be inserted at the start
	///  of the ciphertext, and read there for decryption - defaults to true
	KAES(
		KStringRef&  sOutput,
		Direction    direction,
		Algorithm    algorithm       = AES,
		Mode         mode            = GCM,
		Bits         bits            = B256,
		bool         bInlineIVandTag = true
	);

	/// AES encryption/decryption with a stream to output into - always call Finalize() or destructor to flush the stream
	/// @param sOutStream the stream to output into - for GCM it has to be seekable, otherwise an error is set
	/// @param direction either Decrypt or Encrypt
	/// @param algorithm always AES for now
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	/// @param bInlineIVandTag whether the IV and possible tag should for encryption be inserted at the start
	///  of the ciphertext, and read there for decryption - defaults to true
	KAES(
		KOutStream&  OutStream,
		Direction    direction,
		Algorithm    algorithm       = AES,
		Mode         mode            = GCM,
		Bits         bits            = B256,
		bool         bInlineIVandTag = true
	);

	/// copy construction
	KAES(const KAES&) = delete;
	/// move construction
	KAES(KAES&&) noexcept;
	// destruction
	~KAES();

	/// copy assignment
	KAES& operator=(const KAES&) = delete;

	/// set a password and optionally a salt - will be used to compute the key for the cipher
	/// - call either this or SetKey() after construction
	bool SetPassword(KStringView sPassword, KStringView sSalt = "");
	/// set the encryption key - call either this or SetPassword() after construction
	bool SetKey(KStringView sKey);

	/// returns needed key length in bytes, if you want to create a valid key yourself and set it with SetKey()
	uint16_t GetNeededKeyLength() const { return m_iKeyLength; }
	/// returns needed IV length in bytes, if you want to create a valid IV yourself and set it with SetInitializationVector()
	uint16_t GetNeededIVLength() const { return m_iIVLength; }
	/// returns block size of the chosen cipher - the input/output functions are independent from the
	/// block size, this is for information only
	uint16_t GetBlockSize() const { return m_iBlockSize; }
	/// returns the name of the chosen cipher - for information only
	KStringView GetCipherName() const { return m_sCipherName; }
	/// returns wether this instance is for encryption or decryption
	Direction GetDirection() const { return m_Direction; }
	/// returns the cipher mode
	Mode GetMode() const { return m_Mode; }

	/// appends more data from a string
	bool Add(KStringView sInput);
	/// appends more data from a stream
	bool Add(KInStream& InputStream);
	/// appends more data from a stream
	bool Add(KInStream&& InputStream);

	/// appends more data from a string
	KAES& operator+=(KStringView sInput)
	{
		Add(sInput);
		return *this;
	}
	/// appends more data from a string
	void operator()(KStringView sInput)
	{
		Add(sInput);
	}
	/// appends more data from a stream
	void operator()(KInStream& InputStream)
	{
		Add(InputStream);
	}

	/// finalize by filling up output to full block size
	bool Finalize();

	/// for encryption: get the initialization vector as string - you will only need this if you do not want to pass
	/// IV and authentication tag at the start of the ciphertext - call after adding input data
	const KString& GetInitializationVector() const { return m_sIV; }

	/// for encryption: get the authentication tag as string - you will only need this if you do not want to pass
	/// IV and authentication tag at the start of the ciphertext - call after finalizing the encryption
	const KString& GetAuthenticationTag() const { return m_sTag; }

	/// for decryption: set the initialization vector from a string - you will only need this if you do not want to pass
	/// IV and authentication tag at the start of the ciphertext - call before adding any input data
	/// for encryption: if you want to set your own initialization vector instead of a random one, call this
	/// before adding any input data.
	bool SetInitializationVector(KStringView sTag);

	/// for decryption: set the authentication tag from a string - you will only need this if you do not want to pass
	/// IV and authentication tag at the start of the ciphertext and if the selected cipher supports authentication,
	/// like GCM - call before adding any input data
	bool SetAuthenticationTag(KStringView sTag);

	/// static: return cipher for algorithm, mode and bits
	static const evp_cipher_st* GetCipher(Algorithm algorithm, Mode mode, Bits bits);

	/// static: generate a key of iKeyLen bytes size derived from sPassword and sSalt, using an sha256 hmac
	/// (HKDF, see RFC 5869) - when called with OpenSSL < v1.1.0 this function delegates to
	/// CreateKeyFromPasswordPKCS5() as the HKDF algorithm is not available before v1.1.0
	static KString CreateKeyFromPasswordHKDF(
		uint16_t    iKeyLen,
		KStringView sPassword,
		KStringView sSalt       = ""
	 );

	/// static: generate a key of iKeyLen bytes size derived from sPassword and sSalt, using algorithm digest
	/// with iIterations rounds (PKCS5_PBKDF2_HMAC, see RFC 2898) - better use the HKDF version
	static KString CreateKeyFromPasswordPKCS5(
		uint16_t    iKeyLen,
		KStringView sPassword,
		KStringView sSalt       = "",
		Digest      digest      = SHA256,
		uint16_t    iIterations = 1024
	 );

//------
private:
//------

	bool Initialize(
		Algorithm    algorithm,
		Bits         bits
	);

	bool CompleteInitialization();
	void SetOutput(KStringRef& sOutput);
	void SetOutput(KOutStream& OutStream);
	bool SetTag();
	bool AddString(KStringView sInput);
	bool AddStream(KStringView sInput);
	bool FinalizeString();
	bool FinalizeStream();
	void Release() noexcept;

	static KString GetOpenSSLError();

	const evp_cipher_st* m_Cipher        { nullptr };
	evp_cipher_ctx_st*   m_evpctx        { nullptr };
	uint16_t             m_iKeyLength    { 0 };
	uint16_t             m_iIVLength     { 0 };
	uint16_t             m_iTagLength    { 0 };
	uint16_t             m_iBlockSize    { 0 };
	uint16_t             m_iGetIVLength  { 0 };
	uint16_t             m_iGetTagLength { 0 };
	KStringView          m_sCipherName;
	KString              m_sIV;
	KString              m_sTag;
	KOutStream*          m_OutStream     { nullptr };
	KStringRef*          m_OutString     { nullptr };
	std::streampos       m_StartOfStream {       0 };
	Direction            m_Direction;
	Mode                 m_Mode;
	bool                 m_bInlineIVandTag { false };
	bool                 m_bKeyIsSet       { false };
	bool                 m_bInitCompleted  { false };

}; // KAES

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES encryption. See class KAES for full documentation
///
/// A typical encryption setup may look like:
/// @code
/// KString sPlainText = "this is a secret message for your eyes only";
/// KStringView sPassword = "MySecretPassword";
///
/// KString sEncrypted;               ///< will take the ciphertext
/// KToAES Encryptor(sEncrypted);     ///< uses default mode GCM with 256 bits
/// Encryptor.SetThrowOnError(true);  ///< make sure we throw in case of errors
/// Encryptor.SetPassword(sPassword); ///< set key derived from password
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

	/// AES encryption with a string to output into - always call Finalize() or destructor to terminate the string
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KToAES(KStringRef& sOutput, KAES::Mode mode = KAES::GCM, KAES::Bits bits = KAES::B256)
	: KAES(sOutput, KAES::Encrypt, KAES::AES, mode, bits)
	{
	}

	/// AES encryption with a stream to output into - always call Finalize() or destructor to terminate the string
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KToAES(KOutStream& OutStream, KAES::Mode mode = KAES::GCM, KAES::Bits bits = KAES::B256)
	: KAES(OutStream, KAES::Encrypt, KAES::AES, mode, bits)
	{
	}

}; // KToAES

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Symmetrical AES decryption. See class KAES for full documentation
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

	/// AES decryption with a string to output into - always call Finalize() or destructor to terminate the string
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KFromAES(KStringRef& sOutput, KAES::Mode mode = KAES::GCM, KAES::Bits bits = KAES::B256)
	: KAES(sOutput, KAES::Decrypt, KAES::AES, mode, bits)
	{
	}

	/// AES decryption with a stream to output into - always call Finalize() or destructor to terminate the string
	/// @param sOutput the string to output into
	/// @param mode either ECB, CBC, CCM or GCM - defaults to GCM
	/// @param bits one of B128, B192, B256 - defaults to B256
	KFromAES(KOutStream& OutStream, KAES::Mode mode = KAES::GCM, KAES::Bits bits = KAES::B256)
	: KAES(OutStream, KAES::Decrypt, KAES::AES, mode, bits)
	{
	}

}; // KFromAES

DEKAF2_NAMESPACE_END

#endif // of OPENSSL_VERSION_NUMBER >= 0x010002000L
