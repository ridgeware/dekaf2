#include "catch.hpp"

#include <dekaf2/core/logging/ktracecontext.h>
#include <dekaf2/core/logging/bits/klogserializer.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/strings/kstring.h>
#include <functional>
#include <thread>

using namespace dekaf2;

namespace {

// the example of the standard
constexpr KStringView sExample { "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" };

bool IsLowerHex(KStringView sHex)
{
	return sHex.find_first_not_of("0123456789abcdef") == KStringView::npos;
}

} // end of anonymous namespace

TEST_CASE("KTraceContext")
{
	SECTION("a new trace")
	{
		auto Context = KTraceContext::Start();

		CHECK_FALSE ( Context.empty() );
		CHECK ( Context.GetTraceID().size()  == 32 );
		CHECK ( Context.GetSpanID().size()   == 16 );
		CHECK ( IsLowerHex(Context.GetTraceID()) );
		CHECK ( IsLowerHex(Context.GetSpanID())  );
		CHECK ( Context.GetParentID().empty() );
		CHECK ( Context.GetTraceState().empty() );
		CHECK ( Context.GetFlags() == KTraceContext::Sampled );
		CHECK ( Context.Serialize() == kFormat("00-{}-{}-01", Context.GetTraceID(), Context.GetSpanID()) );

		auto Other = KTraceContext::Start();

		CHECK ( Other.GetTraceID() != Context.GetTraceID() );
		CHECK ( Other.GetSpanID()  != Context.GetSpanID()  );
	}

	SECTION("a trace that a caller started")
	{
		KTraceContext Context;

		REQUIRE ( Context.Parse(sExample) );
		CHECK ( Context.GetTraceID()  == "4bf92f3577b34da6a3ce929d0e0e4736" );
		CHECK ( Context.GetParentID() == "00f067aa0ba902b7" );
		CHECK ( Context.GetFlags()    == 1 );

		// the own span id is new
		CHECK ( Context.GetSpanID().size() == 16 );
		CHECK ( IsLowerHex(Context.GetSpanID()) );
		CHECK ( Context.GetSpanID() != Context.GetParentID() );

		// a request to another service carries the own span id
		CHECK ( Context.Serialize() == kFormat("00-4bf92f3577b34da6a3ce929d0e0e4736-{}-01", Context.GetSpanID()) );
	}

	SECTION("the flags stay as they arrived")
	{
		KTraceContext Context;

		REQUIRE ( Context.Parse("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-03") );
		CHECK ( Context.GetFlags() == 3 );
		CHECK ( Context.Serialize().ends_with("-03") );

		REQUIRE ( Context.Parse("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00") );
		CHECK ( Context.GetFlags() == 0 );
	}

	SECTION("a traceparent that is not valid")
	{
		for (KStringView sTraceParent :
		{
			"",
			"00",
			// uppercase hex digits
			"00-4BF92F3577B34DA6A3CE929D0E0E4736-00f067aa0ba902b7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00F067AA0BA902B7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-0A",
			"0A-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
			// ids of zeros only
			"00-00000000000000000000000000000000-00f067aa0ba902b7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-01",
			// version ff
			"ff-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
			// version 00 has exactly 55 characters
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01-",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-1",
			"00-4bf92f3577b34da6a3ce929d0e0e473-00f067aa0ba902b7-011",
			// separators
			"00_4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736_00f067aa0ba902b7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7_01",
			// other characters
			"00-4bf92f3577b34da6a3ce929d0e0e473g-00f067aa0ba902b7-01",
			" 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
			"00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01\n",
			// a later version with more fields needs a dash after the flags
			"01-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01x",
		})
		{
			INFO ( sTraceParent );

			KTraceContext Context;

			// an earlier content is gone after a failed parse
			REQUIRE ( Context.Parse(sExample, "a=b") );
			CHECK_FALSE ( Context.Parse(sTraceParent, "a=b") );
			CHECK ( Context.empty() );
			CHECK ( Context.GetSpanID().empty() );
			CHECK ( Context.GetParentID().empty() );
			CHECK ( Context.GetTraceState().empty() );
			CHECK ( Context.Serialize().empty() );
		}
	}

	SECTION("a later version is read as version 00")
	{
		for (KStringView sTraceParent :
		{
			"01-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
			"cc-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01-what-comes-later",
		})
		{
			INFO ( sTraceParent );

			KTraceContext Context;

			REQUIRE ( Context.Parse(sTraceParent) );
			CHECK ( Context.GetTraceID()  == "4bf92f3577b34da6a3ce929d0e0e4736" );
			CHECK ( Context.GetParentID() == "00f067aa0ba902b7" );
			// we send the version we know
			CHECK ( Context.Serialize().starts_with("00-4bf92f3577b34da6a3ce929d0e0e4736-") );
		}
	}

	SECTION("tracestate")
	{
		KTraceContext Context;

		REQUIRE ( Context.Parse(sExample, "rojo=00f067aa0ba902b7,congo=t61rcWkgMzE") );
		CHECK ( Context.GetTraceState() == "rojo=00f067aa0ba902b7,congo=t61rcWkgMzE" );

		// empty members and white space are allowed
		REQUIRE ( Context.Parse(sExample, " a=1 ,, \tb=2") );
		CHECK ( Context.GetTraceState() == " a=1 ,, \tb=2" );

		// a tracestate that is not valid is dropped, the trace stays
		KString sTooMany;

		for (int i = 0; i < 33; ++i)
		{
			sTooMany += kFormat("k{}=v,", i);
		}

		for (KStringView sTraceState : { KStringView("a=1\nb=2"), KStringView("a=\x01"), KStringView("a=\xc3\xa4"), KStringView(sTooMany) })
		{
			REQUIRE ( Context.Parse(sExample, sTraceState) );
			CHECK ( Context.GetTraceState().empty() );
			CHECK ( Context.GetTraceID() == "4bf92f3577b34da6a3ce929d0e0e4736" );
		}

		// 32 members are permitted
		sTooMany.clear();

		for (int i = 0; i < 32; ++i)
		{
			sTooMany += kFormat("k{}=v,", i);
		}

		REQUIRE ( Context.Parse(sExample, sTooMany) );
		CHECK ( Context.GetTraceState() == sTooMany );
	}

	SECTION("the context of the thread")
	{
		CHECK ( KTraceContext::Current().empty() );

		auto Outer = KTraceContext::Start();
		auto Inner = KTraceContext::Start();

		{
			KTraceContext::Scope Trace(Outer);

			CHECK ( KTraceContext::Current().GetTraceID() == Outer.GetTraceID() );

			{
				KTraceContext::Scope Trace(Inner);
				CHECK ( KTraceContext::Current().GetTraceID() == Inner.GetTraceID() );

				// another thread has its own context
				KString sInOtherThread { "not set" };
				std::thread Other([&sInOtherThread]() { sInOtherThread = KTraceContext::Current().GetTraceID(); });
				Other.join();
				CHECK ( sInOtherThread.empty() );
			}

			CHECK ( KTraceContext::Current().GetTraceID() == Outer.GetTraceID() );

			{
				// an empty context switches it off for a while
				KTraceContext::Scope Trace(KTraceContext{});
				CHECK ( KTraceContext::Current().empty() );
			}

			CHECK ( KTraceContext::Current().GetTraceID() == Outer.GetTraceID() );
		}

		CHECK ( KTraceContext::Current().empty() );
	}

	SECTION("Bind() carries the context into another thread")
	{
		auto Context = KTraceContext::Start();

		KString sDuring;
		KString sAfter { "not set" };

		std::function<void()> Task;

		{
			KTraceContext::Scope Trace(Context);
			Task = KTraceContext::Bind([&sDuring]() { sDuring = KTraceContext::Current().GetTraceID(); });
		}

		// the context of the creating thread has ended, the task keeps it
		std::thread Other([&]()
		{
			Task();
			sAfter = KTraceContext::Current().GetTraceID();
		});

		Other.join();

		CHECK ( sDuring == Context.GetTraceID() );
		CHECK ( sAfter.empty() );

		// arguments and the result pass through
		auto Add = KTraceContext::Bind([](int a, int b) { return a + b; });
		CHECK ( Add(2, 3) == 5 );
	}

	SECTION("the header names")
	{
		CHECK ( KHTTPHeader("traceparent") == KHTTPHeader::TRACEPARENT );
		CHECK ( KHTTPHeader("Traceparent") == KHTTPHeader::TRACEPARENT );
		CHECK ( KHTTPHeader("TRACESTATE")  == KHTTPHeader::TRACESTATE  );
		CHECK ( KHTTPHeader(KHTTPHeader::TRACEPARENT).Serialize() == "traceparent" );
		CHECK ( KHTTPHeader(KHTTPHeader::TRACESTATE).Serialize()  == "tracestate"  );
	}
}

