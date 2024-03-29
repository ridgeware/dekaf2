#include "catch.hpp"

#include <dekaf2/kstreambuf.h>
#include <dekaf2/kcountingstreambuf.h>

using namespace dekaf2;

TEST_CASE("KStreamBuf") {

	SECTION("KNullBuf")
	{
		KNullStreamBuf NullBuf;
		std::istream MyNullStream(&NullBuf);
		KInStream iss(MyNullStream);

		KString sOutput;
		// the following will trip the limit warning for endless input streams:
		// kAppendAllUnseekable(): stepped over limit of 1024 MB for non-seekable input stream - aborted reading
		iss.ReadRemaining(sOutput);

		KString sNull(1, '\0');
		CHECK ( sOutput.find_first_not_of(sNull) == npos );
	}

	SECTION("KNullBuf with KCountingInputStreamBuf")
	{
		KNullStreamBuf NullBuf;
		std::istream MyNullStream(&NullBuf);
		KInStream iss(MyNullStream);
		KCountingInputStreamBuf Counter(iss);

		KString sOutput;
		iss.Read(sOutput, 10*1024*1024);

		KString sNull(1, '\0');
		CHECK ( sOutput.find_first_not_of(sNull) == npos );
		CHECK ( Counter.Count() == 10*1024*1024 );
	}
}

