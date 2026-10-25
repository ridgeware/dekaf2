#include "catch.hpp"

#include <dekaf2/web/push/kwebpush.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <atomic>

using namespace dekaf2;

TEST_CASE("KWebPush")
{
#if DEKAF2_HAS_SQLITE3
	SECTION("SQLite init succeeds")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});

		CHECK_FALSE ( Push.HasError() );
		CHECK_FALSE ( Push.GetPublicKey().empty() );
	}

	SECTION("public key is valid base64url 65-byte uncompressed EC P-256")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});

		REQUIRE_FALSE ( Push.HasError() );

		auto sRaw = KBase64Url::Decode(Push.GetPublicKey());
		// uncompressed P-256 public key: 0x04 prefix + 32 bytes X + 32 bytes Y = 65
		CHECK ( sRaw.size() == 65 );
		CHECK ( static_cast<unsigned char>(sRaw[0]) == 0x04 );
	}

	SECTION("key persistence across instances")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KString sKey1;

		{
			KWebPush Push(KStringViewZ{sDB});
			REQUIRE_FALSE ( Push.HasError() );
			sKey1 = Push.GetPublicKey();
		}

		{
			KWebPush Push(KStringViewZ{sDB});
			REQUIRE_FALSE ( Push.HasError() );
			CHECK ( Push.GetPublicKey() == sKey1 );
		}
	}

	SECTION("subscribe and list round-trip")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		KWebPush::Subscription sub;
		sub.sUser      = "alice";
		sub.sEndpoint  = "https://fcm.googleapis.com/fcm/send/test-123";
		sub.sP256dh    = "BHxGRu_eQL-gNob7MkdfZ_sHAz-4aeNiJjBOvGkzLTfwx7c6yCvT9r5j4TT5N6rbOW7r1a-dO0";
		sub.sAuth      = "g6-RtBqPKEd7n5uBhEjV4w";
		sub.sUserAgent = "Chrome/130";

		CHECK ( Push.Subscribe(std::move(sub)) );

		auto Subs = Push.ListSubscriptions();
		REQUIRE ( Subs.size() == 1 );
		CHECK ( Subs[0].sUser     == "alice" );
		CHECK ( Subs[0].sEndpoint == "https://fcm.googleapis.com/fcm/send/test-123" );
		CHECK ( Subs[0].sP256dh   == "BHxGRu_eQL-gNob7MkdfZ_sHAz-4aeNiJjBOvGkzLTfwx7c6yCvT9r5j4TT5N6rbOW7r1a-dO0" );
		CHECK ( Subs[0].sAuth     == "g6-RtBqPKEd7n5uBhEjV4w" );
		CHECK ( Subs[0].sUserAgent == "Chrome/130" );
		CHECK ( Subs[0].tCreated  != KUnixTime{} );
		CHECK ( Subs[0].tLastMod  != KUnixTime{} );
	}

	SECTION("filter subscriptions by user")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		KWebPush::Subscription s1;
		s1.sUser     = "alice";
		s1.sEndpoint = "https://push.example.com/alice";
		s1.sP256dh   = "BAAAA";
		s1.sAuth     = "AAAA";

		KWebPush::Subscription s2;
		s2.sUser     = "bob";
		s2.sEndpoint = "https://push.example.com/bob";
		s2.sP256dh   = "BBBBB";
		s2.sAuth     = "BBBB";

		CHECK ( Push.Subscribe(std::move(s1)) );
		CHECK ( Push.Subscribe(std::move(s2)) );

		auto All   = Push.ListSubscriptions();
		auto Alice = Push.ListSubscriptions("alice");
		auto Bob   = Push.ListSubscriptions("bob");

		CHECK ( All.size()   == 2 );
		CHECK ( Alice.size() == 1 );
		CHECK ( Bob.size()   == 1 );
		CHECK ( Alice[0].sUser == "alice" );
		CHECK ( Bob[0].sUser   == "bob" );
	}

	SECTION("unsubscribe by endpoint")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		KWebPush::Subscription sub;
		sub.sUser     = "alice";
		sub.sEndpoint = "https://push.example.com/unsub";
		sub.sP256dh   = "BAAAA";
		sub.sAuth     = "AAAA";

		Push.Subscribe(std::move(sub));
		CHECK ( Push.HasSubscriptions() );

		CHECK ( Push.Unsubscribe("https://push.example.com/unsub") );
		CHECK_FALSE ( Push.HasSubscriptions() );
	}

	SECTION("unsubscribe by user")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		KWebPush::Subscription s1;
		s1.sUser     = "alice";
		s1.sEndpoint = "https://push.example.com/a1";
		s1.sP256dh   = "BAAAA";
		s1.sAuth     = "AAAA";

		KWebPush::Subscription s2;
		s2.sUser     = "alice";
		s2.sEndpoint = "https://push.example.com/a2";
		s2.sP256dh   = "BAAAA";
		s2.sAuth     = "AAAA";

		Push.Subscribe(std::move(s1));
		Push.Subscribe(std::move(s2));

		CHECK ( Push.ListSubscriptions("alice").size() == 2 );

		CHECK ( Push.UnsubscribeUser("alice") );
		CHECK_FALSE ( Push.HasSubscriptions() );
	}

	SECTION("upsert updates existing subscription")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		KWebPush::Subscription sub;
		sub.sUser     = "alice";
		sub.sEndpoint = "https://push.example.com/upsert";
		sub.sP256dh   = "BAAAA";
		sub.sAuth     = "AAAA";

		Push.Subscribe(sub);

		// update same endpoint with new key
		sub.sP256dh = "BCCCC";
		Push.Subscribe(sub);

		auto Subs = Push.ListSubscriptions();
		REQUIRE ( Subs.size() == 1 );
		CHECK ( Subs[0].sP256dh == "BCCCC" );
	}

	SECTION("HasSubscriptions on empty DB")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		KWebPush Push(KStringViewZ{sDB});
		REQUIRE_FALSE ( Push.HasError() );

		CHECK_FALSE ( Push.HasSubscriptions() );
	}

	SECTION("sContact normalization")
	{
		KTempDir TempDir;
		KString sDB = kFormat("{}/push_test.db", TempDir.Name());

		// default contact
		{
			KWebPush Push(KStringViewZ{sDB});
			CHECK_FALSE ( Push.HasError() );
		}

		// mailto: stripped
		{
			KWebPush Push(KStringViewZ(sDB), "mailto:test@example.com");
			CHECK_FALSE ( Push.HasError() );
		}

		// plain email accepted
		{
			KWebPush Push(KStringViewZ(sDB), "admin@mysite.com");
			CHECK_FALSE ( Push.HasError() );
		}
	}
