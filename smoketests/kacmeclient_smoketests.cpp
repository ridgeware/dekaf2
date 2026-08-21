#include "catch.hpp"

#include <dekaf2/web/acme/kacmeclient.h>
#include <dekaf2/web/acme/kacmemanager.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

using namespace dekaf2;

// Runs the full ACME flows against a local Pebble instance:
//
//   podman run --rm -d --name pebble -p 14000:14000 -e PEBBLE_VA_NOSLEEP=1 ghcr.io/letsencrypt/pebble
//
// Pebble validates tls-alpn-01 by connecting to port 5001 of the ordered domain.
// The default domain "host.containers.internal" resolves to this host from inside
// the container, so no DNS mock is needed. The tests skip themselves when no ACME
// server responds at the directory URL.
//
// Environment overrides (e.g. for a staging acceptance run on a public server):
//
//   DEKAF2_ACME_DIR    - directory URL      (default https://localhost:14000/dir)
//   DEKAF2_ACME_DOMAIN - domain to order    (default host.containers.internal)
//   DEKAF2_ACME_PORT   - local server port  (default 5001)
//   DEKAF2_ACME_VERIFY - 1 = verify the directory's CA (needed for staging)

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
	#define DEKAF2_SMOKETEST_HAS_TLS_ALPN 1
#else
	#define DEKAF2_SMOKETEST_HAS_TLS_ALPN 0
#endif

namespace {

struct AcmeEnv
{
	AcmeEnv()
	{
		sDirectory = kGetEnv("DEKAF2_ACME_DIR"   , "https://localhost:14000/dir");
		sDomain    = kGetEnv("DEKAF2_ACME_DOMAIN", "host.containers.internal");
		bVerify    = kGetEnv("DEKAF2_ACME_VERIFY", "0") == "1";
		iPort      = KString(kGetEnv("DEKAF2_ACME_PORT", "5001")).UInt16();
		iHttpPort  = KString(kGetEnv("DEKAF2_ACME_HTTP_PORT", "5002")).UInt16();
	}

	/// returns true when an ACME server responds at the directory URL
	bool HaveServer() const
	{
		KWebClient Check;
		Check.SetVerifyCerts(false);
		Check.Get(KURL(sDirectory));
		return Check.GetStatusCode() == 200;
	}

	KString  sDirectory;
	KString  sDomain;
	bool     bVerify;
	uint16_t iPort;
	uint16_t iHttpPort;

}; // AcmeEnv

struct PeerInfo
{
	/// true if the server presented a CA issued cert (and not a self-signed one)
	bool IsIssued() const { return bOK && sSubject != sIssuer; }

	KString sSubject;
	KString sIssuer;
	KString sSerial;
	bool    bOK { false };

}; // PeerInfo

//-----------------------------------------------------------------------------
// connect to the local server with the given SNI name and return subject,
// issuer and serial of the certificate it presents
PeerInfo GetPeerInfo(uint16_t iPort, KStringView sSNI)
//-----------------------------------------------------------------------------
{
	PeerInfo Info;

	KTLSClient Client;
	Client.SetManualTLSHandshake(true);

	if (!Client.Connect(kFormat("localhost:{}", iPort), KStreamOptions::None))
	{
		return Info;
	}

	if (!sSNI.empty())
	{
		::SSL_set_tlsext_host_name(Client.GetNativeTLSHandle(), KString(sSNI).c_str());
	}

	if (!Client.StartManualTLSHandshake())
	{
		return Info;
	}

	auto* Peer = ::SSL_get_peer_certificate(Client.GetNativeTLSHandle());

	if (!Peer)
	{
		return Info;
	}

	char szBuffer[256] = "";
	::X509_NAME_oneline(::X509_get_subject_name(Peer), szBuffer, sizeof(szBuffer));
	Info.sSubject = szBuffer;

	szBuffer[0] = 0;
	::X509_NAME_oneline(::X509_get_issuer_name(Peer), szBuffer, sizeof(szBuffer));
	Info.sIssuer = szBuffer;

	auto* Serial = ::X509_get_serialNumber(Peer);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	Info.sSerial.assign(reinterpret_cast<const char*>(::ASN1_STRING_get0_data(Serial)),
	                    static_cast<std::size_t>(::ASN1_STRING_length(Serial)));
#else
	Info.sSerial.assign(reinterpret_cast<const char*>(Serial->data),
	                    static_cast<std::size_t>(Serial->length));
#endif

	::X509_free(Peer);

	Info.bOK = true;

	return Info;

} // GetPeerInfo

} // end of anonymous namespace

