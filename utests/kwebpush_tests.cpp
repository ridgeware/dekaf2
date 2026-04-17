#include "catch.hpp"

#include <dekaf2/web/push/kwebpush.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

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
		CHECK       ( Push.SendToUser("x", KJSON{}) == 0 );
		CHECK       ( Push.SendToAll(KJSON{}) == 0 );
	}
}