#endif // DEKAF2_HAS_SQLITE3

	SECTION("guard against failed initialization")
	{
		// pass a nullptr DB backend
		KWebPush Push(std::unique_ptr<KWebPush::DB>(nullptr));

		CHECK ( Push.HasError() );

		// all public methods should fail gracefully
		KWebPush::Subscription sub;
		sub.sEndpoint = "https://push.example.com/fail";
		sub.sP256dh   = "BAAAA";
		sub.sAuth     = "AAAA";

		CHECK_FALSE ( Push.Subscribe(std::move(sub)) );
		CHECK_FALSE ( Push.Unsubscribe("x") );
		CHECK_FALSE ( Push.UnsubscribeUser("x") );
		CHECK_FALSE ( Push.HasSubscriptions() );
		CHECK       ( Push.ListSubscriptions().empty() );
		CHECK       ( Push.Send(KJSON{}, "x") == 0 );
		CHECK       ( Push.Send(KJSON{}) == 0 );
	}
}

#if DEKAF2_HAS_SQLITE3
namespace {

// a push service that answers every request with m_iStatus, and with m_sLocation if set
class KPushTestService : public KTCPServer
{

public:

	KPushTestService(uint16_t iPort)
	: KTCPServer(iPort, false, 5)
	{
	}

	std::atomic<int> m_iStatus   { 201 };
	std::atomic<int> m_iRequests { 0 };
	KString          m_sLocation;

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		stream->SetReaderRightTrim("\r\n");

		KString  sLine;
		uint64_t iLength { 0 };

		while (stream->ReadLine(sLine) && !sLine.empty())
		{
			if (sLine.ToLowerASCII().starts_with("content-length:"))
			{
				iLength = KStringView(sLine).Mid(15).Trim().UInt64();
			}
		}

		KString sBody;
		stream->Read(sBody, iLength);

		++m_iRequests;

		KString sLocation = m_sLocation.empty() ? KString{} : kFormat("Location: {}\r\n", m_sLocation);
		stream->Write(kFormat("HTTP/1.1 {} Push\r\n{}Content-Length: 0\r\nConnection: close\r\n\r\n", m_iStatus.load(), sLocation)).Flush();
	}

}; // KPushTestService

} // end of anonymous namespace