TEST_CASE("KAcmeClient_ACME")
{
#if !DEKAF2_SMOKETEST_HAS_TLS_ALPN

	WARN("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side - skipping");

#else

	AcmeEnv Env;

	if (!Env.HaveServer())
	{
		WARN("no ACME server at " << Env.sDirectory << " - skipping (start Pebble, see comment in this file)");
		return;
	}

	// the server under test - starts with an ephemeral self-signed cert
	KTCPServer Server(Env.iPort, /*bTLS*/true, /*iMaxThreads*/10, /*bStoreNewCerts*/false);
	REQUIRE ( Server.Start(chrono::seconds(15), /*bBlock*/false) == true );

	auto* TLSContext = Server.GetTLSContext();
	REQUIRE ( TLSContext != nullptr );

	KAcmeClient::Options Options;
	Options.sDirectoryURL = Env.sDirectory;
	Options.bVerifyTLS    = Env.bVerify;

	KAcmeClient Acme(std::move(Options));
	REQUIRE ( Acme.AttachTo(*TLSContext) == true );

	auto Cert = Acme.OrderCertificate({ Env.sDomain });

	INFO    ( Acme.Error() );
	REQUIRE ( Acme.HasError() == false );
	REQUIRE ( Cert.IsValid()  == true  );
	CHECK   ( Cert.sAccountKeyPEM.empty() == false );

	// the leaf is currently valid
	KRSACert Leaf(Cert.sCertPEM);
	CHECK ( Leaf.empty()      == false );
	CHECK ( Leaf.IsValidNow() == true  );

	// renewal path 1: publish the issued cert through SNI dispatch - this switches
	// per handshake and is therefore effective for the very next connection
	auto NewContext = std::make_shared<KTLSContext>(true);
	REQUIRE ( NewContext->SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM) == true );
	REQUIRE ( TLSContext->AddSNIContext(Env.sDomain, NewContext) == true );

	CHECK ( GetPeerInfo(Env.iPort, Env.sDomain).IsIssued() == true  );
	CHECK ( GetPeerInfo(Env.iPort, "").IsIssued()          == false ); // no SNI still gets the bootstrap cert

	// renewal path 2: swap the default context's cert - the accept loop keeps one
	// pre-allocated TLS stream with the old cert, so allow a few connections
	REQUIRE ( TLSContext->RemoveSNIContext(Env.sDomain) == true );
	REQUIRE ( TLSContext->SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM) == true );

	bool bSwapped = false;

	for (int i = 0; !bSwapped && i < 3; ++i)
	{
		bSwapped = GetPeerInfo(Env.iPort, "").IsIssued();
	}

	CHECK ( bSwapped == true );

	Server.Stop();

#endif
}

