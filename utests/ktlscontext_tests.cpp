#include "catch.hpp"

#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <memory>
#include <vector>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
std::shared_ptr<KTLSContext> CreateServerContext(KRSAKey& Key, KStringView sDomain)
//-----------------------------------------------------------------------------
{
	KRSACert Cert(Key, sDomain, "US");

	auto Context = std::make_shared<KTLSContext>(true);

	if (!Context->SetTLSCertificates(Cert.GetPEM(), Key.GetPEM(true)))
	{
		return nullptr;
	}

	return Context;

} // CreateServerContext

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

	KString PeerCertCN()
	{
		KString sCN;

		auto* Cert = ::SSL_get_peer_certificate(client);

		if (Cert)
		{
			char szBuffer[256];

			if (::X509_NAME_get_text_by_NID(::X509_get_subject_name(Cert), NID_commonName,
			                                szBuffer, sizeof(szBuffer)) > 0)
			{
				sCN = szBuffer;
			}

			::X509_free(Cert);
		}

		return sCN;
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

TEST_CASE("KTLSContext")
{
	KRSAKey Key(2048);

	auto Default = CreateServerContext(Key, "default.test");
	auto Alpha   = CreateServerContext(Key, "alpha.test");
	auto Wild    = CreateServerContext(Key, "*.wild.test");

	REQUIRE ( Default != nullptr );
	REQUIRE ( Alpha   != nullptr );
	REQUIRE ( Wild    != nullptr );

	CHECK ( Default->AddSNIContext("alpha.test" , Alpha) == true );
	CHECK ( Default->AddSNIContext("*.wild.test", Wild ) == true );

	KTLSContext ClientCtx(false);

	SECTION("SNILookup")
	{
		CHECK ( Default->GetSNIContext("alpha.test")    == Alpha   );
		CHECK ( Default->GetSNIContext("ALPHA.Test.")   == Alpha   );
		CHECK ( Default->GetSNIContext("www.wild.test") == Wild    );
		CHECK ( Default->GetSNIContext("a.b.wild.test") == nullptr );
		CHECK ( Default->GetSNIContext("wild.test")     == nullptr );
		CHECK ( Default->GetSNIContext("unknown.test")  == nullptr );

		CHECK ( Default->AddSNIContext("x.test", nullptr) == false );
		CHECK ( Default->AddSNIContext(""      , Alpha  ) == false );
		CHECK ( Default->AddSNIContext("x.test", std::make_shared<KTLSContext>(false)) == false );
		CHECK ( ClientCtx.AddSNIContext("x.test", Alpha) == false );

		CHECK ( Default->RemoveSNIContext("ALPHA.test") == true    );
		CHECK ( Default->RemoveSNIContext("alpha.test") == false   );
		CHECK ( Default->GetSNIContext("alpha.test")    == nullptr );
	}

	SECTION("SNIDispatch")
	{
		{
			// no SNI sent - the default context serves
			TLSPair Pair(*Default, ClientCtx);
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "default.test" );
		}

		{
			TLSPair Pair(*Default, ClientCtx);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "alpha.test") == 1 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "alpha.test" );
			CHECK   ( KStringView(::SSL_get_servername(Pair.server, TLSEXT_NAMETYPE_host_name)) == "alpha.test" );
		}

		{
			TLSPair Pair(*Default, ClientCtx);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "www.wild.test") == 1 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "*.wild.test" );
		}

		{
			// unknown SNI falls back to the default context
			TLSPair Pair(*Default, ClientCtx);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "unknown.test") == 1 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "default.test" );
		}
	}

	SECTION("ServerALPN")
	{
		CHECK ( Default->SetALPN(std::vector<KStringView>{ "h2", "http/1.1" }) == true );

		{
			// server preference order must pick h2, not the client's first choice
			TLSPair Pair(*Default, ClientCtx);
			static const unsigned char sProtos[] = "\x08" "http/1.1" "\x02" "h2";
			CHECK   ( ::SSL_set_alpn_protos(Pair.client, sProtos, sizeof(sProtos) - 1) == 0 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.SelectedALPN() == "h2" );
		}

		{
			// no overlap is answered with a fatal alert
			TLSPair Pair(*Default, ClientCtx);
			static const unsigned char sProtos[] = "\x05" "bogus";
			CHECK ( ::SSL_set_alpn_protos(Pair.client, sProtos, sizeof(sProtos) - 1) == 0 );
			CHECK ( Pair.Handshake() == false );
		}

		{
			// a client without ALPN connects without it
			TLSPair Pair(*Default, ClientCtx);
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.SelectedALPN() == "" );
		}
	}

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
	SECTION("SNICallback")
	{
		// the ACME tls-alpn-01 pattern: the callback selects a challenge context
		// by offered ALPN, overriding the hostname map
		auto Acme = CreateServerContext(Key, "acme.test");
		REQUIRE ( Acme != nullptr );
		CHECK   ( Acme->SetALPN(std::vector<KStringView>{ "acme-tls/1" }) == true );

		CHECK ( Default->SetSNICallback([&Acme](const KTLSContext::ClientHello& Hello) -> std::shared_ptr<KTLSContext>
		{
			if (Hello.HasALPN("acme-tls/1"))
			{
				return Acme;
			}
			return nullptr;
		}) == true );

		{
			TLSPair Pair(*Default, ClientCtx);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "alpha.test") == 1 );
			static const unsigned char sProtos[] = "\x0a" "acme-tls/1";
			CHECK   ( ::SSL_set_alpn_protos(Pair.client, sProtos, sizeof(sProtos) - 1) == 0 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "acme.test" );
			CHECK   ( Pair.SelectedALPN() == "acme-tls/1" );
		}

		{
			// without the ALPN the callback declines, and the hostname map serves
			TLSPair Pair(*Default, ClientCtx);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "alpha.test") == 1 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "alpha.test" );
		}
	}
#endif
}
