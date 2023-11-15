#include "catch.hpp"

#include <dekaf2/kcountingstreambuf.h>
#include <dekaf2/kinstringstream.h>
#include <dekaf2/koutstringstream.h>

using namespace dekaf2;

TEST_CASE("KCountingStreambuf")
{
	SECTION("KCountingInputStreambuf")
	{
		KStringView sInput = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		KInStringStream iss(sInput);
		KString sOutput;

		KCountingInputStreamBuf Counter(iss.istream());

		sOutput += iss.Read();
		sOutput += iss.ReadRemaining();

		CHECK ( sInput == sOutput );
		CHECK ( Counter.Count() == sInput.size() );
	}

	SECTION("KCountingOutputStreambuf")
	{
		KStringView sInput = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		KString sOutput;
		KOutStringStream oss(sOutput);

		KCountingOutputStreamBuf Counter(oss.ostream());

		oss.Write(sInput.front());
		oss.Write(sInput.data() + 1, sInput.size() - 1);

		CHECK ( sInput == sOutput );
		CHECK ( Counter.Count() == sInput.size() );
	}

}
