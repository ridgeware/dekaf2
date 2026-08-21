#include "catch.hpp"

#include <dekaf2/web/acme/kacmeclient.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <openssl/objects.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
// ASN1_STRING_get0_data() only exists from OpenSSL 1.1.0 on
KStringView Asn1View(const ASN1_STRING* String)
//-----------------------------------------------------------------------------
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	return { reinterpret_cast<const char*>(::ASN1_STRING_get0_data(String)),
	         static_cast<std::size_t>(::ASN1_STRING_length(String)) };
#else
	return { reinterpret_cast<const char*>(String->data),
	         static_cast<std::size_t>(String->length) };
#endif
}

//-----------------------------------------------------------------------------
// a client/server SSL pair, connected through an in-memory BIO pair
struct TLSPair
//-----------------------------------------------------------------------------
{
	TLSPair(KTLSContext& Server, KTLSContext& Client)
	{
		server = ::SSL_new(Server.GetContext().native_handle());
		client = ::SSL_new(Client.GetContext().native_handle());

		::BIO* ClientBio;
		::BIO* ServerBio;
		::BIO_new_bio_pair(&ClientBio, 0, &ServerBio, 0);

		::SSL_set_bio(client, ClientBio, ClientBio);
		::SSL_set_bio(server, ServerBio, ServerBio);
		::SSL_set_connect_state(client);
		::SSL_set_accept_state(server);
	}

	~TLSPair()
	{
		::SSL_free(client);
		::SSL_free(server);
	}

	static bool WantsMoreData(::SSL* ssl, int iResult)
	{
		auto iError = ::SSL_get_error(ssl, iResult);
		return iError == SSL_ERROR_WANT_READ || iError == SSL_ERROR_WANT_WRITE;
	}

	bool Handshake()
	{
		for (int i = 0; i < 20; ++i)
		{
			auto c = ::SSL_do_handshake(client);
			auto s = ::SSL_do_handshake(server);

			if (c == 1 && s == 1)
			{
				return true;
			}

			if ((c != 1 && !WantsMoreData(client, c)) ||
				(s != 1 && !WantsMoreData(server, s)))
			{
				return false;
			}
		}

		return false;
	}

	KStringView SelectedALPN()
	{
		const unsigned char* pProto;
		unsigned int iProto;
		::SSL_get0_alpn_selected(client, &pProto, &iProto);
		return { reinterpret_cast<const char*>(pProto), iProto };
	}

	::SSL* client;
	::SSL* server;

}; // TLSPair

} // end of anonymous namespace