TEST_CASE("KAcmeManager_ACME")
{
#if !DEKAF2_SMOKETEST_HAS_TLS_ALPN

	WARN("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side - skipping");

#else

	AcmeEnv Env;

	if (!Env.HaveServer())
	{
		WARN("no ACME server at " << Env.sDirectory << " - skipping (start Pebble, see comment in this file)");
		return;
	}

	KTempDir Tmp;

	KTCPServer Server(Env.iPort, true, 10, false);
	REQUIRE ( Server.Start(chrono::seconds(15), false) == true );

	KAcmeManager::Options Options;
	Options.Domains            = { Env.sDomain };
	Options.sStorageDir        = Tmp.Name();
	Options.Acme.sDirectoryURL = Env.sDirectory;
	Options.Acme.bVerifyTLS    = Env.bVerify;
	Options.RenewBefore        = chrono::years(10);  // force a renewal on every check
	Options.CheckInterval      = chrono::seconds(3);

	{
		KAcmeManager Manager(Options);

		// blocking start: returns with the first certificate installed
		REQUIRE ( Manager.Start(*Server.GetTLSContext(), /*bBlockUntilIssued*/true) == true );
		CHECK   ( Manager.ValidUntil() > KUnixTime::now() );

		auto First = GetPeerInfo(Env.iPort, Env.sDomain);
		CHECK ( First.IsIssued() == true );

		// certificate, key and account key are persisted
		CHECK ( kNonEmptyFileExists(kFormat("{}{}acme-cert.pem"   , Tmp.Name(), kDirSep)) );
		CHECK ( kNonEmptyFileExists(kFormat("{}{}acme-privkey.pem", Tmp.Name(), kDirSep)) );
		CHECK ( kNonEmptyFileExists(kFormat("{}{}acme-account.pem", Tmp.Name(), kDirSep)) );

		// ARI: the CA knows the issued certificate and suggests a renewal window
		{
			auto sCertID = KAcmeClient::CreateARICertID(kReadAll(kFormat("{}{}acme-cert.pem", Tmp.Name(), kDirSep)));
			CHECK ( sCertID.empty() == false );

			KAcmeClient::Options AriOptions;
			AriOptions.sDirectoryURL = Env.sDirectory;
			AriOptions.bVerifyTLS    = Env.bVerify;

			KAcmeClient Ari(std::move(AriOptions));
			auto Window = Ari.GetRenewalInfo(sCertID);

			CHECK ( Window.IsValid() == true );
			CHECK ( Window.WindowEnd > Window.WindowStart );
		}

		// the timer renews (RenewBefore is huge) - the served serial must change
		bool bRenewed = false;

		for (int i = 0; !bRenewed && i < 30; ++i)
		{
			kSleep(chrono::seconds(1));
			auto Next = GetPeerInfo(Env.iPort, Env.sDomain);
			bRenewed  = Next.bOK && !Next.sSerial.empty() && Next.sSerial != First.sSerial;
		}

		CHECK ( bRenewed == true );

		Manager.Stop();
	}

	{
		// a second manager installs the stored certificate without any order -
		// the unreachable directory URL proves that no CA is contacted. Pebble
		// picks a random profile per order (6 days or 90 days validity), so the
		// renewal threshold must lie below the shortest one
		Options.Acme.sDirectoryURL = "https://localhost:1/dir";
		Options.RenewBefore        = chrono::hours(1);
		Options.Acme.sAccountKeyPEM.clear();

		KAcmeManager Second(Options);

		auto bSecond = Second.Start(*Server.GetTLSContext(), true);
		INFO  ( Second.Error() );
		CHECK ( bSecond == true );
		CHECK ( Second.ValidUntil() > KUnixTime::now() );

		Second.Stop();
	}

	Server.Stop();

#endif
}

TEST_CASE("KAcmeClient_EC")
{
#if !DEKAF2_SMOKETEST_HAS_TLS_ALPN

	WARN("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side - skipping");

#else

	AcmeEnv Env;

	if (!Env.HaveServer())
	{
		WARN("no ACME server at " << Env.sDirectory << " - skipping (start Pebble, see comment in this file)");
		return;
	}

	KTCPServer Server(Env.iPort, true, 10, false);
	REQUIRE ( Server.Start(chrono::seconds(15), false) == true );

	// order with P-256 account and certificate keys (ES256 JWS)
	KAcmeClient::Options Options;
	Options.sDirectoryURL = Env.sDirectory;
	Options.bVerifyTLS    = Env.bVerify;
	Options.Keys          = KAcmeClient::KeyType::EC;

	KAcmeClient Acme(std::move(Options));
	REQUIRE ( Acme.AttachTo(*Server.GetTLSContext()) == true );

	auto Cert = Acme.OrderCertificate({ Env.sDomain });

	INFO    ( Acme.Error() );
	REQUIRE ( Acme.HasError() == false );
	REQUIRE ( Cert.IsValid()  == true  );

	// the issued cert carries an EC public key
	KRSACert Leaf(Cert.sCertPEM);
	REQUIRE ( Leaf.empty() == false );

	auto* PubKey = ::X509_get_pubkey(Leaf.GetCert());
	REQUIRE ( PubKey != nullptr );
	CHECK   ( ::EVP_PKEY_base_id(PubKey) == EVP_PKEY_EC );
	::EVP_PKEY_free(PubKey);

	// both keys load as P-256 keys, and the cert serves from the TLS context
	CHECK ( KECKey(KStringView(Cert.sKeyPEM)).empty()        == false );
	CHECK ( KECKey(KStringView(Cert.sAccountKeyPEM)).empty() == false );

	REQUIRE ( Server.GetTLSContext()->SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM) == true );

	Server.Stop();

