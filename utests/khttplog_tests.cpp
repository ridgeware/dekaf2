#include "catch.hpp"

#include <dekaf2/http/server/khttplog.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>

using namespace dekaf2;

namespace {
KTempDir TempDir;

// EXTENDED format log lines with known field values
KStringView sExtendedLine1 =
	R"(172.31.71.1 - user1 [07/May/2025:16:55:52 +0000] "POST /page3/ HTTP/1.1" 200 29222 "https://host.testdomain.org/page1/" "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" "other.testdomain.org" 869 1811 68 63961; "192.168.1.1")";

KStringView sExtendedLine2 =
	R"(172.31.71.1 - user1 [07/May/2025:16:55:53 +0000] "GET /page4/ HTTP/1.1" 200 38969 "https://host.testdomain.org/page2/" "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" "other.testdomain.org" 741 5595 67 1289; "192.168.1.1")";

KStringView sExtendedLine3 =
	R"(10.0.0.5 - admin [08/May/2025:10:00:00 +0000] "DELETE /api/v1/resource/42 HTTP/1.1" 204 0 "-" "curl/8.0" "api.example.com" 100 200 99 500; "-")";

// COMMON format: %h %l %u %t \"%r\" %>s %b
KStringView sCommonLine =
	R"(192.168.1.100 - john [10/Oct/2023:13:55:36 +0000] "GET /index.html HTTP/1.1" 200 2326)";

// COMBINED format: adds referer and user-agent
KStringView sCombinedLine =
	R"(192.168.1.100 - john [10/Oct/2023:13:55:36 +0000] "GET /index.html HTTP/1.1" 200 2326 "https://www.example.com/" "Mozilla/5.0")";

} // end of anonymous namespace

TEST_CASE("KHTTPLog")
{
	SECTION("default state")
	{
		KHTTPLog Logger;
		CHECK ( Logger.is_open() == false );
	}

	SECTION("Open with format")
	{
		KHTTPLog Logger;
		auto sLogname = kFormat("{}/httplog-open.log", TempDir.Name());
		CHECK ( Logger.Open(KHTTPLog::LOG_FORMAT::EXTENDED, sLogname) == true  );
		CHECK ( Logger.is_open()                                      == true  );
	}

	SECTION("Open convenience")
	{
		KHTTPLog Logger;
		auto sLogname = kFormat("{}/httplog-conv.log", TempDir.Name());
		CHECK ( Logger.Open(sLogname) == true  );
		CHECK ( Logger.is_open()      == true  );
	}

	SECTION("Open PARSED without format string fails")
	{
		KHTTPLog Logger;
		auto sLogname = kFormat("{}/httplog-parsed-fail.log", TempDir.Name());
		CHECK ( Logger.Open(KHTTPLog::LOG_FORMAT::PARSED, sLogname) == false );
		CHECK ( Logger.is_open()                                    == false );
	}

	SECTION("Open PARSED with format string")
	{
		KHTTPLog Logger;
		auto sLogname = kFormat("{}/httplog-parsed-ok.log", TempDir.Name());
		CHECK ( Logger.Open(KHTTPLog::LOG_FORMAT::PARSED, sLogname, "%h %s") == true  );
		CHECK ( Logger.is_open()                                              == true  );
	}

	SECTION("constructor with format")
	{
		auto sLogname = kFormat("{}/httplog-ctor.log", TempDir.Name());
		KHTTPLog Logger(KHTTPLog::LOG_FORMAT::COMBINED, sLogname);
		CHECK ( Logger.is_open() == true );
	}

	SECTION("double open fails")
	{
		KHTTPLog Logger;
		auto sLogname = kFormat("{}/httplog-double.log", TempDir.Name());
		CHECK ( Logger.Open(KHTTPLog::LOG_FORMAT::COMMON, sLogname) == true  );
		CHECK ( Logger.Open(KHTTPLog::LOG_FORMAT::COMMON, sLogname) == false );
	}

}