TEST_CASE("KAcmeClient")
{
	SECTION("JWKThumbprint")
	{
		// the test vector from RFC 7638 section 3.1
		KRSAKey Key(KJSON {
			{ "n", "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw" },
			{ "e", "AQAB" }
		});
		REQUIRE ( Key.empty() == false );

		CHECK ( KAcmeClient::JWKThumbprint(Key) == "NzbLsXh8uDCcd-6MNwXF4W_7noWXFZAfHkxZsRGC9Xs" );
	}

	SECTION("SignJWS")
	{
		KRSAKey Key(2048);
		REQUIRE ( Key.empty() == false );

		auto sJWS = KAcmeClient::SignJWS(Key, KJSON {
			{ "nonce", "abc123" },
			{ "url"  , "https://example.com/acme/new-order" },
			{ "kid"  , "https://example.com/acme/acct/1"    }
		}, R"({"foo":"bar"})");

		KJSON jJWS;
		KString sError;
		REQUIRE ( kjson::Parse(jJWS, sJWS, sError) == true );

		// the protected header carries the input plus the algorithm
		KJSON jProtected;
		REQUIRE ( kjson::Parse(jProtected, KBase64Url::Decode(jJWS["protected"].String()), sError) == true );
		CHECK   ( jProtected["alg"]   == "RS256"  );
		CHECK   ( jProtected["nonce"] == "abc123" );

		CHECK ( KBase64Url::Decode(jJWS["payload"].String()) == R"({"foo":"bar"})" );

		// the signature verifies over protected.payload
		KRSAVerify Verifier(KRSAVerify::Digest::SHA256);
		Verifier.Update(jJWS["protected"].String());
		Verifier.Update(".");
		Verifier.Update(jJWS["payload"].String());
		CHECK ( Verifier.Verify(Key, KBase64Url::Decode(jJWS["signature"].String())) == true );

		// an empty payload (POST-as-GET) encodes as the empty string
		auto sJWS2 = KAcmeClient::SignJWS(Key, KJSON { { "nonce", "x" } }, "");
		KJSON jJWS2;
		REQUIRE ( kjson::Parse(jJWS2, sJWS2, sError) == true );
		CHECK   ( jJWS2["payload"] == "" );
	}

	SECTION("ChallengeContext")
	{
		// validate the tls-alpn-01 challenge cert like an ACME server does
		// (RFC 8737 section 3): handshake with SNI and ALPN acme-tls/1, then check
		// SAN and acmeIdentifier extension of the presented certificate

		constexpr KStringViewZ sDomain  = "alpha.test";
		constexpr KStringViewZ sKeyAuth = "sometoken.somethumbprint";

		auto Challenge = KAcmeClient::CreateChallengeContext(sDomain, sKeyAuth, 2048);
		REQUIRE ( Challenge != nullptr );

		KTLSContext ClientCtx(false);
		TLSPair Pair(*Challenge, ClientCtx);

		CHECK ( ::SSL_set_tlsext_host_name(Pair.client, sDomain.c_str()) == 1 );
		static const unsigned char sProtos[] = "\x0a" "acme-tls/1";
		CHECK ( ::SSL_set_alpn_protos(Pair.client, sProtos, sizeof(sProtos) - 1) == 0 );

		REQUIRE ( Pair.Handshake()    == true         );
		CHECK   ( Pair.SelectedALPN() == "acme-tls/1" );

		auto* Cert = ::SSL_get_peer_certificate(Pair.client);
		REQUIRE ( Cert != nullptr );

		// the domain is the single SAN dNSName
		{
			auto* SANs = static_cast<GENERAL_NAMES*>(::X509_get_ext_d2i(Cert, NID_subject_alt_name, nullptr, nullptr));
			REQUIRE ( SANs != nullptr );
			REQUIRE ( sk_GENERAL_NAME_num(SANs) == 1 );

			auto* Name = sk_GENERAL_NAME_value(SANs, 0);
			REQUIRE ( Name->type == GEN_DNS );
			CHECK   ( Asn1View(Name->d.dNSName) == sDomain );

			sk_GENERAL_NAME_pop_free(SANs, GENERAL_NAME_free);
		}

		// the critical acmeIdentifier extension carries SHA-256 of the key authorization
		{
			auto* Obj = ::OBJ_txt2obj("1.3.6.1.5.5.7.1.31", 1);
			REQUIRE ( Obj != nullptr );

			auto iPos = ::X509_get_ext_by_OBJ(Cert, Obj, -1);
			::ASN1_OBJECT_free(Obj);
			REQUIRE ( iPos >= 0 );

			auto* Ext = ::X509_get_ext(Cert, iPos);
			REQUIRE ( Ext != nullptr );
			CHECK   ( ::X509_EXTENSION_get_critical(Ext) == 1 );

			auto* Data = ::X509_EXTENSION_get_data(Ext);
			REQUIRE ( Data != nullptr );

			KString sExpected("\x04\x20", 2);
			sExpected += KSHA256(sKeyAuth).Digest();

			CHECK ( Asn1View(Data) == sExpected );
		}

		::X509_free(Cert);
	}

	SECTION("ChallengeResolver")
	{
		// the resolver only reacts to acme-tls/1, and survives its client
		KTLSContext::SNICallback Resolver;

		{
			KAcmeClient Acme;
			Resolver = Acme.GetChallengeResolver();
		}

		KTLSContext::ClientHello Hello;
		Hello.sServerName = "www.example.com";
		CHECK ( Resolver(Hello) == nullptr );

		Hello.ALPNs.push_back("acme-tls/1");
		CHECK ( Resolver(Hello) == nullptr ); // no challenge pending for the name
	}

	SECTION("ECAccountKeys")
	{
		KECKey Key(true);
		REQUIRE ( Key.empty() == false );

		// the JWK carries the P-256 point coordinates
		auto jJWK = Key.GetPublicJWK("mykid");
		CHECK ( jJWK["kty"] == "EC"    );
		CHECK ( jJWK["crv"] == "P-256" );
		CHECK ( jJWK["alg"] == "ES256" );
		CHECK ( jJWK["x"].String().size() == 43 ); // 32 bytes base64url
		CHECK ( jJWK["y"].String().size() == 43 );

		// the thumbprint is stable and SHA-256 sized
		auto sThumb = KAcmeClient::JWKThumbprint(Key);
		CHECK ( sThumb.size() == 43 );
		CHECK ( sThumb == KAcmeClient::JWKThumbprint(Key) );

		// an ES256 JWS verifies over protected.payload
		auto sJWS = KAcmeClient::SignJWS(Key, KJSON { { "nonce", "abc" } }, R"({"foo":"bar"})");

		KJSON jJWS;
		KString sError;
		REQUIRE ( kjson::Parse(jJWS, sJWS, sError) == true );

		KJSON jProtected;
		REQUIRE ( kjson::Parse(jProtected, KBase64Url::Decode(jJWS["protected"].String()), sError) == true );
		CHECK   ( jProtected["alg"] == "ES256" );

		auto sSignature = KBase64Url::Decode(jJWS["signature"].String());
		CHECK ( sSignature.size() == 64 ); // raw r||s

		KECVerify Verifier;
		CHECK ( Verifier.Verify(Key,
		                        kFormat("{}.{}", jJWS["protected"].String(), jJWS["payload"].String()),
		                        sSignature) == true );
	}

	SECTION("ARICertID")
	{
		KRSAKey  Key(2048);
		KRSACert Cert(Key, "ari.test", "", "", "", chrono::days(7));
		REQUIRE ( Cert.HasError() == false );

		// a cert without an Authority Key Identifier has no ARI ID
		CHECK ( KAcmeClient::CreateARICertID(Cert.GetPEM()) == "" );

		// add a known AKI: SEQUENCE { [0] keyIdentifier (20 bytes) }
		KString sKeyID;
		for (int i = 0; i < 20; ++i)
		{
			sKeyID += static_cast<char>(i);
		}

		KString sAKI("\x30\x16\x80\x14", 4);
		sAKI += sKeyID;

		REQUIRE ( Cert.AddExtension("2.5.29.35", sAKI) == true );
		REQUIRE ( Cert.Sign(Key) == true );

		auto sPEM = Cert.GetPEM();

		KRSACert Parsed(sPEM);
		CHECK ( Parsed.GetAuthorityKeyIdentifier() == sKeyID );
		CHECK ( Parsed.GetSerialBytes().empty()    == false  );

		CHECK ( KAcmeClient::CreateARICertID(sPEM) ==
		        kFormat("{}.{}", KBase64Url::Encode(sKeyID), KBase64Url::Encode(Parsed.GetSerialBytes())) );
	}

	SECTION("RFC3339")
	{
		// ARI renewal windows come as RFC 3339 timestamps
		KUnixTime Parsed = kParseTimestamp("2026-08-21T12:00:00Z");
		CHECK ( Parsed.to_time_t() == 1787313600 );
	}

	SECTION("Errors")
	{
		KAcmeClient Acme;

		CHECK ( Acme.OrderCertificate({}).IsValid()                    == false );
		CHECK ( Acme.HasError()                                        == true  );
		CHECK ( Acme.OrderCertificate({ "*.example.com" }).IsValid()   == false );
		CHECK ( Acme.Error().contains("wildcard")                      == true  );

		// ARI failures are advisory and set no error
		CHECK ( Acme.GetRenewalInfo("").IsValid()                      == false );
	}
}
