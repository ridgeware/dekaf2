#include "catch.hpp"

#include <dekaf2/crypto/auth/kopenid.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <ctime>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
/// an issuer with its own EC key, which hands out a provider for its key and signs tokens
struct TestIssuer
//-----------------------------------------------------------------------------
{
	TestIssuer(KStringView sIssuer, KStringView sKeyID)
	: sIssuer(sIssuer)
	, sKeyID(sKeyID)
	, Key(true)
	{
		auto sPubRaw = Key.GetPublicKeyRaw();

		JWKS = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32))},
				{"y",   KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32))},
				{"kid", this->sKeyID},
				{"alg", "ES256"},
				{"use", "sig"}
			}
		}} };
	}

	KOpenIDProvider Provider() const
	{
		return KOpenIDProvider(KOpenIDKeys(JWKS), sIssuer);
	}

	/// a token of this issuer, valid from tNotBefore to tExpires (seconds since the epoch)
	KString Token(KStringView sSubject, std::time_t tNotBefore, std::time_t tExpires, KStringView sOtherKeyID = KStringView{}) const
	{
		KJSON jHeader  = { {"alg", "ES256"}, {"kid", sOtherKeyID.empty() ? sKeyID : KString(sOtherKeyID)}, {"typ", "JWT"} };
		KJSON jPayload = { {"iss", sIssuer}, {"sub", KString(sSubject)}, {"nbf", tNotBefore}, {"exp", tExpires} };

		auto sData = kFormat("{}.{}", KBase64Url::Encode(jHeader.dump()), KBase64Url::Encode(jPayload.dump()));

		KECSign Signer;
		return kFormat("{}.{}", sData, KBase64Url::Encode(Signer.Sign(Key, sData)));
	}

	KString sIssuer;
	KString sKeyID;
	KECKey  Key;
	KJSON   JWKS;

}; // TestIssuer

} // end of anonymous namespace

TEST_CASE("KJWT audience matching")
{
	// An EMPTY expected audience must accept every token shape. This is the
	// opt-in default and the reason adding the audience parameter cannot break
	// existing KRESTServer / SSO deployments.
	SECTION("empty expected audience accepts any token (opt-in default)")
	{
		KJSON jString; jString["aud"] = "some-resource";
		KJSON jArray;  jArray ["aud"] = KJSON::array({ "api://a", "api://b" });
		KJSON jNone;   jNone  ["sub"] = "user"; // no aud claim at all

		CHECK ( KJWT::AudienceMatches(jString, "") );
		CHECK ( KJWT::AudienceMatches(jArray,  "") );
		CHECK ( KJWT::AudienceMatches(jNone,   "") );
		CHECK ( KJWT::AudienceMatches(jString, KStringView{}) );
	}

	// --- Step 3: opt-in correctness ------------------------------------------
	SECTION("string aud is matched exactly and case-sensitively")
	{
		KJSON jTok; jTok["aud"] = "client-A";
		CHECK       ( KJWT::AudienceMatches(jTok, "client-A") );
		CHECK_FALSE ( KJWT::AudienceMatches(jTok, "client-B") );
		CHECK_FALSE ( KJWT::AudienceMatches(jTok, "Client-A") );  // case-sensitive
	}

	SECTION("array aud is matched by membership (multi-aud style)")
	{
		KJSON jTok; jTok["aud"] = KJSON::array({ "client-A", "https://api.example" });
		CHECK       ( KJWT::AudienceMatches(jTok, "client-A") );
		CHECK       ( KJWT::AudienceMatches(jTok, "https://api.example") );
		CHECK_FALSE ( KJWT::AudienceMatches(jTok, "client-B") );
	}

	SECTION("a missing aud is rejected only when an audience is actually expected")
	{
		KJSON jTok; jTok["sub"] = "user";
		CHECK_FALSE ( KJWT::AudienceMatches(jTok, "client-A") );
	}
}