#endif
}

TEST_CASE("KAcmeClient_HTTP01")
{
	// http-01 has no OpenSSL version requirements - the responder is plain HTTP

	AcmeEnv Env;

	if (!Env.HaveServer())
	{
		WARN("no ACME server at " << Env.sDirectory << " - skipping (start Pebble, see comment in this file)");
		return;
	}

	KAcmeClient::Options Options;
	Options.sDirectoryURL = Env.sDirectory;
	Options.bVerifyTLS    = Env.bVerify;
	Options.ChallengeType = KAcmeClient::Challenge::Http01;

	KAcmeClient Acme(std::move(Options));

	auto Resolver = Acme.GetHTTPChallengeResolver();

	CHECK ( Resolver("unknown-token") == "" );

	// serve the challenge on Pebble's http-01 validation port
	KRESTRoutes Routes;

	Routes.AddRoute({ KHTTPMethod::GET, false, "/.well-known/acme-challenge/:token", [&Resolver](KRESTServer& http)
	{
		auto sKeyAuth = Resolver(http.Request.Resource.Query[":token"]);

		if (sKeyAuth.empty())
		{
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "no such challenge" };
		}

		http.SetRawOutput(std::move(sKeyAuth));

	}, KRESTRoute::PLAIN });

	KREST::Options RESTOptions;
	RESTOptions.Type                 = KREST::HTTP;
	RESTOptions.iPort                = Env.iHttpPort;
	RESTOptions.bBlocking            = false;
	RESTOptions.bCreateEphemeralCert = false; // else the server speaks TLS

	KREST REST;
	REQUIRE ( REST.Execute(RESTOptions, Routes) == true );

	// the responder must answer 404 for unknown tokens
	{
		KWebClient Probe;
		auto sBody = Probe.Get(KURL(kFormat("http://localhost:{}/.well-known/acme-challenge/foo", Env.iHttpPort)));
		INFO  ( sBody );
		CHECK ( Probe.GetStatusCode() == 404 );
	}

	auto Cert = Acme.OrderCertificate({ Env.sDomain });

	INFO    ( Acme.Error() );
	REQUIRE ( Acme.HasError() == false );
	REQUIRE ( Cert.IsValid()  == true  );

	KRSACert Leaf(Cert.sCertPEM);
	CHECK ( Leaf.empty()      == false );
	CHECK ( Leaf.IsValidNow() == true  );
}

TEST_CASE("KTCPServer_SetACME")
{
#if !DEKAF2_SMOKETEST_HAS_TLS_ALPN

	WARN("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side - skipping");

#else

	AcmeEnv Env;

	if (!Env.HaveServer())
	{
		WARN("no ACME server at " << Env.sDirectory << " - skipping (start Pebble, see comment in this file)");
		return;
	}

	KTempDir Tmp;

	// the two-line integration: the server starts with a self-signed cert and
	// obtains its real certificate in the background
	KTCPServer Server(Env.iPort, true, 10, false);
	REQUIRE ( Server.SetACME({ Env.sDomain }, "", Env.sDirectory, Tmp.Name(), Env.bVerify) == true );
	REQUIRE ( Server.Start(chrono::seconds(15), false) == true );

	bool bIssued = false;

	for (int i = 0; !bIssued && i < 60; ++i)
	{
		kSleep(chrono::seconds(1));
		bIssued = GetPeerInfo(Env.iPort, Env.sDomain).IsIssued();
	}

	CHECK ( bIssued == true );

	Server.Stop();

#endif
}
