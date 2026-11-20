#include "catch.hpp"

#include <dekaf2/http/protocol/kserversentevent.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/core/format/kformat.h>
#include <limits>
#include <vector>

using namespace dekaf2;

namespace {

// parses sStream in pieces of iPieceSize bytes, and returns its events
std::vector<KServerSentEvent> ParseInPieces(KStringView sStream, std::size_t iPieceSize, KServerSentEventParser& Parser)
{
	std::vector<KServerSentEvent> Events;
	KServerSentEvent Event;

	while (!sStream.empty())
	{
		Parser.Add(sStream.substr(0, iPieceSize));
		sStream.remove_prefix(iPieceSize);

		while (Parser.Next(Event))
		{
			Events.push_back(Event);
		}
	}

	return Events;
}

std::vector<KServerSentEvent> ParseInPieces(KStringView sStream, std::size_t iPieceSize)
{
	KServerSentEventParser Parser;
	return ParseInPieces(sStream, iPieceSize, Parser);
}

// the sizes of the pieces that every stream of these tests is parsed with
std::vector<std::size_t> PieceSizes(KStringView sStream)
{
	return { 1, 2, 3, 7, sStream.size() };
}

} // end anonymous namespace

TEST_CASE("KServerSentEvent")
{
	SECTION("data")
	{
		CHECK ( KServerSentEvent("hello").Serialize() == "data: hello\n\n" );

		// every line of the data gets its own field, for all three line breaks
		CHECK ( KServerSentEvent("a\nb\r\nc\rd").Serialize() == "data: a\ndata: b\ndata: c\ndata: d\n\n" );

		// a line break at the end is a last, empty line
		CHECK ( KServerSentEvent("a\n").Serialize() == "data: a\ndata:\n\n" );

		// a space at the start of a line remains one
		CHECK ( KServerSentEvent(" a").Serialize() == "data:  a\n\n" );
	}

	SECTION("type and id")
	{
		CHECK ( KServerSentEvent("hello", "update", "42").Serialize() == "event: update\nid: 42\ndata: hello\n\n" );

		// an event with a type and without data gets an empty line of data
		CHECK ( KServerSentEvent("", "refresh").Serialize() == "event: refresh\ndata:\n\n" );

		// an id alone sets the last id of the client
		CHECK ( KServerSentEvent("", "", "7").Serialize() == "id: 7\n\n" );

		// a type or an id that would start a new field is not written
		CHECK ( KServerSentEvent("x", "a\nb", "1\r2").Serialize() == "data: x\n\n" );
		CHECK ( KServerSentEvent("x", "", KString("1\0002", 3)).Serialize() == "data: x\n\n" );
	}

	SECTION("empty and clear")
	{
		KServerSentEvent Event;
		CHECK ( Event.empty() );
		CHECK ( Event.Serialize().empty() );

		Event.sData  = "hello";
		Event.sEvent = "update";
		Event.sID    = "1";
		CHECK_FALSE ( Event.empty() );

		KString sOut { "retry: 1000\n" };
		Event.Serialize(sOut);
		CHECK ( sOut == "retry: 1000\nevent: update\nid: 1\ndata: hello\n\n" );

		Event.clear();
		CHECK ( Event.empty() );
	}

	SECTION("comment")
	{
		CHECK ( KServerSentEvent::Comment()       == ":\n"         );
		CHECK ( KServerSentEvent::Comment("ping") == ": ping\n"    );
		CHECK ( KServerSentEvent::Comment("a\nb") == ": a\n: b\n"  );
	}

	SECTION("retry")
	{
		CHECK ( KServerSentEvent::Retry(chrono::seconds(3))        == "retry: 3000\n" );
		CHECK ( KServerSentEvent::Retry(chrono::milliseconds(250)) == "retry: 250\n"  );
		CHECK ( KServerSentEvent::Retry(KDuration::zero())         == "retry: 0\n"    );
		CHECK ( KServerSentEvent::Retry(chrono::seconds(-1)).empty() );
	}

	SECTION("media type and request header")
	{
		CHECK ( KMIME::EVENT_STREAM == "text/event-stream" );
		CHECK ( KMIME(KMIME::EVENT_STREAM).IsCompressible() );

		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader("Last-Event-ID"), "42");
		CHECK ( Headers.Headers.Get(KHTTPHeader::LAST_EVENT_ID) == "42" );
		CHECK ( KHTTPHeader(KHTTPHeader::LAST_EVENT_ID).Serialize() == "last-event-id" );
	}
}

