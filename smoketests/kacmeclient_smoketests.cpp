#include "catch.hpp"

#include <dekaf2/web/acme/kacmeclient.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/system/os/ksystem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

using namespace dekaf2;

// Runs the full ACME order flow against a local Pebble instance:
//
//   podman run --rm -d --name pebble -p 14000:14000 -e PEBBLE_VA_NOSLEEP=1 ghcr.io/letsencrypt/pebble
//
// Pebble validates tls-alpn-01 by connecting to port 5001 of the ordered domain.
// The default domain "host.containers.internal" resolves to this host from inside
// the container, so no DNS mock is needed. The test skips itself when no ACME
// server responds at the directory URL.
//
// Environment overrides (e.g. for a staging acceptance run on a public server):
//
//   DEKAF2_ACME_DIR    - directory URL      (default https://localhost:14000/dir)
//   DEKAF2_ACME_DOMAIN - domain to order    (default host.containers.internal)
//   DEKAF2_ACME_PORT   - local server port  (default 5001)
//   DEKAF2_ACME_VERIFY - 1 = verify the directory's CA (needed for staging)

TEST_CASE("KAcmeClient_ACME")
{
#if OPENSSL_VERSION_NUMBER < 0x10101000L || defined(LIBRESSL_VERSION_NUMBER)

	WARN("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side - skipping");

#else

	KString sDirectory = kGetEnv("DEKAF2_ACME_DIR"   , "https://localhost:14000/dir");
	KString sDomain    = kGetEnv("DEKAF2_ACME_DOMAIN", "host.containers.internal");
	bool    bVerify    = kGetEnv("DEKAF2_ACME_VERIFY", "0") == "1";
	auto    iPort      = KString(kGetEnv("DEKAF2_ACME_PORT", "5001")).UInt16();

	// skip cleanly when no ACME test server is running
	{
		KWebClient Check;
		Check.SetVerifyCerts(false);
		Check.Get(KURL(sDirectory));

		if (Check.GetStatusCode() != 200)
		{
			WARN("no ACME server at " << sDirectory << " - skipping (start Pebble, see comment in this file)");
			return;
		}
	}

	// the server under test - starts with an ephemeral self-signed cert
	KTCPServer Server(iPort, /*bTLS*/true, /*iMaxThreads*/10, /*bStoreNewCerts*/false);
	REQUIRE ( Server.Start(chrono::seconds(15), /*bBlock*/false) == true );

	auto* TLSContext = Server.GetTLSContext();
	REQUIRE ( TLSContext != nullptr );

	KAcmeClient::Options Options;
	Options.sDirectoryURL = sDirectory;
	Options.bVerifyTLS    = bVerify;

	KAcmeClient Acme(std::move(Options));
	REQUIRE ( Acme.AttachTo(*TLSContext) == true );

	auto Cert = Acme.OrderCertificate({ sDomain });

	INFO    ( Acme.Error() );
	REQUIRE ( Acme.HasError() == false );
	REQUIRE ( Cert.IsValid()  == true  );
	CHECK   ( Cert.sAccountKeyPEM.empty() == false );

	// the leaf is currently valid
	KRSACert Leaf(Cert.sCertPEM);
	CHECK ( Leaf.empty()      == false );
	CHECK ( Leaf.IsValidNow() == true  );

	// returns true if the server presents a CA issued cert (subject != issuer)
	// on a fresh connection with the given SNI name
	auto ServesIssuedCert = [iPort](KStringView sSNI) -> bool
	{
		KTLSClient Client;
		Client.SetManualTLSHandshake(true);

		if (!Client.Connect(kFormat("localhost:{}", iPort), KStreamOptions::None))
		{
			return false;
		}

		if (!sSNI.empty())
		{
			::SSL_set_tlsext_host_name(Client.GetNativeTLSHandle(), KString(sSNI).c_str());
		}

		if (!Client.StartManualTLSHandshake())
		{
			return false;
		}

		auto* Peer = ::SSL_get_peer_certificate(Client.GetNativeTLSHandle());

		if (!Peer)
		{
			return false;
		}

		char szSubject[256] = "";
		char szIssuer [256] = "";
		::X509_NAME_oneline(::X509_get_subject_name(Peer), szSubject, sizeof(szSubject));
		::X509_NAME_oneline(::X509_get_issuer_name (Peer), szIssuer , sizeof(szIssuer ));
		::X509_free(Peer);

		return KStringView(szIssuer) != KStringView(szSubject);
	};

	// renewal path 1: publish the issued cert through SNI dispatch - this switches
	// per handshake and is therefore effective for the very next connection
	auto NewContext = std::make_shared<KTLSContext>(true);
	REQUIRE ( NewContext->SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM) == true );
	REQUIRE ( TLSContext->AddSNIContext(sDomain, NewContext) == true );

	CHECK ( ServesIssuedCert(sDomain) == true  );
	CHECK ( ServesIssuedCert("")      == false ); // no SNI still gets the bootstrap cert

	// renewal path 2: swap the default context's cert - the accept loop keeps one
	// pre-allocated TLS stream with the old cert, so allow a few connections
	REQUIRE ( TLSContext->RemoveSNIContext(sDomain) == true );
	REQUIRE ( TLSContext->SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM) == true );

	bool bSwapped = false;

	for (int i = 0; !bSwapped && i < 3; ++i)
	{
		bSwapped = ServesIssuedCert("");
	}

	CHECK ( bSwapped == true );

	Server.Stop();

#endif
}
