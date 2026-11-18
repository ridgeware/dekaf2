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


#include <dekaf2/crypto/auth/kargon2.h>
#include <dekaf2/crypto/auth/bits/kargon2core.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/hash/bits/kdigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/types/kctype.h>
#include <limits>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
/// base64 as the PHC format wants it: the standard alphabet without padding
KString EncodeField(KStringView sBytes)
//-----------------------------------------------------------------------------
{
	auto sEncoded = KBase64::Encode(sBytes, false);
	sEncoded.TrimRight('=');

	return sEncoded;

} // EncodeField

//-----------------------------------------------------------------------------
/// @return true if sField is one of the base64 fields of the PHC format
bool IsField(KStringView sField)
//-----------------------------------------------------------------------------
{
	for (auto ch : sField)
	{
		if (!KASCII::kIsAlNum(static_cast<unsigned char>(ch)) && ch != '+' && ch != '/')
		{
			return false;
		}
	}

	return !sField.empty();

} // IsField

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString KArgon2::Derive(Variant     variant,
                        KStringView sPassword,
                        KStringView sSalt,
                        const Cost& cost,
                        std::size_t iTagLength,
                        KStringView sSecret,
                        KStringView sAssociatedData)
//-----------------------------------------------------------------------------
{
	return detail::KArgon2Core::Derive(static_cast<uint32_t>(variant), sPassword, sSalt, sSecret, sAssociatedData,
	                                   cost.iMemoryKiB, cost.iIterations, cost.iParallelism, iTagLength);

} // Derive

//-----------------------------------------------------------------------------
KArgon2::KArgon2(KDuration Duration, uint32_t iMemoryKiB, bool bComputeAtFirstUse)
//-----------------------------------------------------------------------------
{
	m_Cost.iMemoryKiB = iMemoryKiB;
	ComputeCost(Duration, bComputeAtFirstUse);

} // ctor

//-----------------------------------------------------------------------------
bool KArgon2::SetCost(Cost cost)
//-----------------------------------------------------------------------------
{
	if (cost.iParallelism < 1 || cost.iParallelism > 0x00ffffff)
	{
		return SetError(kFormat("invalid parallelism: {}", cost.iParallelism));
	}

	if (cost.iIterations < 1)
	{
		return SetError(kFormat("invalid count of iterations: {}", cost.iIterations));
	}

	if (cost.iMemoryKiB / 8 < cost.iParallelism)
	{
		return SetError(kFormat("invalid memory size: {} KiB for a parallelism of {}", cost.iMemoryKiB, cost.iParallelism));
	}

	m_Cost              = cost;
	m_bComputeAtNextUse = false;

	kDebug(2, "cost set to {} KiB, {} iterations, parallelism {}", m_Cost.iMemoryKiB, m_Cost.iIterations, m_Cost.iParallelism);

	return true;

} // SetCost

//-----------------------------------------------------------------------------
void KArgon2::ComputeCost(KDuration Duration, bool bComputeAtNextUse)
//-----------------------------------------------------------------------------
{
	// compute a count of iterations that results in less or equal the requested
	// duration - the time grows linearly with it

	m_Duration = Duration;

	if (bComputeAtNextUse)
	{
		m_bComputeAtNextUse = true;
		return;
	}

	m_bComputeAtNextUse = false;

	auto cost = m_Cost;
	cost.iIterations = 1;

	if (cost.iMemoryKiB / 8 < cost.iParallelism)
	{
		cost.iMemoryKiB = 8 * cost.iParallelism;
	}

	KStopTime Timer;

	auto sTag = Derive(Variant::Argon2id, "any password would do", "any salt would do", cost);

	auto Elapsed = Timer.elapsed();

	kSafeZeroize(sTag);

	if (Elapsed.nanoseconds().count() > 0)
	{
		auto iIterations = Duration.nanoseconds().count() / Elapsed.nanoseconds().count();

		if (iIterations > std::numeric_limits<uint32_t>::max())
		{
			iIterations = std::numeric_limits<uint32_t>::max();
		}

		if (iIterations > 1)
		{
			cost.iIterations = static_cast<uint32_t>(iIterations);
		}
	}

	SetCost(cost);

} // ComputeCost

//-----------------------------------------------------------------------------
KArgon2::Cost KArgon2::GetCost()
//-----------------------------------------------------------------------------
{
	if (m_bComputeAtNextUse)
	{
		ComputeCost(m_Duration, false);
	}

	return m_Cost;

} // GetCost