TEST_CASE("KServerSentEventParser")
{
	// the examples of the standard (HTML, section "Server-sent events")

	SECTION("standard: one event with three lines of data")
	{
		KStringView sStream { "data: YHOO\ndata: +2\ndata: 10\n\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 1 );
			CHECK ( Events[0].sData  == "YHOO\n+2\n10" );
			CHECK ( Events[0].sEvent == "" );
			CHECK ( Events[0].sID    == "" );
		}
	}

	SECTION("standard: comment, id, reset of the id, space after the colon")
	{
		KStringView sStream { ": test stream\n\ndata: first event\nid: 1\n\ndata:second event\nid\n\ndata:  third event\n\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 3 );
			CHECK ( Events[0].sData == "first event"  );
			CHECK ( Events[0].sID   == "1"            );
			CHECK ( Events[1].sData == "second event" );
			CHECK ( Events[1].sID   == ""             );
			// only one space after the colon is removed
			CHECK ( Events[2].sData == " third event" );
			CHECK ( Events[2].sID   == ""             );
		}
	}

	SECTION("standard: empty data, and a last block without empty line")
	{
		KStringView sStream { "data\n\ndata\ndata\n\ndata:" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 2 );
			CHECK ( Events[0].sData == ""   );
			CHECK ( Events[1].sData == "\n" );
		}
	}

	SECTION("standard: with and without space after the colon")
	{
		KStringView sStream { "data:test\n\ndata: test\n\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 2 );
			CHECK ( Events[0].sData == "test" );
			CHECK ( Events[1].sData == "test" );
		}
	}

	// further cases

	SECTION("line breaks CR, LF and CRLF")
	{
		KStringView sStream { "data: one\r\n\r\ndata: two\r\rdata: three\n\ndata: a\rdata: b\r\ndata: c\n\r\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 4 );
			CHECK ( Events[0].sData == "one"     );
			CHECK ( Events[1].sData == "two"     );
			CHECK ( Events[2].sData == "three"   );
			CHECK ( Events[3].sData == "a\nb\nc" );
		}
	}

	SECTION("byte order mark")
	{
		KStringView sStream { "\xef\xbb\xbf" "data: x\n\n" "\xef\xbb\xbf" "data: ignored\n\ndata: y\n\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			// only at the start of the stream it is a byte order mark, later it is part of a field name
			REQUIRE ( Events.size() == 2 );
			CHECK ( Events[0].sData == "x" );
			CHECK ( Events[1].sData == "y" );
		}

		// a stream may start with bytes that only look like the start of a byte order mark
		auto Events = ParseInPieces("\xef\xbb" "data: ignored\n\ndata: z\n\n", 1);
		REQUIRE ( Events.size() == 1 );
		CHECK ( Events[0].sData == "z" );
	}

	SECTION("type")
	{
		KStringView sStream { "event: update\ndata: 1\n\ndata: 2\n\nevent: lost\n\ndata: 3\n\nevent: a\nevent: b\ndata: 4\n\n" };

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			auto Events = ParseInPieces(sStream, iPieceSize);

			REQUIRE ( Events.size() == 4 );
			CHECK ( Events[0].sEvent == "update" );
			// the type holds for one event
			CHECK ( Events[1].sEvent == ""       );
			// and not past a block without data
			CHECK ( Events[2].sEvent == ""       );
			// the last type of a block counts
			CHECK ( Events[3].sEvent == "b"      );
		}
	}

	SECTION("unknown fields, and field names in another case")
	{
		auto Events = ParseInPieces("Data: no\nfoo: bar\nfoo\ndata: yes\nDATA: no\n\n", 5);

		REQUIRE ( Events.size() == 1 );
		CHECK ( Events[0].sData == "yes" );
	}

	SECTION("id")
	{
		KServerSentEventParser Parser;
		KServerSentEvent       Event;

		CHECK ( Parser.GetLastEventID() == "" );

		// the id holds for the following events
		Parser.Add("id: 5\ndata: a\n\ndata: b\n\n");
		REQUIRE ( Parser.Next(Event) );
		CHECK ( Event.sID == "5" );
		REQUIRE ( Parser.Next(Event) );
		CHECK ( Event.sData == "b" );
		CHECK ( Event.sID   == "5" );
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.GetLastEventID() == "5" );

		// an id counts with the end of its block, also without data
		Parser.Add("id: 6\n");
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.GetLastEventID() == "5" );
		Parser.Add("\n");
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.GetLastEventID() == "6" );

		// an id with a NUL character is ignored
		Parser.Add(KStringView("id: 7\0008\ndata: c\n\n", 17));
		REQUIRE ( Parser.Next(Event) );
		CHECK ( Event.sData == "c" );
		CHECK ( Event.sID   == "6" );
	}

	SECTION("retry")
	{
		KServerSentEventParser Parser;
		KServerSentEvent       Event;

		CHECK_FALSE ( Parser.HasRetry() );

		// only digits count
		Parser.Add("retry: 3s\nretry:\nretry: -1\nretry: 1.5\n");
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK_FALSE ( Parser.HasRetry() );

		// the time counts at once, without the end of a block
		Parser.Add("retry: 3000\n");
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.HasRetry() );
		CHECK ( Parser.GetRetry() == chrono::seconds(3) );

		Parser.Add("retry:0\n");
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.HasRetry() );
		CHECK ( Parser.GetRetry() == KDuration::zero() );

		// a time that a KDuration cannot hold is ignored
		Parser.Add(kFormat("retry: {}\n", std::numeric_limits<int64_t>::max()));
		Parser.Add(kFormat("retry: {}\n", KString(40, '9')));
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.GetRetry() == KDuration::zero() );

		// the longest time that it can hold counts
		Parser.Add(kFormat("retry: {}\n", KDuration::max().milliseconds().count()));
		CHECK_FALSE ( Parser.Next(Event) );
		CHECK ( Parser.GetRetry().milliseconds() == KDuration::max().milliseconds() );
	}

	SECTION("a block is an event when its empty line arrives")
	{
		KServerSentEventParser Parser;
		KServerSentEvent       Event;

		Parser.Add("data: a");
		CHECK_FALSE ( Parser.Next(Event) );
		Parser.Add("b\n");
		CHECK_FALSE ( Parser.Next(Event) );
		Parser.Add("\n");
		REQUIRE ( Parser.Next(Event) );
		CHECK ( Event.sData == "ab" );
		CHECK_FALSE ( Parser.Next(Event) );
	}

	SECTION("clear")
	{
		KServerSentEventParser Parser;
		KServerSentEvent       Event;

		Parser.Add("retry: 10\nid: 1\ndata: a\n\nevent: e\ndata: half");
		REQUIRE ( Parser.Next(Event) );

		Parser.clear();
		CHECK ( Parser.GetLastEventID() == "" );
		CHECK_FALSE ( Parser.HasRetry() );

		// a new stream, that may start with a byte order mark
		Parser.Add("\xef\xbb\xbf" "data: b\n\n");
		REQUIRE ( Parser.Next(Event) );
		CHECK ( Event.sData  == "b" );
		CHECK ( Event.sEvent == ""  );
		CHECK ( Event.sID    == ""  );
	}

	SECTION("the parser reads what Serialize() writes")
	{
		std::vector<KServerSentEvent> Sent
		{
			{ "hello"                        },
			{ "two\nlines", "update", "1"    },
			{ " leading and trailing space " },
			{ "line break at the end\n"      },
			{ ""          , "refresh"        },
			{ "übergrößenträger \xf0\x9f\x98\x80", "größe", "2" },
		};

		KString sStream;
		sStream += KServerSentEvent::Retry(chrono::seconds(5));

		for (const auto& Event : Sent)
		{
			sStream += KServerSentEvent::Comment("keep alive");
			Event.Serialize(sStream);
		}

		for (auto iPieceSize : PieceSizes(sStream))
		{
			INFO ( iPieceSize );
			KServerSentEventParser Parser;
			auto Events = ParseInPieces(sStream, iPieceSize, Parser);

			REQUIRE ( Events.size() == Sent.size() );

			for (std::size_t i = 0; i < Sent.size(); ++i)
			{
				INFO ( i );
				CHECK ( Events[i].sData  == Sent[i].sData  );
				CHECK ( Events[i].sEvent == Sent[i].sEvent );
			}

			// the id holds until the next one
			CHECK ( Events[0].sID == ""  );
			CHECK ( Events[1].sID == "1" );
			CHECK ( Events[4].sID == "1" );
			CHECK ( Events[5].sID == "2" );
			CHECK ( Parser.GetLastEventID() == "2" );
			CHECK ( Parser.GetRetry() == chrono::seconds(5) );
		}
	}
}
