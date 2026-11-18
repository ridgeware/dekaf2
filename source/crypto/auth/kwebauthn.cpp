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


#include <dekaf2/crypto/auth/kwebauthn.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/encoding/kcbor.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/ec/ked25519sign.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/core/format/kformat.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

namespace {

// the flags of the authenticator data
constexpr uint8_t iFlagUserPresent     = 0x01;
constexpr uint8_t iFlagUserVerified    = 0x04;
constexpr uint8_t iFlagBackupEligible  = 0x08;
constexpr uint8_t iFlagBackedUp        = 0x10;
constexpr uint8_t iFlagAttestedData    = 0x40;
constexpr uint8_t iFlagExtensions      = 0x80;

// the labels of a COSE key (RFC 9052 and RFC 9053) and the values this class knows
constexpr int64_t iCoseKeyType         = 1;
constexpr int64_t iCoseAlgorithm       = 3;
constexpr int64_t iCoseCurve           = -1; // for the key types OKP and EC2
constexpr int64_t iCoseX               = -2;
constexpr int64_t iCoseY               = -3;
constexpr int64_t iCoseModulus         = -1; // for the key type RSA
constexpr int64_t iCoseExponent        = -2;

constexpr int64_t iKeyTypeOKP          = 1;
constexpr int64_t iKeyTypeEC2          = 2;
constexpr int64_t iKeyTypeRSA          = 3;
constexpr int64_t iCurveP256           = 1;
constexpr int64_t iCurveEd25519        = 6;
constexpr int64_t iAlgorithmES256      = -7;
constexpr int64_t iAlgorithmEdDSA      = -8;
constexpr int64_t iAlgorithmRS256      = -257;

constexpr std::size_t iMaxCredentialID = 1023;
constexpr std::size_t iMinRSAModulus   = 2048 / 8;

//-----------------------------------------------------------------------------
bool Contains(const std::vector<KString>& List, KStringView sValue)
//-----------------------------------------------------------------------------
{
	return std::find(List.begin(), List.end(), sValue) != List.end();
}

//-----------------------------------------------------------------------------
KStringViewZ ToString(KWebAuthn::UserVerification Verification)
//-----------------------------------------------------------------------------
{
	switch (Verification)
	{
		case KWebAuthn::UserVerification::Required:    return "required";
		case KWebAuthn::UserVerification::Preferred:   return "preferred";
		case KWebAuthn::UserVerification::Discouraged: return "discouraged";
	}

	return "";
}

//-----------------------------------------------------------------------------
KStringViewZ ToString(KWebAuthn::Discoverable Discovery)
//-----------------------------------------------------------------------------
{
	switch (Discovery)
	{
		case KWebAuthn::Discoverable::Required:    return "required";
		case KWebAuthn::Discoverable::Preferred:   return "preferred";
		case KWebAuthn::Discoverable::Discouraged: return "discouraged";
	}

	return "";
}

//-----------------------------------------------------------------------------
/// @return a list of credentials for the options, from their IDs in bytes
KJSON CredentialList(const std::vector<KString>& IDs)
//-----------------------------------------------------------------------------
{
	KJSON jList = KJSON::array();

	for (auto& sID : IDs)
	{
		jList.push_back(KJSON
		{
			{ "type", "public-key"             },
			{ "id"  , KBase64Url::Encode(sID)  }
		});
	}

	return jList;
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The authenticator data: what the authenticator signs together with the hash
/// of the client data. The views point into the parsed string.
struct AuthenticatorData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	bool Parse (KStringView sData, KString& sError);
	bool Has   (uint8_t iFlag) const { return (iFlags & iFlag) != 0; }

	KStringView sRelyingPartyHash;
	uint8_t     iFlags     { 0 };
	uint32_t    iSignCount { 0 };
	// only with the flag for attested credential data
	KStringView sAAGUID;
	KStringView sCredentialID;
	KStringView sPublicKey;
	KCBOR       PublicKey;
};

//-----------------------------------------------------------------------------
bool AuthenticatorData::Parse(KStringView sData, KString& sError)
//-----------------------------------------------------------------------------
{
	// the hash of the relying party ID, the flags, and the signature counter
	if (sData.size() < 32 + 1 + 4)
	{
		sError = "the authenticator data is too short";
		return false;
	}

	sRelyingPartyHash = sData.substr(0, 32);
	iFlags            = static_cast<uint8_t>(sData[32]);
	iSignCount        = 0;

	for (std::size_t iPos = 33; iPos < 37; ++iPos)
	{
		iSignCount = (iSignCount << 8) | static_cast<uint8_t>(sData[iPos]);
	}

	sData.remove_prefix(37);

	if (Has(iFlagAttestedData))
	{
		// the AAGUID, the length of the credential ID, the ID, and the public key
		if (sData.size() < 16 + 2)
		{
			sError = "the attested credential data is too short";
			return false;
		}

		sAAGUID = sData.substr(0, 16);

		auto iLength = (static_cast<std::size_t>(static_cast<uint8_t>(sData[16])) << 8)
		              | static_cast<std::size_t>(static_cast<uint8_t>(sData[17]));

		sData.remove_prefix(18);

		if (iLength > sData.size())
		{
			sError = "the credential ID is longer than the authenticator data";
			return false;
		}

		sCredentialID = sData.substr(0, iLength);
		sData.remove_prefix(iLength);

		auto sKeyStart = sData;

		if (!PublicKey.ParseFirst(sData))
		{
			sError = kFormat("the public key is not valid CBOR: {}", PublicKey.Error());
			return false;
		}

		sPublicKey = sKeyStart.substr(0, sKeyStart.size() - sData.size());
	}

	if (Has(iFlagExtensions))
	{
		KCBOR Extensions;

		if (!Extensions.ParseFirst(sData) || !Extensions.IsMap())
		{
			sError = "the extensions of the authenticator data are no CBOR map";
			return false;
		}
	}

	if (!sData.empty())
	{
		sError = "the authenticator data has bytes after its end";
		return false;
	}

	return true;

} // AuthenticatorData::Parse

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the public key of a credential, loaded from its COSE key
class PublicKey
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	bool Load   (const KCBOR& Key, KString& sError);
	bool Verify (KStringView sData, KStringView sSignature) const;

//----------
private:
//----------

