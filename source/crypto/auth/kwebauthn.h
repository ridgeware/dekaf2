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

/// @file kwebauthn.h
/// the server side of WebAuthn, for logins with passkeys

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/duration/kduration.h>
#include <cstdint>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The relying party of WebAuthn (W3C Web Authentication, Level 3): the server
/// that registers passkeys and verifies logins with them.
///
/// Each of the two ceremonies has two calls. The first creates the options for
/// the browser, with a new challenge. The second verifies what the browser sent
/// back. The class keeps nothing between the two: the caller stores the challenge
/// with the pending registration or login, and the credentials with its users.
///
/// @code
/// KWebAuthn::Config Config;
/// Config.sRelyingPartyID   = "example.org";
/// Config.sRelyingPartyName = "Example";
/// Config.Origins           = { "https://example.org" };
///
/// KWebAuthn WebAuthn(Config);
///
/// // registration, for a user that is logged in
/// KString sChallenge;
/// auto jOptions = WebAuthn.RegistrationOptions({ sUserHandle, "jdoe", "John Doe" }, sChallenge);
/// // .. send jOptions to navigator.credentials.create(), receive jResponse ..
/// KWebAuthn::Credential Credential;
/// if (WebAuthn.VerifyRegistration(jResponse, sChallenge, Credential)) { /* store it */ }
///
/// // login
/// auto jOptions = WebAuthn.AuthenticationOptions(sChallenge);
/// // .. send jOptions to navigator.credentials.get(), receive jResponse ..
/// auto Credential = LoadCredential(KWebAuthn::GetCredentialID(jResponse));
/// if (WebAuthn.VerifyAuthentication(jResponse, sChallenge, Credential)) { /* store it, log in its user */ }
/// @endcode
///
/// The options and the responses have the JSON form of WebAuthn, in which all
/// binary values are base64url: the options are the input of
/// PublicKeyCredential.parseCreationOptionsFromJSON() and
/// parseRequestOptionsFromJSON(), the responses the output of
/// PublicKeyCredential.toJSON().
///
/// Supported are public keys of the algorithms ES256, RS256, and EdDSA with
/// Ed25519 where OpenSSL has it. The class asks for no attestation. An
/// attestation statement that an authenticator sends anyway is not verified,
/// only its format is noted: there is no proof of the make of an authenticator.
///
/// An object is not for the concurrent use by several threads, because of its
/// error state.
class DEKAF2_PUBLIC KWebAuthn : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// whether the authenticator has to verify the user, by biometry or a PIN
	enum class UserVerification : uint8_t
	{
		Required,    ///< a response without user verification is refused
		Preferred,   ///< asked for, but not checked
		Discouraged  ///< not asked for
	};

	/// whether the authenticator has to store the credential with its user, which
	/// makes a login without a typed user name possible
	enum class Discoverable : uint8_t
	{
		Required,
		Preferred,
		Discouraged
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct DEKAF2_PUBLIC Config
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the ID of the relying party: the host name of the origins, or a parent domain of it
		KString              sRelyingPartyID;
		/// the name of the relying party, as authenticators show it
		KString              sRelyingPartyName;
		/// the origins that responses may come from, like "https://example.org", compared as they are
		std::vector<KString> Origins;
		/// the origins of the top pages that may embed one of the origins in a frame. As long as
		/// this is empty, a response from a frame of another origin is refused.
		std::vector<KString> TopOrigins;
		UserVerification     Verification { UserVerification::Required };
		Discoverable         Discovery    { Discoverable::Required     };
		/// the time the browser gives the user for a ceremony
		KDuration            Timeout      { chrono::minutes(5)         };
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the user that registers a credential
	struct DEKAF2_PUBLIC User
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// up to 64 bytes that identify the user for the authenticator. This should be random
		/// bytes and no name: an authenticator hands it out without user verification.
		KString sHandle;
		/// the name the user logs in with
		KString sName;
		/// the name of the user for display
		KString sDisplayName;
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// What the relying party stores of a credential. The binary values are bytes, not base64url.
	struct DEKAF2_PUBLIC Credential
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the ID of the credential, up to 1023 bytes
		KString              sID;
		/// the public key as a COSE key, as the authenticator sent it
		KString              sPublicKey;
		/// The handle of the user that owns the credential. The caller sets it, the verification
		/// of a registration does not. If set, a user handle in a login has to be the same.
		KString              sUserHandle;
		/// the signature counter of the last login, 0 for authenticators that do not count
		uint32_t             iSignCount      { 0 };
		/// the authenticator verified the user at the registration or at a later login
		bool                 bUserVerified   { false };
		/// the credential may be copied to other devices, like a synced passkey
		bool                 bBackupEligible { false };
		/// the credential was copied at the time of the last login
		bool                 bBackedUp       { false };
		/// how the browser reaches the authenticator ("internal", "hybrid", "usb", ..), from the registration
		std::vector<KString> Transports;
		/// the format of the attestation statement of the registration, "none" without one
		KString              sAttestationFormat;
		/// the 16 bytes that name the make of the authenticator, all zero if it does not tell
		KString              sAAGUID;
	};

	KWebAuthn (Config config);

	/// @return the configuration
	DEKAF2_NODISCARD
	const Config& GetConfig () const { return m_Config; }

	/// create the options for navigator.credentials.create()
	/// @param user the user that registers a credential
	/// @param sChallenge receives the new challenge, to keep until the response arrives
	/// @param ExcludeIDs the IDs of the credentials that the user has already, so that an
	/// authenticator does not register twice
	/// @return the options in their JSON form
	DEKAF2_NODISCARD
	KJSON RegistrationOptions (const User& user, KString& sChallenge, const std::vector<KString>& ExcludeIDs = {}) const;

	/// verify the response to a registration
	/// @param jResponse the response of the browser in its JSON form
	/// @param sChallenge the challenge of the options
	/// @param credential receives the new credential, with the exception of its user handle
	/// @return false if the response is not valid. The caller has to refuse a credential ID
	/// that is registered already, and should use a challenge once.
	DEKAF2_NODISCARD
	bool VerifyRegistration (const KJSON& jResponse, KStringView sChallenge, Credential& credential) const;

	/// create the options for navigator.credentials.get()
	/// @param sChallenge receives the new challenge, to keep until the response arrives
	/// @param AllowIDs the IDs of the credentials of the user, if the user is known already.
	/// Without them the authenticator offers its discoverable credentials.
	/// @return the options in their JSON form
	DEKAF2_NODISCARD
	KJSON AuthenticationOptions (KString& sChallenge, const std::vector<KString>& AllowIDs = {}) const;

	/// verify the response to a login
	/// @param jResponse the response of the browser in its JSON form
	/// @param sChallenge the challenge of the options
	/// @param credential the stored credential with the ID of the response, see GetCredentialID().
	/// A valid response updates its signature counter and its flags, to store again.
	/// @return false if the response is not valid, also if the signature counter did not advance,
	/// which is the sign of a copied authenticator
	DEKAF2_NODISCARD
	bool VerifyAuthentication (const KJSON& jResponse, KStringView sChallenge, Credential& credential) const;

	/// @return the ID of the credential of a response in bytes, not verified
	DEKAF2_NODISCARD
	static KString GetCredentialID (const KJSON& jResponse);

	/// @return the user handle of a login response in bytes, not verified, empty if it has none
	DEKAF2_NODISCARD
	static KString GetUserHandle (const KJSON& jResponse);

//----------
private:
//----------

	bool CheckClientData        (KStringView sClientData, KStringView sType, KStringView sChallenge) const;
	bool CheckAuthenticatorData (KStringView sRelyingPartyHash, uint8_t iFlags) const;

	Config m_Config;

}; // KWebAuthn

/// @}

DEKAF2_NAMESPACE_END
