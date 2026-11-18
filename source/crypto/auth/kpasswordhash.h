/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
*/


#pragma once

/// @file kpasswordhash.h
/// password hashing that validates hashes of bcrypt and of Argon2

#include <dekaf2/crypto/auth/kargon2.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/errors/kerror.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Password hashing for applications that move from bcrypt to Argon2id: validates
/// the hashes of both, tells which stored hashes should be replaced, and generates
/// new hashes with one of the two (Argon2id unless told otherwise). An application
/// that used KBCrypt keeps its stored hashes and replaces them login by login:
/// @code
/// KPasswordHash Hasher;
/// KString       sNewHash;
///
/// if (Hasher.ValidatePassword(sPassword, sStoredHash, sNewHash))
/// {
///     if (!sNewHash.empty())
///     {
///         // the stored hash was one of bcrypt, or of a lower cost
///         StoreHash(sUser, sNewHash);
///     }
///     // logged in
/// }
/// @endcode
class DEKAF2_PUBLIC KPasswordHash : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the algorithms of password hashes
	enum class Algorithm : uint8_t
	{
		Unknown, ///< no hash of a known algorithm
		BCrypt,  ///< bcrypt, hashes start with $2a$, $2b$ or $2y$
		Argon2   ///< Argon2, hashes start with $argon2 - new hashes are Argon2id
	};

	/// default constructor: new hashes are Argon2id with the default cost of KArgon2
	KPasswordHash () = default;

	/// constructor that chooses the algorithm for new hashes, with its default cost
	/// @param algorithm the algorithm for new hashes, Algorithm::Argon2 or Algorithm::BCrypt
	explicit KPasswordHash (Algorithm algorithm) { SetAlgorithm(algorithm); }

	/// constructor that computes the cost of new hashes to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	/// @param Duration the maximum duration for the hash computation
	/// @param algorithm the algorithm for new hashes, Algorithm::Argon2 or Algorithm::BCrypt
	/// @param bComputeAtFirstUse if true, the cost will be computed at
	/// first use of the hash function, otherwise immediately.
	KPasswordHash (KDuration Duration, Algorithm algorithm = Algorithm::Argon2, bool bComputeAtFirstUse = true);

	/// generate a password hash with the algorithm for new hashes
	/// @param sPassword the password to be hashed (bcrypt uses only its first 72 bytes,
	/// and stops at a zero byte)
	/// @return the password hash, or an empty string on error
	DEKAF2_NODISCARD
	KString   GenerateHash        (KStringView sPassword);

	/// verify a password against a known hash of bcrypt or Argon2
	/// @param sPassword the password to be verified
	/// @param sHash the known password hash
	/// @return true if password matches hash, false otherwise
	DEKAF2_NODISCARD
	bool      ValidatePassword    (KStringView sPassword, KStringView sHash);

	/// verify a password against a known hash of bcrypt or Argon2, and generate a new
	/// hash for it when the known one should be replaced
	/// @param sPassword the password to be verified
	/// @param sHash the known password hash
	/// @param sNewHash receives the hash to store instead of sHash, or is cleared when
	/// sHash can stay or the password does not match
	/// @return true if password matches hash, false otherwise
	DEKAF2_NODISCARD
	bool      ValidatePassword    (KStringView sPassword, KStringView sHash, KString& sNewHash);

	/// checks if a hash should be replaced at the next successful login
	/// @param sHash the known password hash
	/// @return true if the hash is not of the algorithm for new hashes, or of a lower
	/// cost than new hashes have. A hash of Argon2 is never replaced by one of bcrypt.
	DEKAF2_NODISCARD
	bool      NeedsRehash         (KStringView sHash);

	/// spends the time of a password check without a hash to check against - for a
	/// login with an unknown user name, so that the response time does not tell
	/// which names exist
	/// @param sPassword the password that came with the login
	void      SpendValidationTime (KStringView sPassword);

	/// @return the algorithm of a hash, told by its start
	DEKAF2_NODISCARD
	static Algorithm GetAlgorithm (KStringView sHash);

	/// @return the algorithm for new hashes
	DEKAF2_NODISCARD
	Algorithm GetAlgorithm        () const { return m_Algorithm; }

	/// sets the algorithm for new hashes
	/// @param algorithm Algorithm::Argon2 or Algorithm::BCrypt
	/// @return false for Algorithm::Unknown
	bool      SetAlgorithm        (Algorithm algorithm);

	/// @return the hasher for Argon2, to read or set its cost
	KArgon2&  Argon2              () { return m_Argon2; }

	/// @return the hasher for bcrypt, to read or set its workload
	KBCrypt&  BCrypt              () { return m_BCrypt; }

//----------
private:
//----------

	KArgon2   m_Argon2;
	KBCrypt   m_BCrypt;
	Algorithm m_Algorithm { Algorithm::Argon2 };

}; // KPasswordHash

/// @}

DEKAF2_NAMESPACE_END