namespace {

KStringView sRequest =
	"POST /page3/subpage?query=value HTTP/1.1\r\n"
	"Host: other.testdomain.org\r\n"
	"Referer: https://host.testdomain.org/page1/\r\n"
	"User-Agent: Mozilla/5.0 \"quoted\" back\\slash\r\n"
	"X-Forwarded-For: 192.168.1.1\r\n"
	"Content-Length: 0\r\n"
	"Connection: close\r\n"
	"\r\n";

// runs sRequest through a KRESTServer that writes its access log with Options.Logger into
// sLogname, appends the response to sResponse, and returns the log line
KString LogRequest(KRESTServer::Options& Options, KStringViewZ sLogname, KString& sResponse)
{
	// a debug build always prints the json with indentation, a release build only with bPrettyPrint
	Options.bPrettyPrint = true;

	KInStringStream iss(sRequest);
	KOutStringStream oss(sResponse);
	KStream stream(iss, oss);
	KRESTRoutes Routes;
	Routes.AddRoute({ KHTTPMethod::POST, false, "/page3/subpage", [](KRESTServer& http)
	{
		// a user name with a space, which the unquoted %u field must not contain
		http.SetAuthenticatedUser("first last");
		http.json.tx["response"] = "hello";
	}});
	KRESTServer HTTP(stream, "172.31.71.1:52044", url::KProtocol::HTTP, 80, Routes, Options);
	HTTP.Execute();

	// the log line is written by a background thread
	KString sLine;
	KStopTime Timeout;

	while (!sLine.ends_with('\n') && Timeout.elapsed() < chrono::seconds(5))
	{
		kSleep(chrono::milliseconds(1));
		sLine = kReadAll(sLogname);
	}

	return sLine;
}

} // end of anonymous namespace

TEST_CASE("KHTTPLog output")
{
	// the loggers write in a background thread, and must outlive their last line
	KRESTServer::Options Common;
	KRESTServer::Options Combined;
	KRESTServer::Options Extended;
	KRESTServer::Options Parsed;

	auto sCommonLog   = kFormat("{}/httplog-common.log"  , TempDir.Name());
	auto sCombinedLog = kFormat("{}/httplog-combined.log", TempDir.Name());
	auto sExtendedLog = kFormat("{}/httplog-extended.log", TempDir.Name());
	auto sParsedLog   = kFormat("{}/httplog-parsed.log"  , TempDir.Name());

	REQUIRE ( Common.Logger.Open(KHTTPLog::LOG_FORMAT::COMMON, sCommonLog) );
	REQUIRE ( Combined.Logger.Open(KHTTPLog::LOG_FORMAT::COMBINED, sCombinedLog) );
	REQUIRE ( Extended.Logger.Open(KHTTPLog::LOG_FORMAT::EXTENDED, sExtendedLog) );
	REQUIRE ( Parsed.Logger.Open(KHTTPLog::LOG_FORMAT::PARSED, sParsedLog, "%u|%T|%{ms}T|%b|%B") );

	KString sResponse;

	auto sCommon   = LogRequest(Common  , sCommonLog  , sResponse);
	auto sCombined = LogRequest(Combined, sCombinedLog, sResponse);
	auto sParsed   = LogRequest(Parsed  , sParsedLog  , sResponse);
	sResponse.clear();
	auto sExtended = LogRequest(Extended, sExtendedLog, sResponse);

	// the time stamp and the duration differ from request to request
	auto Normalize = [](KString sLine)
	{
		auto iStart = sLine.find('[');
		sLine.replace(iStart, sLine.find(']') - iStart + 1, "[time]");
		auto iEnd = sLine.find(';');

		if (iEnd != KString::npos)
		{
			iStart = sLine.rfind(' ', iEnd) + 1;
			sLine.replace(iStart, iEnd - iStart, "duration");
		}

		return sLine;
	};

	// the response body is {"response":"hello"}, printed with indentation and a line feed
	CHECK ( Normalize(sCommon)   == R"(172.31.71.1 - firstlast [time] "POST /page3/subpage?query=value HTTP/1.1" 200 25)" "\n" );
	CHECK ( Normalize(sCombined) == R"(172.31.71.1 - firstlast [time] "POST /page3/subpage?query=value HTTP/1.1" 200 25 "https://host.testdomain.org/page1/" "Mozilla/5.0 \"quoted\" back\\slash")" "\n" );
	// the request is executed in this thread
	CHECK ( Normalize(sExtended) == kFormat(R"(172.31.71.1 - firstlast [time] "POST /page3/subpage?query=value HTTP/1.1" 200 25 "https://host.testdomain.org/page1/" "Mozilla/5.0 \"quoted\" back\\slash" "other.testdomain.org" {} {} {} duration; "192.168.1.1")" "\n",
	                                        sRequest.size(), sResponse.size(), kGetTid()) );
	// the user without spaces, and the durations in seconds and milliseconds as numbers, also
	// when they are 0
	auto Fields = sParsed.Split("|");
	REQUIRE ( Fields.size() == 5 );
	CHECK ( Fields[0] == "firstlast" );
	CHECK ( kIsInteger(Fields[1]) );
	CHECK ( kIsInteger(Fields[2]) );
	CHECK ( Fields[3] == "25" );
	CHECK ( Fields[4] == "25" );

	// the parser reads the EXTENDED line back
	{
		KHTTPLogParser Log(sExtendedLog);
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.sRemoteIP            == "172.31.71.1" );
		CHECK ( log.Method.Serialize()   == "POST" );
		CHECK ( log.Resource.Serialize() == "/page3/subpage?query=value" );
		CHECK ( log.iHTTPStatus          == 200 );
		CHECK ( log.sReferrer            == "https://host.testdomain.org/page1/" );
		CHECK ( log.sUserAgent           == "Mozilla/5.0 \"quoted\" back\\slash" );
		CHECK ( log.sHost                == "other.testdomain.org" );
		CHECK ( log.sForwardedFor        == "192.168.1.1" );
		// the time stamp has the form of kFormCommonLogTimestamp()
		CHECK ( sExtended.contains(kFormCommonLogTimestamp(KUTCTime(log.Timestamp))) );
	}

	// %D is a plain number of microseconds
	KStringView sTTLB = sExtended;
	sTTLB = sTTLB.Left(sTTLB.rfind(';'));
	sTTLB.remove_prefix(sTTLB.rfind(' ') + 1);
	CHECK ( kIsInteger(sTTLB) );
}