TEST_CASE("KJWT token_use matching")
{
	// token_use binding is the guard that keeps an OIDC id_token from being replayed
	// as a bearer access token at a resource server once both token kinds share the
	// same aud=client_id. Like audience binding it is strictly opt-in.

	SECTION("empty expected token_use accepts any token (opt-in default)")
	{
		KJSON jAccess; jAccess["token_use"] = "access";
		KJSON jId;     jId    ["token_use"] = "id";
		KJSON jNone;   jNone  ["sub"]       = "user"; // no token_use claim at all

		CHECK ( KJWT::TokenUseMatches(jAccess, "") );
		CHECK ( KJWT::TokenUseMatches(jId,     "") );
		CHECK ( KJWT::TokenUseMatches(jNone,   "") );
		CHECK ( KJWT::TokenUseMatches(jAccess, KStringView{}) );
	}

	SECTION("a present token_use must match exactly and case-sensitively")
	{
		KJSON jAccess; jAccess["token_use"] = "access";
		KJSON jId;     jId    ["token_use"] = "id";

		CHECK       ( KJWT::TokenUseMatches(jAccess, "access") );
		CHECK_FALSE ( KJWT::TokenUseMatches(jId,     "access") );  // id_token rejected as access token
		CHECK_FALSE ( KJWT::TokenUseMatches(jAccess, "Access") );  // case-sensitive
		CHECK       ( KJWT::TokenUseMatches(jId,     "id") );
	}

	SECTION("a token that omits token_use is accepted (third-party access tokens)")
	{
		// enterprise / third-party access tokens commonly carry no token_use claim;
		// requiring "access" must not break them - only a PRESENT, mismatching value
		// is rejected.
		KJSON jTok; jTok["sub"] = "user";
		CHECK ( KJWT::TokenUseMatches(jTok, "access") );
	}
}

TEST_CASE("KOpenIDKeys")
{
	SECTION("empty default ctor")
	{
		KOpenIDKeys Keys;
		CHECK ( Keys.empty() );
		CHECK ( Keys.size() == 0 );
	}

	SECTION("JWKS with no keys fails validation")
	{
		KJSON jwks = { {"keys", KJSON::array()} };
		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.HasError() );
		CHECK ( Keys.empty() );
	}

	SECTION("ES256: JWK parse + VerifySignature")
	{
		// generate an EC P-256 key pair
		KECKey Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPubRaw = Key.GetPublicKeyRaw();
		REQUIRE ( sPubRaw.size() == 65 );

		// extract x (bytes 1..32) and y (bytes 33..64) from uncompressed point
		auto sX = KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32));
		auto sY = KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32));

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   sX},
				{"y",   sY},
				{"kid", "test-ec"},
				{"alg", "ES256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data
		KStringView sData = "eyJhbGciOiJFUzI1NiJ9.eyJzdWIiOiJ0ZXN0In0";
		KECSign Signer;
		auto sSig = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// positive verification
		CHECK ( Keys.VerifySignature("test-ec", "ES256", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "ES256", "", sData, sSig) );

		// wrong algorithm
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "RS256", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES256", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES256", "", "tampered.data", sSig) );
	}

