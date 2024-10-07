/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

/// @file kbcrypt.h
/// safe password hashing with dynamic workload computation

#include "kstring.h"
#include "kstringview.h"
#include "kduration.h"
#include "kerror.h"
#include <array>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// safe password hashing, allowing dynamic workload computation to adapt to used hardware
class DEKAF2_PUBLIC KBCrypt : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor that uses a fixed workload of 8, which may have
	/// very different execution times on different hardware
	KBCrypt () = default;

	/// constructor that computes the workload factor to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	KBCrypt (KDuration Duration) { ComputeWorkload(Duration); }

	/// generate a bcrypt password hash to store into a database
	/// @param sPassword the password to be hashed
	/// @return the bcrypt password hash, including salt and algorithm
	DEKAF2_NODISCARD
	KString  GenerateHash     (KStringViewZ sPassword);

	/// verify a password against a known bcrypt password hash
	/// @param sPassword the password to be verified
	/// @param sHash the known bcrypt password hash
	/// @return true if password matches hash, false otherwise
	DEKAF2_NODISCARD
	bool     ValidatePassword (KStringViewZ sPassword, KStringViewZ sHash);

	/// returns the current workload factor, either preset or computed,
	DEKAF2_NODISCARD
	uint16_t GetWorkload      () const { return m_iWorkload; }

	/// sets the workload factor to a fixed value between 4 and 31 -
	/// better use timed computation
	bool     SetWorkload      (uint16_t iWorkload, bool bAdjustIfOutOfBounds = true);

	/// computes the workload factor to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	void     ComputeWorkload  (KDuration Duration);

//----------
private:
//----------

	static constexpr uint16_t iBCryptHashSize = 64;
	static constexpr uint16_t iRandomBytes    = 16;

	using Token = std::array<char, iBCryptHashSize>;

	uint16_t m_iWorkload { 8 };

	bool GenerateSalt  (Token& Salt);
	bool HashPassword  (KStringViewZ sPassword, const char* sSalt, Token& Hash);
	bool SafeCompare   (KStringView s1, const Token& s2);
	bool CheckPassword (KStringViewZ sPassword, KStringViewZ sHash);

}; // KBCrypt

DEKAF2_NAMESPACE_END