TEST_CASE("KHTTPLogParser")
{
	SECTION("Parse EXTENDED format")
	{
		auto sLogname = kFormat("{}/parser-extended.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n");

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.is_open() );

		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.sRemoteIP            == "172.31.71.1" );
		CHECK ( log.sUser                == "user1" );
		CHECK ( log.Timestamp            == KUnixTime("2025-05-07 16:55:52") );
		CHECK ( log.Method.Serialize()   == "POST" );
		CHECK ( log.Resource.Serialize() == "/page3/" );
		CHECK ( log.HTTPVersion.Serialize() == KHTTPVersion(KHTTPVersion::http11).Serialize() );
		CHECK ( log.iHTTPStatus          == 200   );
		CHECK ( log.iContentLength       == 29222 );
		CHECK ( log.sReferrer            == "https://host.testdomain.org/page1/" );
		CHECK ( log.sUserAgent           == "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" );
		CHECK ( log.sHost                == "other.testdomain.org" );
		CHECK ( log.iRXBytes             == 869   );
		CHECK ( log.iTXBytes             == 1811  );
		CHECK ( log.iProcessID           == 68    );
		CHECK ( log.TotalTime.microseconds().count() == 63961 );
		CHECK ( log.sForwardedFor        == "192.168.1.1" );

		auto log2 = Log.Next();
		CHECK ( log2.IsValid() );
		CHECK ( log2.Method.Serialize()   == "GET" );
		CHECK ( log2.Resource.Serialize() == "/page4/" );
		CHECK ( log2.Timestamp            == KUnixTime("2025-05-07 16:55:53") );

		// end of file
		auto log3 = Log.Next();
		CHECK ( log3.IsValid() == false );
	}

	SECTION("Parse COMMON format")
	{
		auto sLogname = kFormat("{}/parser-common.log", TempDir.Name());
		kWriteFile(sLogname, KString(sCommonLine) + "\n");

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.is_open() );

		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.sRemoteIP            == "192.168.1.100" );
		CHECK ( log.sUser                == "john" );
		CHECK ( log.Timestamp            == KUnixTime("2023-10-10 13:55:36") );
		CHECK ( log.Method.Serialize()   == "GET" );
		CHECK ( log.Resource.Serialize() == "/index.html" );
		CHECK ( log.iHTTPStatus          == 200  );
		CHECK ( log.iContentLength       == 2326 );
		// COMMON format has no referer/user-agent/host/extended fields
		CHECK ( log.sReferrer.empty()    );
		CHECK ( log.sUserAgent.empty()   );
		CHECK ( log.sHost.empty()        );
	}

	SECTION("Parse COMBINED format")
	{
		auto sLogname = kFormat("{}/parser-combined.log", TempDir.Name());
		kWriteFile(sLogname, KString(sCombinedLine) + "\n");

		KHTTPLogParser Log(sLogname);
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.sRemoteIP            == "192.168.1.100" );
		CHECK ( log.sReferrer            == "https://www.example.com/" );
		CHECK ( log.sUserAgent           == "Mozilla/5.0" );
		// COMBINED has no host/extended fields
		CHECK ( log.sHost.empty()        );
	}

	SECTION("Parse DELETE with no content")
	{
		auto sLogname = kFormat("{}/parser-delete.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine3) + "\n");

		KHTTPLogParser Log(sLogname);
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.Method.Serialize()   == "DELETE" );
		CHECK ( log.Resource.Serialize() == "/api/v1/resource/42" );
		CHECK ( log.iHTTPStatus          == 204 );
		CHECK ( log.iContentLength       == 0   );
		// "-" in quotes is parsed as literal dash, not converted to empty
		CHECK ( log.sReferrer            == "-" );
		CHECK ( log.sForwardedFor        == "-" );
	}

	SECTION("Parse escaped fields")
	{
		// the writer escapes quotes, backslashes and control characters with a backslash
		KString sInput(R"(10.0.0.5 - us\"er [08/May/2025:10:00:00 +0000] "GET /a\"b HTTP/1.1" 200 12 )"
		               R"("https://example.com/\"quoted\"" "agent \"x\" back\\slash\ttab\x01ctl \xZZ" )"
		               R"("api.example.com" 100 200 99 500; "192.168.1.1")");
		KHTTPLogParser::Data log(sInput);

		CHECK ( log.IsValid() );
		CHECK ( log.sUser                == "us\"er" );
		CHECK ( log.Method.Serialize()   == "GET" );
		CHECK ( log.Resource.Serialize() == "/a%22b" );
		CHECK ( log.iHTTPStatus          == 200 );
		CHECK ( log.iContentLength       == 12 );
		CHECK ( log.sReferrer            == "https://example.com/\"quoted\"" );
		CHECK ( log.sUserAgent           == "agent \"x\" back\\slash\ttab\x01" "ctl \\xZZ" );
		// the fields after the escaped ones
		CHECK ( log.sHost                == "api.example.com" );
		CHECK ( log.iRXBytes             == 100 );
		CHECK ( log.iTXBytes             == 200 );
		CHECK ( log.TotalTime.microseconds().count() == 500 );
		CHECK ( log.sForwardedFor        == "192.168.1.1" );
		// the log line itself stays as it was
		CHECK ( log.GetLogLine()         == sInput );
	}

	SECTION("Parse unterminated quote")
	{
		KHTTPLogParser::Data log(KString(R"(10.0.0.5 - - [08/May/2025:10:00:00 +0000] "GET / HTTP/1.1" 200 12 "ref" "agent \" end\)"));

		CHECK ( log.sReferrer  == "ref" );
		// without a closing quote the field takes the rest of the line
		CHECK ( log.sUserAgent == "agent \" end\\" );
	}

	SECTION("Parse invalid line")
	{
		KHTTPLogParser::Data log(KString("this is not a valid log line"));
		CHECK ( log.IsValid() == false );
	}

	SECTION("Parse empty line")
	{
		KHTTPLogParser::Data log(KString(""));
		CHECK ( log.IsValid() == false );
	}

	SECTION("GetLogLine returns original input")
	{
		KString sInput(sExtendedLine1);
		KHTTPLogParser::Data log(sInput);
		CHECK ( log.IsValid() );
		CHECK ( log.GetLogLine() == sInput );
	}

	SECTION("default Data is invalid")
	{
		KHTTPLogParser::Data log;
		CHECK ( log.IsValid() == false );
	}

	SECTION("Open from file")
	{
		auto sLogname = kFormat("{}/parser-open-file.log", TempDir.Name());
		kWriteFile(sLogname, KString(sCommonLine) + "\n");

		KHTTPLogParser Log;
		CHECK ( Log.is_open() == false );
		CHECK ( Log.Open(sLogname) == true );
		CHECK ( Log.is_open() == true );
	}

	SECTION("Open from stream")
	{
		auto sLogname = kFormat("{}/parser-open-stream.log", TempDir.Name());
		kWriteFile(sLogname, KString(sCommonLine) + "\n");
		KInFile File(sLogname);

		KHTTPLogParser Log;
		CHECK ( Log.Open(File) == true );
		CHECK ( Log.is_open()  == true );
		auto log = Log.Next();
		CHECK ( log.IsValid() );
	}

	SECTION("Open nonexistent file")
	{
		KHTTPLogParser Log;
		CHECK ( Log.Open("/nonexistent/file.log") == false );
		CHECK ( Log.is_open() == false );
	}

	SECTION("Next with StartDate filter")
	{
		auto sLogname = kFormat("{}/parser-startdate.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		// start after line1's timestamp but before line2's
		KHTTPLogParser Log(sLogname, KUnixTime("2025-05-07 16:55:53"));
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		// should skip line1, return line2
		CHECK ( log.Timestamp == KUnixTime("2025-05-07 16:55:53") );
		CHECK ( log.Method.Serialize() == "GET" );
	}

	SECTION("Next with EndDate filter")
	{
		auto sLogname = kFormat("{}/parser-enddate.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		KHTTPLogParser Log(sLogname);
		// enddate before line2's timestamp: should only return line1
		auto log1 = Log.Next(KUnixTime("2025-05-07 16:55:53"));
		CHECK ( log1.IsValid() );
		CHECK ( log1.Timestamp == KUnixTime("2025-05-07 16:55:52") );

		auto log2 = Log.Next(KUnixTime("2025-05-07 16:55:53"));
		CHECK ( log2.IsValid() == false );
	}

	SECTION("Next with StartDate and EndDate")
	{
		auto sLogname = kFormat("{}/parser-range.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		// window: [16:55:53 .. 11:00:00 next day) => line2 and line3
		KHTTPLogParser Log(sLogname, KUnixTime("2025-05-07 16:55:53"));
		auto log = Log.Next(KUnixTime("2025-05-08 11:00:00"));
		CHECK ( log.IsValid() );
		CHECK ( log.Method.Serialize() == "GET" );

		auto log2 = Log.Next(KUnixTime("2025-05-08 11:00:00"));
		CHECK ( log2.IsValid() );
		CHECK ( log2.Method.Serialize() == "DELETE" );

		auto log3 = Log.Next(KUnixTime("2025-05-08 11:00:00"));
		CHECK ( log3.IsValid() == false );
	}

	SECTION("Rewind")
	{
		auto sLogname = kFormat("{}/parser-rewind.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n");

		KHTTPLogParser Log(sLogname);

		auto log1 = Log.Next();
		CHECK ( log1.IsValid() );
		CHECK ( log1.Method.Serialize() == "POST" );

		CHECK ( Log.Rewind() == true );

		// after rewind, should read the same first entry again
		auto log1b = Log.Next();
		CHECK ( log1b.IsValid() );
		CHECK ( log1b.Method.Serialize() == "POST" );
		CHECK ( log1b.Timestamp          == log1.Timestamp );
	}

	SECTION("Rewind when not open")
	{
		KHTTPLogParser Log;
		CHECK ( Log.Rewind() == false );
	}

	SECTION("OldestDate")
	{
		auto sLogname = kFormat("{}/parser-oldest.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		KHTTPLogParser Log(sLogname);

		// read one entry to advance past beginning
		auto log = Log.Next();
		CHECK ( log.IsValid() );

		// OldestDate should still return the first entry's timestamp
		CHECK ( Log.OldestDate() == KUnixTime("2025-05-07 16:55:52") );

		// verify stream position is preserved - next entry should be line2
		auto log2 = Log.Next();
		CHECK ( log2.IsValid() );
		CHECK ( log2.Method.Serialize() == "GET" );
	}

	SECTION("NewestDate")
	{
		auto sLogname = kFormat("{}/parser-newest.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		KHTTPLogParser Log(sLogname);

		CHECK ( Log.NewestDate() == KUnixTime("2025-05-08 10:00:00") );

		// verify stream position is preserved - first call to Next should return line1
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.Method.Serialize() == "POST" );
	}

	SECTION("NewestDate with trailing newline")
	{
		auto sLogname = kFormat("{}/parser-newest-trail.log", TempDir.Name());
		// trailing empty lines should be skipped
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n\n\n");

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.NewestDate() == KUnixTime("2025-05-07 16:55:53") );
	}

	SECTION("OldestDate and NewestDate when not open")
	{
		KHTTPLogParser Log;
		CHECK ( Log.OldestDate() == KUnixTime{} );
		CHECK ( Log.NewestDate() == KUnixTime{} );
	}

	SECTION("OldestDate and NewestDate on empty file")
	{
		auto sLogname = kFormat("{}/parser-empty.log", TempDir.Name());
		kWriteFile(sLogname, "");

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.OldestDate() == KUnixTime{} );
		CHECK ( Log.NewestDate() == KUnixTime{} );
	}

	SECTION("OldestDate and NewestDate single entry")
	{
		auto sLogname = kFormat("{}/parser-single.log", TempDir.Name());
		kWriteFile(sLogname, KString(sCommonLine) + "\n");

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.OldestDate() == KUnixTime("2023-10-10 13:55:36") );
		CHECK ( Log.NewestDate() == KUnixTime("2023-10-10 13:55:36") );
	}

	SECTION("skip invalid lines in log")
	{
		auto sLogname = kFormat("{}/parser-skipinvalid.log", TempDir.Name());
		KString sContent;
		sContent += "this is garbage\n";
		sContent += "more garbage here\n";
		sContent += sCommonLine;
		sContent += "\n";
		kWriteFile(sLogname, sContent);

		KHTTPLogParser Log(sLogname);
		// should skip the garbage lines and find the valid entry
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.sRemoteIP == "192.168.1.100" );
	}

	SECTION("iterate all entries")
	{
		auto sLogname = kFormat("{}/parser-iterate.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		KHTTPLogParser Log(sLogname);

		std::size_t iCount = 0;

		for (;;)
		{
			auto log = Log.Next();

			if (!log.IsValid())
			{
				break;
			}

			++iCount;
		}

		CHECK ( iCount == 3 );
	}

	SECTION("constructor from stream with StartDate")
	{
		auto sLogname = kFormat("{}/parser-ctor-stream.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n");
		KInFile File(sLogname);

		KHTTPLogParser Log(File, KUnixTime("2025-05-07 16:55:53"));
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.Timestamp == KUnixTime("2025-05-07 16:55:53") );
	}

	SECTION("OldestDate preserves StartDate filter")
	{
		auto sLogname = kFormat("{}/parser-oldest-filter.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		// open with StartDate that skips line1
		KHTTPLogParser Log(sLogname, KUnixTime("2025-05-07 16:55:53"));

		// OldestDate reads the absolute oldest entry
		CHECK ( Log.OldestDate() == KUnixTime("2025-05-07 16:55:52") );

		// but the StartDate filter is preserved, so Next() still skips line1
		auto log = Log.Next();
		CHECK ( log.IsValid() );
		CHECK ( log.Timestamp == KUnixTime("2025-05-07 16:55:53") );
	}

	SECTION("NewestDate preserves position for continued iteration")
	{
		auto sLogname = kFormat("{}/parser-newest-continue.log", TempDir.Name());
		kWriteFile(sLogname, KString(sExtendedLine1) + "\n" + sExtendedLine2 + "\n" + sExtendedLine3 + "\n");

		KHTTPLogParser Log(sLogname);

		// read first entry
		auto log1 = Log.Next();
		CHECK ( log1.IsValid() );
		CHECK ( log1.Method.Serialize() == "POST" );

		// call NewestDate mid-iteration
		CHECK ( Log.NewestDate() == KUnixTime("2025-05-08 10:00:00") );

		// continue iteration - should get line2 next
		auto log2 = Log.Next();
		CHECK ( log2.IsValid() );
		CHECK ( log2.Method.Serialize() == "GET" );

		auto log3 = Log.Next();
		CHECK ( log3.IsValid() );
		CHECK ( log3.Method.Serialize() == "DELETE" );
	}
}