TEST_CASE("KWebPush MaxRedirects")
{
	// push services on ports picked by the OS: the endpoint, and redirect targets
	KPushTestService Service(0);
	KPushTestService Elsewhere(0);
	KPushTestService Third(0);
	KPushTestService Second(0);

	for (auto* pService : { &Service, &Elsewhere, &Third, &Second })
	{
		pService->SetBindAddress("127.0.0.1");
		REQUIRE ( pService->Start(chrono::seconds(2), false) );
		REQUIRE ( pService->GetPort() != 0 );
	}

	auto URL = [](const KPushTestService& Target, KStringView sPath)
	{
		return kFormat("http://127.0.0.1:{}{}", Target.GetPort(), sPath);
	};

	// the push service redirects to a second listener
	Service.m_iStatus   = 307;
	Service.m_sLocation = URL(Elsewhere, "/elsewhere");

	KTempDir TempDir;
	KString sDB = kFormat("{}/push_test.db", TempDir.Name());

	KWebPush Push(KStringViewZ{sDB});
	REQUIRE_FALSE ( Push.HasError() );

	auto Subscription = [](KString sEndpoint)
	{
		KECKey UAKey(true);

		KWebPush::Subscription sub;
		sub.sUser     = "ada";
		sub.sEndpoint = std::move(sEndpoint);
		sub.sP256dh   = KBase64Url::Encode(UAKey.GetPublicKeyRaw());
		sub.sAuth     = KBase64Url::Encode(kGetRandom(16));
		return sub;
	};

	REQUIRE ( Push.Subscribe(Subscription(URL(Service, "/send/ada"))) );

	SECTION("MaxRedirects 0: the redirect is not followed, the send fails, the subscription stays")
	{
		KWebPush::SendOptions Options;
		Options.MaxRedirects = 0;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 0 );
		CHECK ( Service.m_iRequests == 1 );
		CHECK ( Elsewhere.m_iRequests == 0 );
		CHECK ( Push.ListSubscriptions("ada").size() == 1 );
	}

	SECTION("the default follows the redirect")
	{
		KWebPush::SendOptions Options;
		CHECK ( Options.MaxRedirects == 20 );
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 1 );
		CHECK ( Service.m_iRequests == 1 );
		CHECK ( Elsewhere.m_iRequests == 1 );
		CHECK ( Push.ListSubscriptions("ada").size() == 1 );
	}

	SECTION("a chain of two redirects: MaxRedirects 1 fails, MaxRedirects 2 delivers")
	{
		Elsewhere.m_iStatus   = 307;
		Elsewhere.m_sLocation = URL(Third, "/third");

		KWebPush::SendOptions Options;
		Options.MaxRedirects = 1;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 0 );
		CHECK ( Service.m_iRequests == 1 );
		CHECK ( Elsewhere.m_iRequests == 1 );
		CHECK ( Third.m_iRequests == 0 );
		CHECK ( Push.ListSubscriptions("ada").size() == 1 );

		Options.MaxRedirects = 2;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 1 );
		CHECK ( Third.m_iRequests == 1 );
	}

	SECTION("a redirect loop stops after the default 20 redirects")
	{
		Elsewhere.m_iStatus   = 307;
		Elsewhere.m_sLocation = URL(Service, "/send/ada");

		KWebPush::SendOptions Options;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 0 );
		CHECK ( Service.m_iRequests + Elsewhere.m_iRequests == 21 );
		CHECK ( Push.ListSubscriptions("ada").size() == 1 );
	}

	SECTION("MaxRedirects 0: a failed redirect on one subscription does not affect the next")
	{
		REQUIRE ( Push.Subscribe(Subscription(URL(Second, "/send/ada2"))) );

		KWebPush::SendOptions Options;
		Options.MaxRedirects = 0;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 1 );
		CHECK ( Elsewhere.m_iRequests == 0 );
		CHECK ( Second.m_iRequests == 1 );
		CHECK ( Push.ListSubscriptions("ada").size() == 2 );
	}

	SECTION("the default: after a redirect the next subscription goes to its own endpoint")
	{
		REQUIRE ( Push.Subscribe(Subscription(URL(Second, "/send/ada2"))) );

		KWebPush::SendOptions Options;
		CHECK ( Push.Send(KJSON{ { "title", "x" } }, "ada", Options) == 2 );
		CHECK ( Elsewhere.m_iRequests == 1 );
		CHECK ( Second.m_iRequests == 1 );
	}
}
#endif // DEKAF2_HAS_SQLITE3
