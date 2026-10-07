#include "catch.hpp"

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>

using namespace dekaf2;

int neverBeCalled()
{
	static int iCallCount = 0;
	return iCallCount++;
}


TEST_CASE("KLog credential redaction") {

	// The log writer strips bearer credentials before anything is written. This
	// exists because per-site discipline demonstrably fails: one sweep of a single
	// application found SIX places logging a live credential, including two that
	// wrote the CONFIGURED token on a failed auth -- so a deliberately wrong guess
	// made the server log the right answer. These tests drive the REAL logging
	// path rather than the helper directly, because what matters is that the text
	// is redacted by the time it reaches a file, not that a function works alone.

	KTempDir TmpDir;
	auto sLogFile = kFormat("{}/klog-redact.log", TmpDir.Name());

	auto Restore = KLog::getInstance().GetLevel();
	KLog::getInstance().SetDebugLog(sLogFile);
	KLog::getInstance().SetLevel(1);

	auto LoggedText = [&sLogFile](KStringView sWhat) -> KString
	{
		kDebug(1, "{}", sWhat);
		KString sOut;
		kReadAll(sLogFile, sOut);
		return sOut;
	};

	SECTION("a JWT is redacted")
	{
		// a real-shaped JWT: three base64url runs, dot separated
		auto sOut = LoggedText("got token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.dBjftJeZ4CVPmB92K27uhbUJU1p1r_wW1gFWFOEjXk");
		CHECK ( sOut.contains("eyJ...[redacted]") );
		CHECK ( !sOut.contains("dBjftJeZ4CVPmB92K27uhbUJU1p1r_wW1gFWFOEjXk") );
		CHECK ( sOut.contains("got token:") );   // the surrounding prose survives
	}

	SECTION("a Bearer value is redacted but the scheme is kept")
	{
		auto sOut = LoggedText("sent header Authorization: Bearer sk-live-abcdef1234567890 to upstream");
		CHECK ( !sOut.contains("sk-live-abcdef1234567890") );
		CHECK ( sOut.contains("Bearer [redacted]") );   // "which auth did it try" still answerable
		CHECK ( sOut.contains("to upstream") );
	}

	SECTION("prose mentioning eyJ is NOT mangled")
	{
		// the shape test matters: a line that merely says "eyJ" must survive, or
		// the net starts eating ordinary debugging output
		auto sOut = LoggedText("token should start with eyJ and it did not");
		CHECK ( sOut.contains("token should start with eyJ and it did not") );
		CHECK ( !sOut.contains("[redacted]") );
	}

	SECTION("an ordinary line is untouched")
	{
		auto sOut = LoggedText("content 1234-5678 moved to FINAL_EYE in 240ms");
		CHECK ( sOut.contains("content 1234-5678 moved to FINAL_EYE in 240ms") );
		CHECK ( !sOut.contains("[redacted]") );
	}

	KLog::getInstance().SetLevel(Restore);
	KLog::getInstance().SetDebugLog("");
}

TEST_CASE("KLog") {

	SECTION("KLog inline optimization")
	{
		kDebug(100, "this should never be output {}", neverBeCalled());
		CHECK ( neverBeCalled() == 0 );
	}

	SECTION("Program name")
	{
#ifdef DEKAF2_WITH_KLOG
		CHECK ( KLog::getInstance().GetName().empty() == false );
#else
		CHECK ( KLog::getInstance().GetName().empty() == true );
#endif
	}
}

#if defined(DEKAF2_IS_WINDOWS) && defined(DEKAF2_WITH_KLOG)

#include <dekaf2/core/logging/bits/klogwriter.h>
#include <dekaf2/core/strings/kutf.h>
#include <dekaf2/util/id/kuuid.h>
#include <windows.h>
#include <vector>

namespace {

// returns true if one of the latest events of the Application log comes from
// sSource, has the type iType, and contains sText
bool HasEvent(KStringView sSource, WORD iType, KStringView sText)
{
	HANDLE hLog = ::OpenEventLogW(nullptr, L"Application");

	if (!hLog)
	{
		return false;
	}

	auto wsSource = kutf::Convert<std::wstring>(sSource);
	auto wsText   = kutf::Convert<std::wstring>(sText);

	std::vector<BYTE> Buffer(64 * 1024);
	bool bFound { false };

	// the latest events first, in up to ten buffers
	for (int iRound = 0; iRound < 10 && !bFound; ++iRound)
	{
		DWORD iRead   { 0 };
		DWORD iNeeded { 0 };

		if (!::ReadEventLogW(hLog, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ, 0,
		                     Buffer.data(), static_cast<DWORD>(Buffer.size()), &iRead, &iNeeded))
		{
			if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || iNeeded <= Buffer.size())
			{
				break;
			}

			Buffer.resize(iNeeded);
			continue;
		}

		for (DWORD iPos = 0; iPos < iRead && !bFound; )
		{
			auto* pRecord = reinterpret_cast<const EVENTLOGRECORD*>(Buffer.data() + iPos);

			// the name of the source follows the record
			auto* sRecordSource = reinterpret_cast<const wchar_t*>(pRecord + 1);

			if (wsSource == sRecordSource && pRecord->EventType == iType && pRecord->NumStrings > 0)
			{
				auto* sString = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(pRecord) + pRecord->StringOffset);
				bFound = std::wstring(sString).find(wsText) != std::wstring::npos;
			}

			iPos += pRecord->Length;
		}
	}

	::CloseEventLog(hLog);

	return bFound;
}

} // end of anonymous namespace

TEST_CASE("KLog Event Log")
{
	SECTION("events of the system log")
	{
		// a marker for the events of this run
		KString sMarker = kFormat("dekaf2-utests-{}", KUUID().ToString());

		KLogSyslogWriter Writer;
		REQUIRE ( Writer.Good() );

		auto sSource = KLogSyslogWriter::SourceName();
		CHECK ( sSource.empty() == false );

		KString sWarning = kFormat("a warning {}", sMarker);
		CHECK ( Writer.Write(0, false, sWarning) );
		CHECK ( HasEvent(sSource, EVENTLOG_WARNING_TYPE, sWarning) );

		KString sError = kFormat("an error {}", sMarker);
		CHECK ( Writer.Write(-1, false, sError) );
		CHECK ( HasEvent(sSource, EVENTLOG_ERROR_TYPE, sError) );

		// one event for several lines, which Event Viewer breaks only at CRLF
		KString sLines = kFormat("first line\nsecond line {}\n", sMarker);
		CHECK ( Writer.Write(2, true, sLines) );
		CHECK ( HasEvent(sSource, EVENTLOG_INFORMATION_TYPE, kFormat("first line\r\nsecond line {}", sMarker)) );
	}

	SECTION("event source of a program")
	{
		CHECK ( KLogSyslogWriter::SourceName("C:\\Program Files\\app\\MyService.EXE") == "MyService" );
		CHECK ( KLogSyslogWriter::SourceName("C:/dir/tool.exe") == "tool" );
		CHECK ( KLogSyslogWriter::SourceName("tool") == "tool" );
	}
}

#endif
