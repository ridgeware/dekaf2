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

#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/util/id/krandom.h>
#include <dekaf2/core/strings/kstringutils.h>

extern "C" {
extern char *crypt_rn(const char *key, const char *setting,
                      void *data, int size);

extern char *crypt_gensalt_rn(const char *prefix, unsigned long count,
                              const char *input, int size,
                              char *output, int output_size);
} // extern C

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KBCrypt::GenerateSalt(Token& Salt)
//-----------------------------------------------------------------------------
{
	KString sInput = kGetRandom(iRandomBytes);

	if (sInput.size() != iRandomBytes)
	{
		return SetError(kFormat("cannot get randomness for salt generation, got {} instead of {} bytes", sInput.size(), iRandomBytes));
	}

	auto bResult = crypt_gensalt_rn
	(
		"$2b$",
		m_iWorkload,
		sInput.data(), static_cast<int>(sInput.size()),
		Salt  .data(), static_cast<int>(Salt  .size())
	) != nullptr;

	kSafeZeroize(sInput);

	if (!bResult)
	{
		return SetError("cannot generate salt");
	}

	return true;

} // GenerateSalt

//-----------------------------------------------------------------------------
bool KBCrypt::HashPassword(KStringViewZ sPassword, const char* sSalt, Token& Hash)
//-----------------------------------------------------------------------------
{
	if (crypt_rn(sPassword.c_str(), sSalt, Hash.data(), iBCryptHashSize) == nullptr)
	{
		return SetError("cannot hash password");
	}

	return true;

} // HashPassword

//-----------------------------------------------------------------------------
// safe in the sense to avoid timing attacks
bool KBCrypt::SafeCompare(KStringView s1, const Token& s2)
//-----------------------------------------------------------------------------
{
	auto is2len = strnlen(s2.data(), s2.size());
	auto iLen   = std::min(s1.size(), is2len);

	int iResult = static_cast<int>(s1.size() ^ is2len);

	for (std::size_t iCount = 0; iCount < iLen; ++iCount)
	{
		iResult |= (static_cast<unsigned char>(s1[iCount]) ^ static_cast<unsigned char>(s2[iCount]));
	}

	return iResult == 0;

} // SafeCompare

//-----------------------------------------------------------------------------
bool KBCrypt::CheckPassword(KStringViewZ sPassword, KStringViewZ sHash)
//-----------------------------------------------------------------------------
{
	if (sHash.size() >= iBCryptHashSize)
	{
		return SetError(kFormat("hash size {} >= iBCryptHashSize {}",  sHash.size(), iBCryptHashSize));
	}

	Token CheckHash;

	if (!HashPassword(sPassword, sHash.data(), CheckHash))
	{
		kSafeZeroize(CheckHash);
		return false;
	}

	auto bResult = SafeCompare(sHash, CheckHash);

	kSafeZeroize(CheckHash);

	return bResult;

} // CheckPassword

//-----------------------------------------------------------------------------
KString KBCrypt::GenerateHash(KStringViewZ sPassword)
//-----------------------------------------------------------------------------
{
	if (m_bComputeAtNextUse)
	{
		ComputeWorkload(m_Duration, false);
	}

	auto Timer = kWouldLog(2) ? KStopTime() : KStopTime(KStopTime::Halted);

	Token Salt;

	if (!GenerateSalt(Salt))
	{
		kSafeZeroize(Salt);
		return {};
	}

	Token Hash;

	if (!HashPassword(sPassword, Salt.data(), Hash))
	{
		kSafeZeroize(Salt);
		kSafeZeroize(Hash);
		return {};
	}

	kDebug(2, "password hash generation took {}", Timer.elapsed());

	KString sResult { Hash.data() };

	kSafeZeroize(Salt);
	kSafeZeroize(Hash);

	return sResult;

} // GenerateHash

//-----------------------------------------------------------------------------
bool KBCrypt::ValidatePassword(KStringViewZ sPassword, KStringViewZ sHash)
//-----------------------------------------------------------------------------
{
	auto Timer = kWouldLog(2) ? KStopTime() : KStopTime(KStopTime::Halted);

	auto bValid = CheckPassword(sPassword, sHash);

	kDebug(2, "password verification took {} and {}", Timer.elapsed(), bValid ? "succeeded" : "failed");

	return bValid;

} // ValidatePassword

//-----------------------------------------------------------------------------
bool KBCrypt::SetWorkload(uint16_t iWorkload, bool bAdjustIfOutOfBounds)
//-----------------------------------------------------------------------------
{
	if (iWorkload < 4)
	{
		if (bAdjustIfOutOfBounds)
		{
			kDebug(2, "workload of {} is inappropriate for bcrypt, setting to {}", iWorkload, 4);
			iWorkload = 4;
		}
		else
		{
			return SetError("invalid workload: {}", iWorkload);
		}
	}
	else if (iWorkload > 31)
	{
		if (bAdjustIfOutOfBounds)
		{
			kDebug(2, "workload of {} is inappropriate for bcrypt, setting to {}", iWorkload, 31);
			iWorkload = 31;
		}
		else
		{
			return SetError("invalid workload: {}", iWorkload);
		}
	}

	m_iWorkload = iWorkload;
	m_bComputeAtNextUse = false;

	kDebug(2, "workload set to {}", m_iWorkload);

	return true;

} // SetWorkload

//-----------------------------------------------------------------------------
void KBCrypt::ComputeWorkload(KDuration Duration, bool bComputeAtNextUse)
//-----------------------------------------------------------------------------
{
	// compute a workload value that results in less or equal the requested
	// duration

	m_Duration = Duration;

	if (bComputeAtNextUse)
	{
		m_bComputeAtNextUse = true;
		return;
	}

	uint16_t i;

	for (i = 4; i <= 31; ++i)
	{
		m_iWorkload = i;

		KStopTime Timer;

		Token Salt;
		GenerateSalt(Salt);
		Token Hash;
		HashPassword("any password would do", Salt.data(), Hash);

		kSafeZeroize(Salt);
		kSafeZeroize(Hash);

		if (Timer.elapsed() > Duration)
		{
			--i;
			break;
		}
	}

	SetWorkload(i, true);

} // ComputeWorkload

//-----------------------------------------------------------------------------
uint16_t KBCrypt::GetWorkload()
//-----------------------------------------------------------------------------
{
	if (m_bComputeAtNextUse)
	{
		ComputeWorkload(m_Duration, false);
	}

	return m_iWorkload;

} // GetWorkload

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
// this looks stupid, but gcc 6 wouldn't compile in debug mode without:
constexpr uint16_t KBCrypt::iBCryptHashSize;
constexpr uint16_t KBCrypt::iRandomBytes;
#endif

DEKAF2_NAMESPACE_END