#if DEKAF2_HAS_ED25519
	SECTION("EdDSA: JWK parse + VerifySignature")
	{
		// generate an Ed25519 key pair
		KEd25519Key Key(true);
		REQUIRE_FALSE ( Key.empty() );

		auto sPubRaw = Key.GetPublicKeyRaw();
		REQUIRE ( sPubRaw.size() == 32 );

		auto sX = KBase64Url::Encode(sPubRaw);

		KJSON jwks = { {"keys", {
			{
				{"kty", "OKP"},
				{"crv", "Ed25519"},
				{"x",   sX},
				{"kid", "test-ed"},
				{"alg", "EdDSA"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data
		KStringView sData = "eyJhbGciOiJFZERTQSJ9.eyJzdWIiOiJ0ZXN0In0";
		KEd25519Sign Signer;
		auto sSig = Signer.Sign(Key, sData);
		REQUIRE ( sSig.size() == 64 );

		// positive verification
		CHECK ( Keys.VerifySignature("test-ed", "EdDSA", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "EdDSA", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-ed", "EdDSA", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-ed", "EdDSA", "", "tampered.data", sSig) );
	}
#endif // DEKAF2_HAS_ED25519

	SECTION("RS256: JWK parse + VerifySignature")
	{
		// use the known 1024-bit RSA test key from krsasign_tests.cpp
		constexpr KStringView sPrivPEM = R"(-----BEGIN PRIVATE KEY-----
MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBALc4tscsMrcwPqCu
n1wrahZhE4suegJntid9AkrCK6bfkIAK9O4AzexALlMQlSjjHZ9j2Elp14rJWK81
OseD+3wPoE0tiVx7pCWhfsCUkFPb0tAteeYOIzpqdpeE5D//sdZh8HN5hfKrWTZd
IA9FSnuEDyH0a7uli61GhT8C66qvAgMBAAECgYB9gdcKtocDH4Q3E4dMXtzr+ZGm
rK6dSSfpAuP4C+xVAh386ASBqIFmzUwuUFSszm7zSTTWjS89/dDHLEJYe1tfpqR/
TdVEUIW9XQ64tm9pycXFiPGnHhJrUYVp9jzc1isuR97u2jgPNA7/3ScBJbB8/A6h
4xSUKbWGriaeq7q6iQJBANszgaL+pfp4Jfr2jafj70lUNlijBoG3Hjswh44IEOR3
bhFhL3cP/Vh4WzQlIDYygspItU/axC4MvZh2xGNUOBMCQQDV+u3MssnzfRC2oSph
n8t9LPOVd4uJMoZI5XRVx0FvEMEVm0PjlSAetJsEoLsVOFuOsf4qTZotxUp0DK8J
zw51AkB3Rm6bD6+nO+uGxNRN7/SL1TwBPSxUNx1HHeAVBASVHPuSj2xxgAzeMBeI
p08Azrlmcuvd+O9ZE2uzY6T3W6NrAkBqxeFvKS+4fgme9+CsAg6KEaoiRRqthTaY
nVZljx3Ji/StEWLY5wq2B6zqrEFuH0cgdxS6iyqJ+E5khge5v0YZAkBJsTVyAJZ/
iwYs1jTW/WRuWYILO9L0Knjhd//IBe1JZJokCcfdeTjTLjYBOXcX7+34t5dIUPXO
EBrURx/EsHSk
-----END PRIVATE KEY-----
)";

		// modulus and exponent from the above key (hex)
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));

		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "test-rsa"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 1 );

		// sign test data with the private key
		KStringView sData = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ0ZXN0In0";
		KRSAKey PrivKey(sPrivPEM);
		REQUIRE_FALSE ( PrivKey.empty() );

		KRSAVerify::Digest Algorithm = KRSASign::SHA256;
		KRSASign Signer(Algorithm, sData);
		auto sSig = Signer.Sign(PrivKey);
		REQUIRE_FALSE ( sSig.empty() );

		// positive verification
		CHECK ( Keys.VerifySignature("test-rsa", "RS256", "", sData, sSig) );

		// wrong kid
		CHECK_FALSE ( Keys.VerifySignature("wrong-kid", "RS256", "", sData, sSig) );

		// tampered signature
		KString sTampered = sSig;
		sTampered[0] ^= 0x01;
		CHECK_FALSE ( Keys.VerifySignature("test-rsa", "RS256", "", sData, sTampered) );

		// tampered data
		CHECK_FALSE ( Keys.VerifySignature("test-rsa", "RS256", "", "tampered.data", sSig) );
	}

	SECTION("GetRSAKey backward compat")
	{
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));

		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "test-rsa"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		REQUIRE ( Keys.size() == 1 );

		// should find the RSA key
		const auto& Key = Keys.GetRSAKey("test-rsa", "RS256", "", "sig");
		CHECK_FALSE ( Key.empty() );

		// wrong kid returns empty
		const auto& Empty = Keys.GetRSAKey("wrong", "RS256", "", "sig");
		CHECK ( Empty.empty() );
	}

	SECTION("mixed key types")
	{
		// EC key
		KECKey ECKeyPair(true);
		auto sPubRaw = ECKeyPair.GetPublicKeyRaw();
		auto sEcX = KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32));
		auto sEcY = KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32));

		// RSA key
		auto sN = KBase64Url::Encode(kUnHex(
			"B738B6C72C32B7303EA0AE9F5C2B6A1661138B2E7A0267B6277D024AC2"
			"2BA6DF90800AF4EE00CDEC402E53109528E31D9F63D84969D78AC958AF35"
			"3AC783FB7C0FA04D2D895C7BA425A17EC0949053DBD2D02D79E60E233A6A"
			"769784E43FFFB1D661F0737985F2AB59365D200F454A7B840F21F46BBBA5"
			"8BAD46853F02EBAAAF"));
		auto sE = KBase64Url::Encode(kUnHex("010001"));

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   sEcX},
				{"y",   sEcY},
				{"kid", "ec-key"},
				{"alg", "ES256"},
				{"use", "sig"}
			},
			{
				{"kty", "RSA"},
				{"n",   sN},
				{"e",   sE},
				{"kid", "rsa-key"},
				{"alg", "RS256"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.size() == 2 );

		// EC signature
		KStringView sData = "header.payload";
		KECSign ECSigner;
		auto sECSig = ECSigner.Sign(ECKeyPair, sData);
		REQUIRE_FALSE ( sECSig.empty() );
		CHECK ( Keys.VerifySignature("ec-key", "ES256", "", sData, sECSig) );

		// EC key should not verify with RSA kid
		CHECK_FALSE ( Keys.VerifySignature("rsa-key", "ES256", "", sData, sECSig) );
	}

	SECTION("unsupported algorithm")
	{
		KECKey ECKeyPair(true);
		auto sPubRaw = ECKeyPair.GetPublicKeyRaw();

		KJSON jwks = { {"keys", {
			{
				{"kty", "EC"},
				{"crv", "P-256"},
				{"x",   KBase64Url::Encode(KStringView(sPubRaw.data() + 1,  32))},
				{"y",   KBase64Url::Encode(KStringView(sPubRaw.data() + 33, 32))},
				{"kid", "test-ec"},
				{"alg", "ES384"},
				{"use", "sig"}
			}
		}} };

		KOpenIDKeys Keys(jwks);
		// key is loaded (kty=EC is accepted), but alg=ES384 won't match ES256 dispatch
		CHECK_FALSE ( Keys.VerifySignature("test-ec", "ES384", "", "data", "sig") );
	}
}