	enum class Algorithm : uint8_t { None, ES256, RS256, EdDSA };

	Algorithm   m_Algorithm { Algorithm::None };
	KECKey      m_ECKey;
	KRSAKey     m_RSAKey;
#if DEKAF2_HAS_ED25519
	KEd25519Key m_Ed25519Key;
#endif

}; // PublicKey

//-----------------------------------------------------------------------------
bool PublicKey::Load(const KCBOR& Key, KString& sError)
//-----------------------------------------------------------------------------
{
	m_Algorithm = Algorithm::None;

	if (!Key.IsMap() || !Key(iCoseKeyType).IsInteger() || !Key(iCoseAlgorithm).IsInteger())
	{
		sError = "the public key is no COSE key";
		return false;
	}

	auto iKeyType   = Key(iCoseKeyType  ).GetInt64();
	auto iAlgorithm = Key(iCoseAlgorithm).GetInt64();

	if (iKeyType == iKeyTypeEC2 && iAlgorithm == iAlgorithmES256)
	{
		auto& X = Key(iCoseX);
		auto& Y = Key(iCoseY);

		if (Key(iCoseCurve) != iCurveP256 || !X.IsBytes() || !Y.IsBytes() || X.size() != 32 || Y.size() != 32)
		{
			sError = "the public key for ES256 is no point of the curve P-256";
			return false;
		}

		// the uncompressed point: 0x04 || x || y
		KString sPoint;
		sPoint.reserve(65);
		sPoint += '\x04';
		sPoint += X.GetString();
		sPoint += Y.GetString();

		if (!m_ECKey.CreateFromRaw(sPoint, false))
		{
			sError = kFormat("the public key for ES256 is not valid: {}", m_ECKey.Error());
			return false;
		}

		m_Algorithm = Algorithm::ES256;
		return true;
	}

	if (iKeyType == iKeyTypeRSA && iAlgorithm == iAlgorithmRS256)
	{
		auto& Modulus  = Key(iCoseModulus );
		auto& Exponent = Key(iCoseExponent);

		if (!Modulus.IsBytes() || !Exponent.IsBytes() || Exponent.size() == 0)
		{
			sError = "the public key for RS256 has no modulus or no exponent";
			return false;
		}

		if (Modulus.size() < iMinRSAModulus)
		{
			sError = "the public key for RS256 is shorter than 2048 bits";
			return false;
		}

		if (!m_RSAKey.CreateFromRaw(Modulus.GetString(), Exponent.GetString()))
		{
			sError = kFormat("the public key for RS256 is not valid: {}", m_RSAKey.Error());
			return false;
		}

		m_Algorithm = Algorithm::RS256;
		return true;
	}

#if DEKAF2_HAS_ED25519
	if (iKeyType == iKeyTypeOKP && iAlgorithm == iAlgorithmEdDSA)
	{
		auto& X = Key(iCoseX);

		if (Key(iCoseCurve) != iCurveEd25519 || !X.IsBytes() || X.size() != 32)
		{
			sError = "the public key for EdDSA is no key of the curve Ed25519";
			return false;
		}

		if (!m_Ed25519Key.CreateFromRaw(X.GetString(), false))
		{
			sError = kFormat("the public key for EdDSA is not valid: {}", m_Ed25519Key.Error());
			return false;
		}

		m_Algorithm = Algorithm::EdDSA;
		return true;
	}
#endif

	sError = kFormat("the public key has the key type {} and the algorithm {}, which are not supported", iKeyType, iAlgorithm);
	return false;

} // PublicKey::Load

