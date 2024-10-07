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
	/// @param Duration the maximum duration for the hash computation
	/// @param bComputeAtFirstUse if true, the workload will be computed at
	/// first use of the hash function, otherwise immediately.
	KBCrypt (KDuration Duration, bool bComputeAtFirstUse = true) { ComputeWorkload(Duration, bComputeAtFirstUse); }

	/// constructor that sets the workload factor to a fixed value between 4 and 31 -
	/// note that this leads to very different execution times on different hardware
	/// @param iWorkload the workload factor to use
	/// @param bAdjustIfOutOfBounds if true will correct values outside of
	/// the valid range, if false will set an error - defaults to true.
	KBCrypt (uint16_t iWorkload, bool bAdjustIfOutOfBounds = true) { SetWorkload(iWorkload, bAdjustIfOutOfBounds); }

	/// generate a bcrypt password hash to e.g. store into a database
	/// @param sPassword the password to be hashed
	/// @return the bcrypt password hash, including salt and algorithm flag
	DEKAF2_NODISCARD
	KString  GenerateHash     (KStringViewZ sPassword);

	/// verify a password against a known bcrypt password hash
	/// @param sPassword the password to be verified
	/// @param sHash the known bcrypt password hash
	/// @return true if password matches hash, false otherwise
	DEKAF2_NODISCARD
	bool     ValidatePassword (KStringViewZ sPassword, KStringViewZ sHash);

	/// returns the current workload factor, either preset or computed
	DEKAF2_NODISCARD
	uint16_t GetWorkload      ();

	/// sets the workload factor to a fixed value between 4 and 31 -
	/// better use timed computation
	/// @param iWorkload the workload factor to use
	/// @param bAdjustIfOutOfBounds if true will correct values outside of
	/// the valid range, if false will set an error - defaults to true.
	/// @return true if the workload factor was accepted (also if adjusted)
	bool     SetWorkload      (uint16_t iWorkload, bool bAdjustIfOutOfBounds = true);

	/// computes the workload factor to take not longer than
	/// the given duration, e.g. some 100ms for a password check
	/// @param Duration the maximum duration for the hash computation
	/// @param bComputeAtNextUse if true, the workload will be computed at
	/// next use of the hash function, otherwise immediately.
	void     ComputeWorkload  (KDuration Duration, bool bComputeAtNextUse);

	/// returns the duration set for the last ComputeWorkload()
	DEKAF2_NODISCARD
	KDuration GetWorkloadDuration () const { return m_Duration; }

//----------
private:
//----------

	static constexpr uint16_t iBCryptHashSize = 64;
	static constexpr uint16_t iRandomBytes    = 16;

	using Token = std::array<char, iBCryptHashSize>;

	KDuration m_Duration;
	uint16_t  m_iWorkload { 8 };
	bool      m_bComputeAtNextUse { false };

	bool GenerateSalt  (Token& Salt);
	bool HashPassword  (KStringViewZ sPassword, const char* sSalt, Token& Hash);
	bool SafeCompare   (KStringView s1, const Token& s2);
	bool CheckPassword (KStringViewZ sPassword, KStringViewZ sHash);

}; // KBCrypt

DEKAF2_NAMESPACE_END
