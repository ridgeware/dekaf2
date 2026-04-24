#include "catch.hpp"
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/core/strings/kstring.h>
#include <thread>
#include <chrono>

using namespace dekaf2;

namespace {

// Minimalist authenticator used by every section — matches "alice/wonder" and "bob/builder"
bool TestAuthenticator(KStringView sUser, KStringView sPassword)
{
	if (sUser == "alice" && sPassword == "wonder")  return true;
	if (sUser == "bob"   && sPassword == "builder") return true;
	return false;
}

// Configure a KSession with short timeouts and the purge timer disabled
// (so tests stay deterministic and don't spawn background activity).
KSession::Config TestConfig()
{
	KSession::Config cfg;
	cfg.sCookieName     = "test_session";
	cfg.sCookiePath     = "/";
	cfg.sSameSite       = "Strict";
	cfg.bSecure         = false;     // so we can test without "__Host-" prefix rules
	cfg.bHttpOnly       = true;
	cfg.IdleTimeout     = std::chrono::hours(1);
	cfg.AbsoluteTimeout = std::chrono::hours(8);
	cfg.PurgeInterval   = KDuration::zero(); // disable background timer
	cfg.iTokenBytes     = 32;
	return cfg;
}

} // anonymous

TEST_CASE("KSession")
{
	SECTION("Construct with empty store fails gracefully")
	{
		KSession Session(std::unique_ptr<KSession::Store>{}, TestConfig());
		CHECK ( Session.HasError() );
	}

	SECTION("Login with no authenticator fails")
	{
		KSession Session(std::make_unique<KSessionMemoryStore>(), TestConfig());
		CHECK_FALSE ( Session.HasError() );

		auto sToken = Session.Login("alice", "wonder");
		CHECK ( sToken.empty() );
		CHECK ( Session.HasError() ); // "no authenticator configured"
	}

	SECTION("Login + Validate + Logout")
	{
		KSession Session(std::make_unique<KSessionMemoryStore>(), TestConfig());
		Session.SetAuthenticator(TestAuthenticator);

		// wrong password → no token
		auto sBad = Session.Login("alice", "wrong");
		CHECK ( sBad.empty() );
		CHECK ( Session.Count() == 0 );

		// correct login → non-empty token
		auto sToken = Session.Login("alice", "wonder", "1.2.3.4", "curl/8.0");
		CHECK_FALSE ( sToken.empty() );
		// base64url of 32 raw bytes is 43 chars with no padding
		CHECK ( sToken.size() == 43 );
		CHECK ( Session.Count() == 1 );

		// validate: should succeed and fill the record
		KSession::Record Rec;
		CHECK ( Session.Validate(sToken, &Rec) );
		CHECK ( Rec.sUsername  == "alice"   );
		CHECK ( Rec.sClientIP  == "1.2.3.4" );
		CHECK ( Rec.sUserAgent == "curl/8.0");

		// invalid token → false, no record change
		CHECK_FALSE ( Session.Validate("obviously-not-a-valid-token") );

		// logout → session gone
		CHECK ( Session.Logout(sToken) );
		CHECK ( Session.Count() == 0 );
		CHECK_FALSE ( Session.Validate(sToken) );

		// repeated logout → false (already gone)
		CHECK_FALSE ( Session.Logout(sToken) );
	}

	SECTION("Validate refreshes LastSeen")
	{
		KSession Session(std::make_unique<KSessionMemoryStore>(), TestConfig());
		Session.SetAuthenticator(TestAuthenticator);

		auto sToken = Session.Login("bob", "builder");
		REQUIRE ( !sToken.empty() );

		KSession::Record RecA;
		REQUIRE ( Session.Validate(sToken, &RecA) );
		auto tFirst = RecA.tLastSeen;

		// give the clock a chance to advance by at least 1 second so the
		// stored to_time_t() actually differs
		std::this_thread::sleep_for(std::chrono::milliseconds(1100));

		KSession::Record RecB;
		REQUIRE ( Session.Validate(sToken, &RecB) );

		CHECK ( RecB.tLastSeen > tFirst );
	}

	SECTION("Idle timeout expires sessions")
	{
		auto cfg = TestConfig();
		cfg.IdleTimeout     = std::chrono::milliseconds(500);
		cfg.AbsoluteTimeout = std::chrono::hours(1);

		KSession Session(std::make_unique<KSessionMemoryStore>(), cfg);
		Session.SetAuthenticator(TestAuthenticator);

		auto sToken = Session.Login("alice", "wonder");
		REQUIRE ( !sToken.empty() );

		// within idle window — valid
		CHECK ( Session.Validate(sToken) );

		// wait past idle timeout
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));

		// now expired — Validate should erase it and return false
		CHECK_FALSE ( Session.Validate(sToken) );
		CHECK ( Session.Count() == 0 );
	}

	SECTION("LogoutAllFor removes every session of a user")
	{
		KSession Session(std::make_unique<KSessionMemoryStore>(), TestConfig());
		Session.SetAuthenticator(TestAuthenticator);

		auto sA1 = Session.Login("alice", "wonder");
		auto sA2 = Session.Login("alice", "wonder");
		auto sB1 = Session.Login("bob",   "builder");
		REQUIRE ( !sA1.empty() );
		REQUIRE ( !sA2.empty() );
		REQUIRE ( !sB1.empty() );
		REQUIRE ( Session.Count() == 3 );

		auto iErased = Session.LogoutAllFor("alice");
		CHECK ( iErased == 2 );
		CHECK ( Session.Count() == 1 );
		// bob's session is untouched
		CHECK ( Session.Validate(sB1) );
	}

	SECTION("PurgeExpired removes idle- and absolute-expired sessions only")
	{
		auto cfg = TestConfig();
		cfg.IdleTimeout     = std::chrono::milliseconds(800);
		cfg.AbsoluteTimeout = std::chrono::hours(1);

		KSession Session(std::make_unique<KSessionMemoryStore>(), cfg);
		Session.SetAuthenticator(TestAuthenticator);

		auto sStale = Session.Login("alice", "wonder");
		auto sFresh = Session.Login("bob",   "builder");
		REQUIRE ( Session.Count() == 2 );

		// in 3 steps of 400ms each, actively refresh sFresh at each step
		// so it never goes idle longer than 400ms < 800ms timeout.
		// sStale is left to go idle for the full 1200ms > 800ms.
		for (int i = 0; i < 3; ++i)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
			REQUIRE ( Session.Validate(sFresh) );
		}

		auto iPurged = Session.PurgeExpired();
		CHECK ( iPurged == 1 );
		CHECK ( Session.Count() == 1 );
		// sFresh still valid
		CHECK ( Session.Validate(sFresh) );
		// sStale gone
		CHECK_FALSE ( Session.Validate(sStale) );
	}

	SECTION("Cookie serialization attributes")
	{
		auto cfg = TestConfig();
		cfg.sCookieName = "mycookie";
		cfg.sCookiePath = "/admin";
		cfg.bSecure     = true;
		cfg.bHttpOnly   = true;
		cfg.sSameSite   = "Lax";

		KSession Session(std::make_unique<KSessionMemoryStore>(), cfg);

		auto sSet = Session.SerializeSetCookie("AbCdEf123");
		CHECK ( sSet.starts_with("mycookie=AbCdEf123") );
		CHECK ( sSet.Contains("; Path=/admin") );
		CHECK ( sSet.Contains("; Secure")     );
		CHECK ( sSet.Contains("; HttpOnly")   );
		CHECK ( sSet.Contains("; SameSite=Lax") );
		// session cookies have no Max-Age → browser drops on close
		CHECK_FALSE ( sSet.Contains("Max-Age") );

		// expiry cookie: empty value + Max-Age=0
		auto sDel = Session.SerializeExpiryCookie();
		CHECK ( sDel.starts_with("mycookie=;") );
		CHECK ( sDel.Contains("; Max-Age=0")   );
	}

	SECTION("Tokens are unique across rapid logins")
	{
		KSession Session(std::make_unique<KSessionMemoryStore>(), TestConfig());
		Session.SetAuthenticator(TestAuthenticator);

		std::vector<KString> Tokens;
		for (int i = 0; i < 64; ++i)
		{
			auto s = Session.Login("alice", "wonder");
			REQUIRE ( !s.empty() );
			Tokens.push_back(std::move(s));
		}

		std::sort(Tokens.begin(), Tokens.end());
		auto last = std::unique(Tokens.begin(), Tokens.end());
		CHECK ( last == Tokens.end() ); // no duplicates
	}
}
