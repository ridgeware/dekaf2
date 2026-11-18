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

/// @file kargon2.h
/// password hashing with Argon2id

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/errors/kerror.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Password hashing with Argon2id (RFC 9106). The hashes are strings in the PHC
/// format, like `$argon2id$v=19$m=19456,t=2,p=1$<salt>$<tag>`: they carry their
/// cost parameters, so that hashes of another cost, and of other implementations,
/// validate. Other than bcrypt, Argon2 uses the whole password, and it is memory hard.
///
/// The computation needs no crypto library.
class DEKAF2_PUBLIC KArgon2 : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the variants of Argon2 - new hashes are always Argon2id
	enum class Variant : uint8_t
	{
		Argon2d  = 0, ///< picks its memory blocks dependent on the password
		Argon2i  = 1, ///< picks its memory blocks independent of the password
		Argon2id = 2  ///< starts as Argon2i and continues as Argon2d
	};

	/// the memory of the default cost, in KiB
	static constexpr uint32_t iDefaultMemoryKiB = 19 * 1024;

	/// the cost of a hash
	struct Cost
	{
		uint32_t iMemoryKiB   { iDefaultMemoryKiB }; ///< the memory to fill, in KiB
		uint32_t iIterations  { 2 };         ///< the count of passes over the memory
		uint32_t iParallelism { 1 };         ///< the count of lanes the memory is split into
	};

	/// default constructor: 19 MiB of memory, two passes, one lane
	KArgon2 () = default;

	/// constructor with a fixed cost - note that this leads to very different
	/// execution times on different hardware
	/// @param cost the cost of new hashes
	KArgon2 (Cost cost) { SetCost(cost); }

	/// constructor that computes the count of passes to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	/// @param Duration the maximum duration for the hash computation
	/// @param iMemoryKiB the memory to fill, in KiB
	/// @param bComputeAtFirstUse if true, the count of passes will be computed at
	/// first use of the hash function, otherwise immediately.
	KArgon2 (KDuration Duration, uint32_t iMemoryKiB = iDefaultMemoryKiB, bool bComputeAtFirstUse = true);

	/// generate an Argon2id password hash to e.g. store into a database
	/// @param sPassword the password to be hashed
	/// @return the password hash in the PHC format, including salt and cost,
	/// or an empty string on error
	DEKAF2_NODISCARD
	KString  GenerateHash     (KStringView sPassword);

	/// verify a password against a known Argon2 password hash of any variant and cost
	/// @param sPassword the password to be verified
	/// @param sHash the known password hash in the PHC format
	/// @return true if password matches hash, false otherwise
	DEKAF2_NODISCARD
	bool     ValidatePassword (KStringView sPassword, KStringView sHash);

	/// checks if a hash should be replaced at the next successful login
	/// @param sHash the known password hash
	/// @return true if the hash is no Argon2id hash, or if it was generated with
	/// less memory, passes or lanes than the current cost
	DEKAF2_NODISCARD
	bool     NeedsRehash      (KStringView sHash);

	/// returns the current cost, either preset or computed
	DEKAF2_NODISCARD
	Cost     GetCost          ();

	/// sets the cost to fixed values
	/// @param cost the cost of new hashes
	/// @return false if the values are outside of the limits of Argon2
	/// (at least 8 KiB of memory per lane, at least one pass and one lane)
	bool     SetCost          (Cost cost);

	/// computes the count of passes to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	/// @param Duration the maximum duration for the hash computation
	/// @param bComputeAtNextUse if true, the count of passes will be computed at
	/// next use of the hash function, otherwise immediately.
	void     ComputeCost      (KDuration Duration, bool bComputeAtNextUse);

	/// returns the duration set for the last ComputeCost()
	DEKAF2_NODISCARD
	KDuration GetCostDuration () const { return m_Duration; }

	/// the Argon2 function itself, e.g. to derive a key from a password
	/// @param variant the variant of Argon2
	/// @param sPassword the password
	/// @param sSalt the salt, at least 8 bytes
	/// @param cost the cost
	/// @param iTagLength the length of the result in bytes, at least 4
	/// @param sSecret an optional secret that is no part of a hash string
	/// @param sAssociatedData optional data that is bound to the result
	/// @return the tag, or an empty string for parameters outside of the limits of Argon2
	DEKAF2_NODISCARD
	static KString Derive     (Variant     variant,
	                           KStringView sPassword,
	                           KStringView sSalt,
	                           const Cost& cost,
	                           std::size_t iTagLength      = iDefaultTagLength,
	                           KStringView sSecret         = KStringView{},
	                           KStringView sAssociatedData = KStringView{});

//----------
private:
//----------

	static constexpr uint16_t iSaltLength       = 16;
	static constexpr uint16_t iDefaultTagLength = 32;

	// the parts of a hash string
	struct Parts
	{
		Variant variant { Variant::Argon2id };
		Cost    cost;
		KString sSalt;
		KString sTag;
	};

	bool Parse (KStringView sHash, Parts& parts) const;

	KDuration m_Duration;
	Cost      m_Cost;
	bool      m_bComputeAtNextUse { false };

}; // KArgon2

/// @}

DEKAF2_NAMESPACE_END
