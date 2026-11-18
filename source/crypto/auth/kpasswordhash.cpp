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


#include <dekaf2/crypto/auth/kpasswordhash.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/types/kctype.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KPasswordHash::KPasswordHash(KDuration Duration, Algorithm algorithm, bool bComputeAtFirstUse)
//-----------------------------------------------------------------------------
{
	if (SetAlgorithm(algorithm))
	{
		switch (m_Algorithm)
		{
			case Algorithm::Argon2:
				m_Argon2.ComputeCost(Duration, bComputeAtFirstUse);
				break;

			case Algorithm::BCrypt:
				m_BCrypt.ComputeWorkload(Duration, bComputeAtFirstUse);
				break;

			case Algorithm::Unknown:
				break;
		}
	}

} // ctor

//-----------------------------------------------------------------------------
bool KPasswordHash::SetAlgorithm(Algorithm algorithm)
//-----------------------------------------------------------------------------
{
	switch (algorithm)
	{
		case Algorithm::Argon2:
		case Algorithm::BCrypt:
			m_Algorithm = algorithm;
			return true;

		case Algorithm::Unknown:
			break;
	}

	return SetError("no algorithm for new hashes");

} // SetAlgorithm

//-----------------------------------------------------------------------------
KPasswordHash::Algorithm KPasswordHash::GetAlgorithm(KStringView sHash)
//-----------------------------------------------------------------------------
{
	if (sHash.starts_with("$argon2"))
	{
		return Algorithm::Argon2;
	}

	// $2b$10$ and 53 characters of salt and hash
	if (sHash.size() == 60
	 && sHash.starts_with("$2")
	 && (sHash[2] == 'a' || sHash[2] == 'b' || sHash[2] == 'y')
	 && sHash[3] == '$'
	 && KASCII::kIsDigit(static_cast<unsigned char>(sHash[4]))
	 && KASCII::kIsDigit(static_cast<unsigned char>(sHash[5]))
	 && sHash[6] == '$')
	{
		return Algorithm::BCrypt;
	}

	return Algorithm::Unknown;

} // GetAlgorithm

//-----------------------------------------------------------------------------
KString KPasswordHash::GenerateHash(KStringView sPassword)
//-----------------------------------------------------------------------------
{
	KString sHash;

	switch (m_Algorithm)
	{
		case Algorithm::Argon2:
			sHash = m_Argon2.GenerateHash(sPassword);

			if (sHash.empty())
			{
				SetError(m_Argon2.CopyLastError());
			}
			break;

		case Algorithm::BCrypt:
		{
			// bcrypt wants a zero terminated password
			KString sCopy(sPassword);
			sHash = m_BCrypt.GenerateHash(sCopy);
			kSafeZeroize(sCopy);

			if (sHash.empty())
			{
				SetError(m_BCrypt.CopyLastError());
			}
			break;
		}

		case Algorithm::Unknown:
			SetError("no algorithm for new hashes");
			break;
	}

	return sHash;

} // GenerateHash

//-----------------------------------------------------------------------------
bool KPasswordHash::ValidatePassword(KStringView sPassword, KStringView sHash)
//-----------------------------------------------------------------------------
{
	switch (GetAlgorithm(sHash))
	{
		case Algorithm::Argon2:
			m_Argon2.ClearError();

			if (m_Argon2.ValidatePassword(sPassword, sHash))
			{
				return true;
			}

			if (m_Argon2.HasError())
			{
				return SetError(m_Argon2.CopyLastError());
			}

			return false;

		case Algorithm::BCrypt:
		{
			// bcrypt wants zero terminated strings
			KString sCopy(sPassword);
			m_BCrypt.ClearError();
			auto bValid = m_BCrypt.ValidatePassword(sCopy, KString(sHash));
			kSafeZeroize(sCopy);

			if (!bValid && m_BCrypt.HasError())
			{
				return SetError(m_BCrypt.CopyLastError());
			}

			return bValid;
		}

		case Algorithm::Unknown:
			break;
	}

	return SetError("no hash of bcrypt or Argon2");

} // ValidatePassword

//-----------------------------------------------------------------------------
bool KPasswordHash::ValidatePassword(KStringView sPassword, KStringView sHash, KString& sNewHash)
//-----------------------------------------------------------------------------
{
	sNewHash.clear();

	if (!ValidatePassword(sPassword, sHash))
	{
		return false;
	}

	if (NeedsRehash(sHash))
	{
		// an empty result leaves the known hash in place
		sNewHash = GenerateHash(sPassword);
	}

	return true;

} // ValidatePassword

//-----------------------------------------------------------------------------
bool KPasswordHash::NeedsRehash(KStringView sHash)
//-----------------------------------------------------------------------------
{
	switch (GetAlgorithm(sHash))
	{
		case Algorithm::Argon2:
			// with bcrypt for new hashes a hash of Argon2 stays
			return m_Algorithm == Algorithm::Argon2 && m_Argon2.NeedsRehash(sHash);

		case Algorithm::BCrypt:
			if (m_Algorithm == Algorithm::BCrypt)
			{
				// the workload is the two digit number behind the version
				return sHash.substr(4, 2).UInt16() < m_BCrypt.GetWorkload();
			}

			return true;

		case Algorithm::Unknown:
			break;
	}

	return true;

} // NeedsRehash

//-----------------------------------------------------------------------------
void KPasswordHash::SpendValidationTime(KStringView sPassword)
//-----------------------------------------------------------------------------
{
	// a new hash costs what the check of a hash of the same cost does
	auto sHash = GenerateHash(sPassword);
	kSafeZeroize(sHash);

} // SpendValidationTime

DEKAF2_NAMESPACE_END