//-----------------------------------------------------------------------------
bool KArgon2::Parse(KStringView sHash, Parts& parts) const
//-----------------------------------------------------------------------------
{
	// $argon2id$v=19$m=19456,t=2,p=1$<salt>$<tag>
	if (!sHash.remove_prefix('$'))
	{
		return SetError("hash does not start with $");
	}

	auto NextField = [&sHash]() -> KStringView
	{
		auto iEnd   = sHash.find('$');
		auto sField = sHash.substr(0, iEnd);
		sHash.remove_prefix(iEnd == KStringView::npos ? sHash.size() : iEnd + 1);
		return sField;
	};

	auto sVariant = NextField();

	if (sVariant == "argon2id")
	{
		parts.variant = Variant::Argon2id;
	}
	else if (sVariant == "argon2i")
	{
		parts.variant = Variant::Argon2i;
	}
	else if (sVariant == "argon2d")
	{
		parts.variant = Variant::Argon2d;
	}
	else
	{
		return SetError(kFormat("no Argon2 hash: {}", sVariant));
	}

	// hashes of the first version of Argon2 have no version field - those are not supported
	auto sVersion = NextField();

	if (sVersion != "v=19")
	{
		return SetError(kFormat("unsupported version of Argon2: {}", sVersion));
	}

	auto sParameters = NextField();

	bool bMemory      { false };
	bool bIterations  { false };
	bool bParallelism { false };

	while (!sParameters.empty())
	{
		auto iEnd       = sParameters.find(',');
		auto sParameter = sParameters.substr(0, iEnd);
		sParameters.remove_prefix(iEnd == KStringView::npos ? sParameters.size() : iEnd + 1);

		auto iEqual = sParameter.find('=');
		auto sName  = sParameter.substr(0, iEqual);
		auto sValue = (iEqual == KStringView::npos) ? KStringView{} : sParameter.substr(iEqual + 1);

		// a count with up to ten digits
		if (sValue.empty() || sValue.size() > 10 || !kIsUnsigned(sValue) || sValue.UInt64() > std::numeric_limits<uint32_t>::max())
		{
			return SetError(kFormat("invalid parameter: {}", sParameter));
		}

		auto iValue = sValue.UInt32();

		if (sName == "m")
		{
			parts.cost.iMemoryKiB = iValue;
			bMemory = true;
		}
		else if (sName == "t")
		{
			parts.cost.iIterations = iValue;
			bIterations = true;
		}
		else if (sName == "p")
		{
			parts.cost.iParallelism = iValue;
			bParallelism = true;
		}
		else
		{
			return SetError(kFormat("unsupported parameter: {}", sParameter));
		}
	}

	if (!bMemory || !bIterations || !bParallelism)
	{
		return SetError("missing parameter");
	}

	auto sSalt = NextField();
	auto sTag  = sHash;

	if (!IsField(sSalt) || !IsField(sTag))
	{
		return SetError("invalid salt or tag");
	}

	parts.sSalt = KBase64::Decode(sSalt);
	parts.sTag  = KBase64::Decode(sTag);

	if (parts.sSalt.size() < 8 || parts.sTag.size() < 4)
	{
		return SetError("salt or tag too short");
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
KString KArgon2::GenerateHash(KStringView sPassword)
//-----------------------------------------------------------------------------
{
	if (m_bComputeAtNextUse)
	{
		ComputeCost(m_Duration, false);
	}

	auto Timer = kWouldLog(2) ? KStopTime() : KStopTime(KStopTime::Halted);

	auto sSalt = kGetRandom(iSaltLength);

	if (sSalt.size() != iSaltLength)
	{
		SetError(kFormat("cannot get randomness for salt generation, got {} instead of {} bytes", sSalt.size(), iSaltLength));
		return {};
	}

	auto sTag = Derive(Variant::Argon2id, sPassword, sSalt, m_Cost, iDefaultTagLength);

	if (sTag.size() != iDefaultTagLength)
	{
		SetError("cannot hash password");
		return {};
	}

	kDebug(2, "password hash generation took {}", Timer.elapsed());

	auto sResult = kFormat("$argon2id$v=19$m={},t={},p={}${}${}",
	                       m_Cost.iMemoryKiB, m_Cost.iIterations, m_Cost.iParallelism,
	                       EncodeField(sSalt), EncodeField(sTag));

	kSafeZeroize(sTag);

	return sResult;

} // GenerateHash

//-----------------------------------------------------------------------------
bool KArgon2::ValidatePassword(KStringView sPassword, KStringView sHash)
//-----------------------------------------------------------------------------
{
	auto Timer = kWouldLog(2) ? KStopTime() : KStopTime(KStopTime::Halted);

	Parts parts;

	if (!Parse(sHash, parts))
	{
		return false;
	}

	auto sTag = Derive(parts.variant, sPassword, parts.sSalt, parts.cost, parts.sTag.size());

	if (sTag.empty())
	{
		return SetError("cannot hash password with the parameters of the hash");
	}

	auto bValid = KDigest::ConstantTimeCompare(sTag, parts.sTag);

	kSafeZeroize(sTag);

	kDebug(2, "password verification took {} and {}", Timer.elapsed(), bValid ? "succeeded" : "failed");

	return bValid;

} // ValidatePassword

//-----------------------------------------------------------------------------
bool KArgon2::NeedsRehash(KStringView sHash)
//-----------------------------------------------------------------------------
{
	Parts parts;

	if (!Parse(sHash, parts))
	{
		// no Argon2 hash, e.g. one of bcrypt
		return true;
	}

	auto cost = GetCost();

	return parts.variant           != Variant::Argon2id
	    || parts.cost.iMemoryKiB    < cost.iMemoryKiB
	    || parts.cost.iIterations   < cost.iIterations
	    || parts.cost.iParallelism  < cost.iParallelism
	    || parts.sSalt.size()       < iSaltLength
	    || parts.sTag.size()        < iDefaultTagLength;

} // NeedsRehash

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr uint32_t KArgon2::iDefaultMemoryKiB;
constexpr uint16_t KArgon2::iSaltLength;
constexpr uint16_t KArgon2::iDefaultTagLength;
#endif

DEKAF2_NAMESPACE_END