//-----------------------------------------------------------------------------
bool PublicKey::Verify(KStringView sData, KStringView sSignature) const
//-----------------------------------------------------------------------------
{
	switch (m_Algorithm)
	{
		case Algorithm::ES256:
			return KECVerify().Verify(m_ECKey, sData, sSignature, KECVerify::Format::Der);

		case Algorithm::RS256:
			return KRSAVerify(KRSAVerify::Digest::SHA256, sData).Verify(m_RSAKey, sSignature);

		case Algorithm::EdDSA:
#if DEKAF2_HAS_ED25519
			return KEd25519Verify().Verify(m_Ed25519Key, sData, sSignature);
#else
			return false;
#endif

		case Algorithm::None:
			break;
	}

	return false;

} // PublicKey::Verify

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KWebAuthn::KWebAuthn(Config config)
//-----------------------------------------------------------------------------
: m_Config(std::move(config))
{
}

//-----------------------------------------------------------------------------
KJSON KWebAuthn::RegistrationOptions(const User& user, KString& sChallenge, const std::vector<KString>& ExcludeIDs) const
//-----------------------------------------------------------------------------
{
	sChallenge = KBase64Url::Encode(kGetRandom(32));

	// in the order of preference
	KJSON jAlgorithms = KJSON::array();
	jAlgorithms.push_back(KJSON{{ "type", "public-key" }, { "alg", iAlgorithmES256 }});
#if DEKAF2_HAS_ED25519
	jAlgorithms.push_back(KJSON{{ "type", "public-key" }, { "alg", iAlgorithmEdDSA }});
#endif
	jAlgorithms.push_back(KJSON{{ "type", "public-key" }, { "alg", iAlgorithmRS256 }});

	KJSON jOptions;

	jOptions["rp"] =
	{
		{ "id"  , m_Config.sRelyingPartyID   },
		{ "name", m_Config.sRelyingPartyName }
	};

	jOptions["user"] =
	{
		{ "id"         , KBase64Url::Encode(user.sHandle) },
		{ "name"       , user.sName                       },
		{ "displayName", user.sDisplayName                }
	};

	jOptions["challenge"]          = sChallenge;
	jOptions["pubKeyCredParams"]   = std::move(jAlgorithms);
	jOptions["timeout"]            = static_cast<int64_t>(m_Config.Timeout.milliseconds().count());
	jOptions["excludeCredentials"] = CredentialList(ExcludeIDs);
	jOptions["attestation"]        = "none";

	jOptions["authenticatorSelection"] =
	{
		{ "residentKey"       , ToString(m_Config.Discovery)                    },
		// the member of WebAuthn Level 1 for the same
		{ "requireResidentKey", m_Config.Discovery == Discoverable::Required    },
		{ "userVerification"  , ToString(m_Config.Verification)                 }
	};

	return jOptions;

} // RegistrationOptions

//-----------------------------------------------------------------------------
KJSON KWebAuthn::AuthenticationOptions(KString& sChallenge, const std::vector<KString>& AllowIDs) const
//-----------------------------------------------------------------------------
{
	sChallenge = KBase64Url::Encode(kGetRandom(32));

	KJSON jOptions;

	jOptions["challenge"]        = sChallenge;
	jOptions["timeout"]          = static_cast<int64_t>(m_Config.Timeout.milliseconds().count());
	jOptions["rpId"]             = m_Config.sRelyingPartyID;
	jOptions["allowCredentials"] = CredentialList(AllowIDs);
	jOptions["userVerification"] = ToString(m_Config.Verification);

	return jOptions;

} // AuthenticationOptions

