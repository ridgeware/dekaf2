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

/// @file ktotp.h
/// Time-based (TOTP, RFC 6238) and HMAC-based (HOTP, RFC 4226) one-time
/// passwords for two-factor authentication, plus helpers for out-of-band codes
/// (email/SMS OTP and single-use backup codes).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/clock/ktime.h>
#include <cstdint>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Time-based one-time passwords (TOTP, RFC 6238) for authenticator apps, built
/// on the HMAC one-time password (HOTP, RFC 4226). A KTOTP instance wraps one
/// shared secret and computes/verifies its codes; the defaults (SHA-1, 6 digits,
/// 30-second period) are what Google Authenticator, Authy, 1Password etc. expect.
///
/// Enrolment and verification:
/// @code
/// auto totp = KTOTP::Generate();                  // a fresh random secret
/// // persist totp.Secret() for the user, then offer it for enrolment:
/// KString sURI = totp.URI("MyService", "alice");  // otpauth:// - render via KQRCode,
///                                                 // and also show totp.Secret() to type in
/// // ... later, at sign-in:
/// if (KTOTP(sStoredSecret).Verify(sCodeFromUser)) { /* second factor ok */ }
/// @endcode
///
/// The static helpers at the end cover the non-TOTP parts of a 2FA setup: a
/// random code to email or text, and single-use backup codes. Those are random
/// (not derived from the secret) - store only their HashCode() and accept each
/// once.
class DEKAF2_PUBLIC KTOTP
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the HMAC hash used by the code algorithm (authenticator apps almost always
	/// use SHA1, which is the default)
	enum class Algorithm : uint8_t { SHA1, SHA256, SHA512 };

	/// Construct from an existing base32 secret (as produced by Generate()/Secret()).
	explicit KTOTP(KString sBase32Secret);

	/// Generate a KTOTP with a fresh, cryptographically random secret.
	/// @param iSecretBytes raw-entropy bytes behind the secret (20 = 160 bit, the
	/// common default and the RFC 4226 recommended minimum)
	DEKAF2_NODISCARD static KTOTP Generate(uint16_t iSecretBytes = 20);

	/// false if the secret is not valid base32 (so no codes can be computed)
	bool empty() const { return m_sKey.empty(); }
	/// the base32 secret - persist this and show it for manual app entry
	const KString& Secret() const { return m_sSecret; }

	// --- parameters (chainable); defaults match common authenticator apps ---
	/// number of digits in a code (6..8); default 6
	KTOTP&    Digits      (uint8_t iDigits)   { m_iDigits = iDigits; return *this;  }
	/// time step in seconds; default 30
	KTOTP&    Period      (uint16_t iSeconds) { m_iPeriod = iSeconds; return *this; }
	/// the HMAC hash; default SHA1
	KTOTP&    UseAlgorithm(Algorithm Algo)    { m_Algorithm = Algo; return *this;   }

	uint8_t   GetDigits   () const { return m_iDigits;   }
	uint16_t  GetPeriod   () const { return m_iPeriod;   }
	Algorithm GetAlgorithm() const { return m_Algorithm; }

	// --- codes ---
	/// The TOTP value at tWhen (default: now), as an integer in [0, 10^digits).
	DEKAF2_NODISCARD uint32_t Code(KUnixTime tWhen = KUnixTime::now()) const;
	/// The TOTP value at tWhen, zero-padded to the digit count (what the app shows).
	DEKAF2_NODISCARD KString  FormatCode(KUnixTime tWhen = KUnixTime::now()) const;
	/// Verify a user-entered code, tolerating +/- iSkewSteps periods of clock drift
	/// (1 = the spec-recommended one step either side). Spaces/dashes in the input
	/// are ignored; the whole window is always scanned (no early-out timing leak).
	DEKAF2_NODISCARD bool Verify(KStringView sCode, int32_t iSkewSteps = 1,
	                             KUnixTime tWhen = KUnixTime::now()) const;

	/// The otpauth://totp/... provisioning URI. Render it as a QR code (e.g. with
	/// KQRCode) for scanning, and also show Secret() for manual entry.
	DEKAF2_NODISCARD KString URI(KStringView sIssuer, KStringView sAccount) const;

	/// The RFC 4226 HOTP primitive (counter-based) that TOTP is built on, exposed
	/// for reuse (e.g. event-based OTP). @param sRawKey the decoded (not base32) key.
	DEKAF2_NODISCARD static uint32_t HOTP(KStringView sRawKey, uint64_t iCounter,
	                                      uint8_t iDigits = 6, Algorithm Algo = Algorithm::SHA1);

	// --- out-of-band one-time codes (NOT TOTP) -------------------------------
	// Random codes for an emailed/texted OTP and for single-use backup codes.
	// Persist only HashCode() of them and accept each exactly once.

	/// A cryptographically random numeric code of iDigits digits (zero-padded),
	/// e.g. for an emailed sign-in code.
	DEKAF2_NODISCARD static KString GenerateNumericCode(uint8_t iDigits = 6);
	/// iCount human-friendly single-use backup codes, lower-case and grouped as
	/// "xxxxx-xxxxx". Returned in plaintext to show the user once; persist only
	/// their HashCode().
	DEKAF2_NODISCARD static std::vector<KString> GenerateBackupCodes(uint16_t iCount = 10,
	                                                                 uint8_t iGroupLen = 5);
	/// SHA-256 (hex) of a code after normalizing it (dashes/spaces removed,
	/// lower-cased) - so the stored hash does not depend on how the user retyped it.
	DEKAF2_NODISCARD static KString HashCode(KStringView sCode);

//----------
private:
//----------

	KString   m_sSecret;                  ///< the base32 secret
	KString   m_sKey;                     ///< the decoded raw key bytes
	uint8_t   m_iDigits   {  6 };
	uint16_t  m_iPeriod   { 30 };
	Algorithm m_Algorithm { Algorithm::SHA1 };

}; // KTOTP

/// @}

DEKAF2_NAMESPACE_END