#ifdef DEKAF2_WITH_KLOG
TEST_CASE("KTraceContext in log lines")
{
	KTraceContext Context;
	REQUIRE ( Context.Parse(sExample) );

	SECTION("text lines show the start of the trace id on request")
	{
		KLogTTYSerializer Serializer;

		// without a context of the thread
		Serializer.Set(1, "UTEST", "utest", "Function", "message");
		KString sWithout = Serializer.Get(false, true);

		{
			KTraceContext::Scope Trace(Context);

			Serializer.Set(1, "UTEST", "utest", "Function", "message");
			KString sOff = Serializer.Get(false, false);

			Serializer.Set(1, "UTEST", "utest", "Function", "message");
			KString sOn = Serializer.Get(false, true);

			Serializer.Set(1, "UTEST", "utest", "Function", "message");
			KString sHiRes = Serializer.Get(true, true);

			INFO ( sOff );
			INFO ( sOn );
			INFO ( sHiRes );

			CHECK_FALSE ( sOff.contains("4bf92f3577b34da6") );
			CHECK ( sOn.contains(" | 4bf92f3577b34da6 | ") );
			CHECK_FALSE ( sOn.contains("4bf92f3577b34da6a") );
			CHECK ( sHiRes.contains(" | 4bf92f3577b34da6 | +") );
			CHECK ( sOn.ends_with("Function(): message\n") );
		}

		INFO  ( sWithout );
		CHECK_FALSE ( sWithout.contains("4bf92f35") );
	}

#ifdef DEKAF2_KLOG_WITH_TCP
	SECTION("JSON lines have the ids")
	{
		KLogJSONSerializer Serializer;

		Serializer.Set(1, "UTEST", "utest", "Function", "without context");
		auto jWithout = kjson::Parse(Serializer.Get());

		CHECK_FALSE ( jWithout.contains("trace_id") );

		KTraceContext::Scope Trace(Context);

		Serializer.Set(1, "UTEST", "utest", "Function", "with context");
		auto jWith = kjson::Parse(Serializer.Get());

		INFO  ( jWith.dump() );
		CHECK ( jWith("trace_id").String()  == "4bf92f3577b34da6a3ce929d0e0e4736" );
		CHECK ( jWith("span_id").String()   == Context.GetSpanID() );
		CHECK ( jWith("parent_id").String() == "00f067aa0ba902b7" );

		// a new trace has no parent
		KTraceContext::Scope NewTrace(KTraceContext::Start());

		Serializer.Set(1, "UTEST", "utest", "Function", "new trace");
		auto jNew = kjson::Parse(Serializer.Get());

		CHECK ( jNew("trace_id").String().size() == 32 );
		CHECK_FALSE ( jNew.contains("parent_id") );
	}
#endif
}
#endif