//-----------------------------------------------------------------------------
KString KWebAuthn::GetCredentialID(const KJSON& jResponse)
//-----------------------------------------------------------------------------
{
	// both members hold the ID in base64url
	auto& sRawID = jResponse("rawId").String();

	return KBase64Url::Decode(sRawID.empty() ? jResponse("id").String() : sRawID);
}

//-----------------------------------------------------------------------------
KString KWebAuthn::GetUserHandle(const KJSON& jResponse)
//-----------------------------------------------------------------------------
{
	return KBase64Url::Decode(jResponse("response")("userHandle").String());
}

//-----------------------------------------------------------------------------
bool KWebAuthn::CheckClientData(KStringView sClientData, KStringView sType, KStringView sChallenge) const
//-----------------------------------------------------------------------------
{
	// a byte order mark is no part of the JSON
	sClientData.remove_prefix("\xef\xbb\xbf");

	KJSON jClientData;

	if (!jClientData.Parse(sClientData) || !jClientData.is_object())
	{
		return SetError("the client data is no JSON object");
	}

	if (jClientData("type").String() != sType)
	{
		return SetError(kFormat("the client data has the type '{}', expected was '{}'", jClientData("type").String(), sType));
	}

	if (sChallenge.empty() || jClientData("challenge").String() != sChallenge)
	{
		return SetError("the client data has another challenge");
	}

	auto& sOrigin = jClientData("origin").String();

	if (!Contains(m_Config.Origins, sOrigin))
	{
		return SetError(kFormat("the origin '{}' is not expected", sOrigin));
	}

	auto& jTopOrigin = jClientData("topOrigin");

	if (jClientData("crossOrigin").Bool() || !jTopOrigin.is_null())
	{
		// the response comes from a frame in a page of another origin
		if (m_Config.TopOrigins.empty())
		{
			return SetError("the response comes from a frame of another origin");
		}

		if (!jTopOrigin.is_null() && !Contains(m_Config.TopOrigins, jTopOrigin.String()))
		{
			return SetError(kFormat("the top origin '{}' is not expected", jTopOrigin.String()));
		}
	}

	return true;

} // CheckClientData

//-----------------------------------------------------------------------------
bool KWebAuthn::CheckAuthenticatorData(KStringView sRelyingPartyHash, uint8_t iFlags) const
//-----------------------------------------------------------------------------
{
	if (m_Config.sRelyingPartyID.empty() || sRelyingPartyHash != KSHA256(m_Config.sRelyingPartyID).Digest())
	{
		return SetError("the authenticator data is for another relying party");
	}

	if ((iFlags & iFlagUserPresent) == 0)
	{
		return SetError("the authenticator did not test the presence of the user");
	}

	if (m_Config.Verification == UserVerification::Required && (iFlags & iFlagUserVerified) == 0)
	{
		return SetError("the authenticator did not verify the user");
	}

	if ((iFlags & iFlagBackupEligible) == 0 && (iFlags & iFlagBackedUp) != 0)
	{
		return SetError("the credential is backed up, but not eligible for a backup");
	}

	return true;

} // CheckAuthenticatorData