TEST_CASE("KJWT Check")
{
	TestIssuer Issuer("https://sso.example", "key-1");
	TestIssuer Other ("https://other.example", "key-2");

	auto tNow  = std::time(nullptr);
	auto sGood = Issuer.Token("alice", tNow - 10, tNow + 600);

	SECTION("a valid token")
	{
		KOpenIDProviderList Providers;
		Providers.push_back(Issuer.Provider());
		REQUIRE ( Providers.front().IsValid() );

		KJWT JWT;
		CHECK ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.IsValid() );
		CHECK ( JWT.GetUser() == "alice" );
	}

	SECTION("the second provider verifies a token the first one has no key for")
	{
		// the first provider's failure must neither clear the parsed token nor leave
		// its error behind
		KOpenIDProviderList Providers;
		Providers.push_back(Other.Provider());
		Providers.push_back(Issuer.Provider());

		KJWT JWT;
		CHECK ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.IsValid() );
		CHECK ( JWT.GetUser() == "alice" );
		CHECK ( kjson::GetStringRef(JWT.Payload, "iss") == "https://sso.example" );
	}

	SECTION("a reused KJWT keeps no error of an earlier token")
	{
		KOpenIDProviderList Providers;
		Providers.push_back(Issuer.Provider());

		KJWT JWT;
		CHECK_FALSE ( JWT.Check(Other.Token("mallory", tNow - 10, tNow + 600), Providers) );
		CHECK_FALSE ( JWT.IsValid() );

		CHECK ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.IsValid() );
	}

	SECTION("the reasons for a rejection")
	{
		KOpenIDProviderList Providers;
		Providers.push_back(Issuer.Provider());

		KJWT JWT;

		// after the signature check the message names the user
		CHECK_FALSE ( JWT.Check(Issuer.Token("alice", tNow - 1200, tNow - 600), Providers) );
		CHECK ( JWT.Error().contains("sub alice: token has expired") );

		CHECK_FALSE ( JWT.Check(Issuer.Token("alice", tNow - 10, tNow + 600, "key-unknown"), Providers) );
		CHECK ( JWT.Error().contains("bad sig for sub alice: no matching key") );

		// the issuer's token with the signature of another key
		auto sToken   = Issuer.Token("alice", tNow - 10, tNow + 600);
		auto sOther   = Other .Token("alice", tNow - 10, tNow + 600);
		auto iSig     = sToken.rfind('.');
		auto iOther   = sOther.rfind('.');
		REQUIRE ( iSig   != KString::npos );
		REQUIRE ( iOther != KString::npos );
		auto sForged  = kFormat("{}{}", sToken.substr(0, iSig), sOther.substr(iOther));
		CHECK_FALSE ( JWT.Check(sForged, Providers) );
		CHECK ( JWT.Error().contains("signature does not verify") );
	}

	SECTION("no providers, and a provider without keys")
	{
		KOpenIDProviderList Providers;

		KJWT JWT;
		CHECK_FALSE ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.Error().contains("no providers") );

		Providers.push_back(KOpenIDProvider());
		CHECK_FALSE ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.Error().contains("provider has no keys") );

		// the list is searched on
		Providers.push_back(Issuer.Provider());
		CHECK ( JWT.Check(sGood, Providers) );
	}

	SECTION("clock leeway")
	{
		KOpenIDProviderList Providers;
		Providers.push_back(Issuer.Provider());

		// the issuer's clock runs 20 seconds ahead of ours
		auto sEarly = Issuer.Token("alice", tNow + 20, tNow + 600);

		KJWT JWT;
		CHECK_FALSE ( JWT.Check(sEarly, Providers) );
		CHECK ( JWT.Error().contains("token will be valid in") );

		CHECK ( JWT.Check(sEarly, Providers, KStringView{}, KStringView{}, KStringView{}, chrono::seconds(30)) );
	}

	SECTION("verifying leaves a shared key set unchanged")
	{
		KOpenIDKeys Keys(Issuer.JWKS);
		REQUIRE ( Keys.IsValid() );

		KString sError;
		CHECK_FALSE ( Keys.VerifySignature("key-unknown", "ES256", "", "data", "sig", "sig", &sError) );
		CHECK ( sError == "no matching key" );
		CHECK ( Keys.IsValid() );
		CHECK ( Keys.Error().empty() );
	}

	SECTION("an unknown key ID asks the provider at once, but at most once an hour")
	{
		// a provider whose requests fail at once: nothing listens on this port
		KOpenIDProvider Provider(KURL("https://127.0.0.1:1"));
		CHECK_FALSE ( Provider.IsValid() );

		auto tStart = KUnixTime::now();

		// the request of the constructor does not count, the first such token asks at once ..
		CHECK       ( Provider.RefreshForUnknownKey(tStart) );
		// .. and the next ones only an hour later
		CHECK_FALSE ( Provider.RefreshForUnknownKey(tStart + chrono::minutes(1)) );
		CHECK_FALSE ( Provider.RefreshForUnknownKey(tStart + chrono::minutes(59)) );
		CHECK       ( Provider.RefreshForUnknownKey(tStart + chrono::minutes(61)) );
		CHECK_FALSE ( Provider.RefreshForUnknownKey(tStart + chrono::minutes(62)) );

		// the regular requests of Refresh() in between do not hold it back
		Provider.Refresh(tStart + chrono::minutes(100));
		Provider.Refresh(tStart + chrono::minutes(115));
		CHECK       ( Provider.RefreshForUnknownKey(tStart + chrono::minutes(121)) );

		// a provider with fixed keys has nobody to ask
		auto Fixed = Issuer.Provider();
		CHECK_FALSE ( Fixed.RefreshForUnknownKey(tStart + chrono::hours(48)) );

		// and a token with an unknown key is rejected as before
		KOpenIDProviderList Providers;
		Providers.push_back(Issuer.Provider());
		KJWT JWT;
		CHECK_FALSE ( JWT.Check(Issuer.Token("alice", tNow - 10, tNow + 600, "key-new"), Providers) );
		CHECK ( JWT.Error().contains("no matching key") );
	}

	SECTION("a provider without keys asks for them, at most once in three minutes")
	{
		// a provider whose requests fail at once: nothing listens on this port
		KOpenIDProvider Provider(KURL("https://127.0.0.1:1"));
		REQUIRE ( Provider.Get().Keys.empty() );

		auto tStart = KUnixTime::now();

		// the constructor asked just now
		CHECK_FALSE ( Provider.RefreshForMissingKeys(tStart) );
		CHECK_FALSE ( Provider.RefreshForMissingKeys(tStart + chrono::minutes(2)) );
		CHECK       ( Provider.RefreshForMissingKeys(tStart + chrono::minutes(4)) );
		CHECK_FALSE ( Provider.RefreshForMissingKeys(tStart + chrono::minutes(5)) );

		// a request of Refresh() counts as well
		Provider.Refresh(tStart + chrono::minutes(8));
		CHECK_FALSE ( Provider.RefreshForMissingKeys(tStart + chrono::minutes(9)) );
		CHECK       ( Provider.RefreshForMissingKeys(tStart + chrono::minutes(12)) );

		// a provider with fixed keys has nobody to ask
		auto Fixed = Issuer.Provider();
		CHECK_FALSE ( Fixed.RefreshForMissingKeys(tStart + chrono::hours(48)) );

		// a token tries the provider without keys and then the next one
		KOpenIDProviderList Providers;
		Providers.push_back(KOpenIDProvider(KURL("https://127.0.0.1:1")));
		KJWT JWT;
		CHECK_FALSE ( JWT.Check(sGood, Providers) );
		CHECK ( JWT.Error().contains("provider has no keys") );

		Providers.push_back(Issuer.Provider());
		CHECK ( JWT.Check(sGood, Providers) );
	}

	SECTION("key sets compare as their provider published them")
	{
		CHECK ( KOpenIDKeys(Issuer.JWKS) == KOpenIDKeys(Issuer.JWKS) );
		CHECK ( KOpenIDKeys(Issuer.JWKS) != KOpenIDKeys(Other.JWKS) );
		CHECK ( KOpenIDKeys()            != KOpenIDKeys(Issuer.JWKS) );
	}

	SECTION("a provider with fixed keys")
	{
		CHECK_FALSE ( KOpenIDProvider(KOpenIDKeys(), "https://sso.example").IsValid() );

		auto Provider = Issuer.Provider();
		CHECK ( Provider.IsValid() );
		CHECK ( Provider.Get().sIssuer == "https://sso.example" );

		// Refresh() leaves it as it is
		Provider.Refresh(KUnixTime::now() + chrono::hours(48));
		CHECK ( Provider.IsValid() );
		CHECK ( Provider.Get().Keys.size() == 1 );
	}
}
