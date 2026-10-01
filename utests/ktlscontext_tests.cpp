#include "catch.hpp"

#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <memory>
#include <vector>

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
// the first entry of a name with the given NID - X509_NAME_get_text_by_NID()
// is deprecated with OpenSSL 4
KStringView NameEntry(const X509_NAME* Name, int iNID)
//-----------------------------------------------------------------------------
{
	for (int i = 0; i < ::X509_NAME_entry_count(Name); ++i)
	{
		auto* Entry = ::X509_NAME_get_entry(Name, i);

		if (::OBJ_obj2nid(::X509_NAME_ENTRY_get_object(Entry)) == iNID)
		{
			return Asn1View(::X509_NAME_ENTRY_get_data(Entry));
		}
	}

	return {};
}

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
			// copy the name before the cert is freed
			sCN = NameEntry(::X509_get_subject_name(Cert), NID_commonName);

			::X509_free(Cert);
		}

		return sCN;
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	/// the NID of the negotiated key exchange group
	int NegotiatedGroup()
	{
		return static_cast<int>(SSL_get_negotiated_group(client));
	}
#endif

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

	SECTION("TrustedCertificates")
	{
		KRSACert PinnedCert(Key, "pinned.test", "US");
		KRSACert OtherCert (Key, "other.test" , "US");
		auto sPinnedPEM = PinnedCert.GetPEM();
		auto sOtherPEM  = OtherCert.GetPEM();

		auto Pinned = std::make_shared<KTLSContext>(true);
		REQUIRE ( Pinned->SetTLSCertificates(sPinnedPEM, Key.GetPEM(true)) == true );

		// handshake with peer verification switched on
		auto Verified = [&Pinned](KTLSContext& Client) -> bool
		{
			TLSPair Pair(*Pinned, Client);
			::SSL_set_verify(Pair.client, SSL_VERIFY_PEER, nullptr);
			return Pair.Handshake() && ::SSL_get_verify_result(Pair.client) == X509_V_OK;
		};

		{
			// the system roots do not know the self-signed certificate
			KTLSContext Client(false);
			CHECK ( Verified(Client) == false );
		}

		{
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(sPinnedPEM) == true );
			CHECK ( Verified(Client) == true );
		}

		{
			// another certificate does not vouch for the server
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(sOtherPEM) == true );
			CHECK ( Verified(Client) == false );
		}

		{
			// several certificates, other PEM blocks in between are skipped
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(kFormat("{}{}{}", sOtherPEM, Key.GetPEM(true), sPinnedPEM)) == true );
			CHECK ( Verified(Client) == true );
		}

		{
			// a failed replacement keeps the previous certificates
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(sPinnedPEM) == true );
			CHECK ( Client.SetTLSVerifyCertificates("no certificate here") == false );
			CHECK ( Client.HasError() == true );
			CHECK ( Client.SetTLSVerifyCertificates(KStringView(sOtherPEM).Left(sOtherPEM.size() / 2)) == false );
			CHECK ( ::ERR_peek_error() == 0 );
			CHECK ( Verified(Client) == true );
		}

		{
			KTempDir TempDir;
			auto sFile = kFormat("{}/pinned.pem", TempDir.Name());
			REQUIRE ( kWriteFile(sFile, sPinnedPEM) == true );

			KTLSContext Client(false);
			CHECK ( Client.LoadTLSVerifyCertificates(sFile) == true );
			CHECK ( Verified(Client) == true );
			CHECK ( Client.LoadTLSVerifyCertificates(kFormat("{}/missing.pem", TempDir.Name())) == false );
			CHECK ( Verified(Client) == true );
		}
	}

	SECTION("TrustedCertificateChain")
	{
		// a CA, and a server certificate issued by it
		KRSACert CACert(Key, "ca.test", "US");
		// basicConstraints: CA:TRUE
		REQUIRE ( CACert.AddExtension("2.5.29.19", KStringView("\x30\x03\x01\x01\xff", 5), true) == true );
		REQUIRE ( CACert.Sign(Key) == true );

		KRSAKey  LeafKey(2048);
		KRSACert LeafCert(LeafKey, "leaf.test", "US");
		REQUIRE ( ::X509_set_issuer_name(LeafCert.GetCert(), ::X509_get_subject_name(CACert.GetCert())) == 1 );
		REQUIRE ( LeafCert.Sign(Key) == true );

		auto sCAPEM   = CACert.GetPEM();
		auto sLeafPEM = LeafCert.GetPEM();

		auto Server = std::make_shared<KTLSContext>(true);
		REQUIRE ( Server->SetTLSCertificates(kFormat("{}{}", sLeafPEM, sCAPEM), LeafKey.GetPEM(true)) == true );

		auto Verified = [&Server](KTLSContext& Client) -> bool
		{
			TLSPair Pair(*Server, Client);
			::SSL_set_verify(Pair.client, SSL_VERIFY_PEER, nullptr);
			return Pair.Handshake() && ::SSL_get_verify_result(Pair.client) == X509_V_OK;
		};

		{
			// trusting the CA
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(sCAPEM) == true );
			CHECK ( Verified(Client) == true );
		}

		{
			// pinning the server certificate alone, it is not self-signed
			KTLSContext Client(false);
			CHECK ( Client.SetTLSVerifyCertificates(sLeafPEM) == true );
			CHECK ( Verified(Client) == true );
		}
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	SECTION("Groups")
	{
		CHECK ( Default->SetGroups("X25519:X448") == true );

		{
			KTLSContext Client(false);
			CHECK   ( Client.SetGroups("X448") == true );
			TLSPair Pair(*Default, Client);
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.NegotiatedGroup() == NID_X448 );
		}

		{
			// commas and white space are accepted as separators
			KTLSContext Client(false);
			CHECK   ( Client.SetGroups(" X25519, X448 ") == true );
			TLSPair Pair(*Default, Client);
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.NegotiatedGroup() == NID_X25519 );
		}

		{
			// no common group - the server rejects the client
			KTLSContext Client(false);
			CHECK ( Client.SetGroups("secp384r1") == true );
			TLSPair Pair(*Default, Client);
			CHECK ( Pair.Handshake() == false );
		}

		{
			// an unknown group is an error, an empty list keeps the setting
			KTLSContext Client(false);
			CHECK ( Client.SetGroups("X25519:NoSuchGroup") == false );
			CHECK ( Client.HasError() == true );
			// the TLS library error queue is drained into the message
			CHECK ( ::ERR_peek_error() == 0 );
			CHECK ( Client.SetGroups("") == true );
		}
	}

	SECTION("DefaultGroups")
	{
		CHECK ( KTLSContext::GetDefaultGroups() == "" );
		CHECK ( KTLSContext::SetDefaultGroups("NoSuchGroup") == false );
		CHECK ( KTLSContext::GetDefaultGroups() == "" );
		CHECK ( KTLSContext::SetDefaultGroups("X448, X25519") == true );
		CHECK ( KTLSContext::GetDefaultGroups() == "X448:X25519" );

		KTLSContext Client(false);

		// restore before anything can bail out of this section
		CHECK ( KTLSContext::SetDefaultGroups("") == true );
		CHECK ( KTLSContext::GetDefaultGroups() == "" );

		CHECK   ( Default->SetGroups("X25519:X448") == true );
		TLSPair Pair(*Default, Client);
		REQUIRE ( Pair.Handshake() == true );
		CHECK   ( Pair.NegotiatedGroup() == NID_X448 );
	}

	SECTION("GroupsWithSNI")
	{
		// the groups of the default context govern the handshake, even when SNI
		// dispatch serves the connection with another context
		CHECK ( Default->SetGroups("X448") == true );
		CHECK ( Alpha->SetGroups("X25519") == true );

		{
			KTLSContext Client(false);
			CHECK ( Client.SetGroups("X25519") == true );
			TLSPair Pair(*Default, Client);
			CHECK ( ::SSL_set_tlsext_host_name(Pair.client, "alpha.test") == 1 );
			CHECK ( Pair.Handshake() == false );
		}

		{
			KTLSContext Client(false);
			CHECK   ( Client.SetGroups("X448") == true );
			TLSPair Pair(*Default, Client);
			CHECK   ( ::SSL_set_tlsext_host_name(Pair.client, "alpha.test") == 1 );
			REQUIRE ( Pair.Handshake() == true );
			CHECK   ( Pair.PeerCertCN() == "alpha.test" );
			CHECK   ( Pair.NegotiatedGroup() == NID_X448 );
		}
	}
#endif

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