//-----------------------------------------------------------------------------
bool KWebAuthn::VerifyRegistration(const KJSON& jResponse, KStringView sChallenge, Credential& credential) const
//-----------------------------------------------------------------------------
{
	if (jResponse("type").String() != "public-key")
	{
		return SetError("the response is no public key credential");
	}

	auto& jInner      = jResponse("response");
	auto  sClientData = KBase64Url::Decode(jInner("clientDataJSON").String());

	if (!CheckClientData(sClientData, "webauthn.create", sChallenge))
	{
		return false;
	}

	KCBOR Attestation;

	if (!Attestation.Parse(KBase64Url::Decode(jInner("attestationObject").String())))
	{
		return SetError(kFormat("the attestation object is not valid CBOR: {}", Attestation.Error()));
	}

	auto& Format    = Attestation("fmt");
	auto& Statement = Attestation("attStmt");
	auto& Data      = Attestation("authData");

	if (!Format.IsText() || !Statement.IsMap() || !Data.IsBytes())
	{
		return SetError("the attestation object lacks its format, its statement or the authenticator data");
	}

	AuthenticatorData AuthData;
	KString           sError;

	if (!AuthData.Parse(Data.GetString(), sError))
	{
		return SetError(sError);
	}

	if (!CheckAuthenticatorData(AuthData.sRelyingPartyHash, AuthData.iFlags))
	{
		return false;
	}

	if (!AuthData.Has(iFlagAttestedData))
	{
		return SetError("the authenticator data has no credential");
	}

	if (AuthData.sCredentialID.empty() || AuthData.sCredentialID.size() > iMaxCredentialID)
	{
		return SetError(kFormat("the credential ID has {} bytes, allowed are 1 to {}", AuthData.sCredentialID.size(), iMaxCredentialID));
	}

	if (GetCredentialID(jResponse) != AuthData.sCredentialID)
	{
		return SetError("the response names another credential ID than the authenticator data");
	}

	// only the algorithms of the options load
	PublicKey Key;

	if (!Key.Load(AuthData.PublicKey, sError))
	{
		return SetError(sError);
	}

	if (Format.GetString() == "none" && Statement.size() != 0)
	{
		return SetError("the attestation format is 'none', but there is an attestation statement");
	}

	// a statement of any other format is not verified

	credential.sID                = AuthData.sCredentialID;
	credential.sPublicKey         = AuthData.sPublicKey;
	credential.iSignCount         = AuthData.iSignCount;
	credential.bUserVerified      = AuthData.Has(iFlagUserVerified);
	credential.bBackupEligible    = AuthData.Has(iFlagBackupEligible);
	credential.bBackedUp          = AuthData.Has(iFlagBackedUp);
	credential.sAttestationFormat = Format.GetString();
	credential.sAAGUID            = AuthData.sAAGUID;
	credential.Transports.clear();

	for (auto& jTransport : jInner("transports").Array())
	{
		if (jTransport.is_string())
		{
			credential.Transports.push_back(jTransport.String());
		}
	}

	return true;

} // VerifyRegistration

//-----------------------------------------------------------------------------
bool KWebAuthn::VerifyAuthentication(const KJSON& jResponse, KStringView sChallenge, Credential& credential) const
//-----------------------------------------------------------------------------
{
	if (jResponse("type").String() != "public-key")
	{
		return SetError("the response is no public key credential");
	}

	if (credential.sID.empty() || GetCredentialID(jResponse) != credential.sID)
	{
		return SetError("the response is for another credential");
	}

	auto sUserHandle = GetUserHandle(jResponse);

	if (!sUserHandle.empty() && !credential.sUserHandle.empty() && sUserHandle != credential.sUserHandle)
	{
		return SetError("the response is for another user");
	}

	auto& jInner      = jResponse("response");
	auto  sClientData = KBase64Url::Decode(jInner("clientDataJSON").String());

	if (!CheckClientData(sClientData, "webauthn.get", sChallenge))
	{
		return false;
	}

	auto              sData = KBase64Url::Decode(jInner("authenticatorData").String());
	AuthenticatorData AuthData;
	KString           sError;

	if (!AuthData.Parse(sData, sError))
	{
		return SetError(sError);
	}

	if (!CheckAuthenticatorData(AuthData.sRelyingPartyHash, AuthData.iFlags))
	{
		return false;
	}

	if (AuthData.Has(iFlagBackupEligible) != credential.bBackupEligible)
	{
		return SetError("the backup eligibility of the credential changed");
	}

	KCBOR CoseKey;

	if (!CoseKey.Parse(credential.sPublicKey))
	{
		return SetError(kFormat("the stored public key is not valid CBOR: {}", CoseKey.Error()));
	}

	PublicKey Key;

	if (!Key.Load(CoseKey, sError))
	{
		return SetError(sError);
	}

	// the signature covers the authenticator data and the hash of the client data
	KString sSigned = sData;
	sSigned += KSHA256(sClientData).Digest();

	if (!Key.Verify(sSigned, KBase64Url::Decode(jInner("signature").String())))
	{
		return SetError("the signature is not valid");
	}

	// authenticators that do not count send 0 every time
	if ((AuthData.iSignCount != 0 || credential.iSignCount != 0) && AuthData.iSignCount <= credential.iSignCount)
	{
		return SetError(kFormat("the signature counter went from {} to {}: the authenticator may be copied", credential.iSignCount, AuthData.iSignCount));
	}

	credential.iSignCount = AuthData.iSignCount;
	credential.bBackedUp  = AuthData.Has(iFlagBackedUp);

	if (AuthData.Has(iFlagUserVerified))
	{
		credential.bUserVerified = true;
	}

	return true;

} // VerifyAuthentication

DEKAF2_NAMESPACE_END
