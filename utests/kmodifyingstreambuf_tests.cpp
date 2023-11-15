#include "catch.hpp"

#include <dekaf2/kmodifyingstreambuf.h>
#include <dekaf2/kinstringstream.h>
#include <dekaf2/koutstringstream.h>

using namespace dekaf2;

TEST_CASE("KModifyingStreambuf")
{
#if 0
	SECTION("KModifyingInputStreambuf")
	{
		KStringView sInput = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		KInStringStream iss(sInput);
		KString sOutput;

		KCountingInputStreamBuf Modifier(iss.InStream());
		Modifier.Replace("jkl", "+++replaced+++");

		sOutput += iss.Read();
		sOutput += iss.ReadRemaining();

		CHECK ( sInput == sOutput );
		CHECK ( Counter.Count() == sInput.size() );
	}
#endif

	SECTION("KModifyingOutputStreambuf")
	{
		KStringView sInput    = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZjk";
		KStringView sExpected = "abcdefghi+++replaced+++mnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZjk";

		KString sOutput;
		KOutStringStream oss(sOutput);

		{
			KModifyingOutputStreamBuf Modifier(oss.ostream());
			Modifier.Replace("jkl", "+++replaced+++");

			oss.Write(sInput.front());
			oss.Write(sInput.data() + 1, sInput.size() - 1);
		}

		CHECK ( sExpected == sOutput );
	}

	SECTION("KModifyingOutputStreambuf single chars")
	{
		KStringView sInput    = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZjk";
		KStringView sExpected = "abcdefghi+++replaced+++mnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZjk";

		KString sOutput;
		KOutStringStream oss(sOutput);

		{
			KModifyingOutputStreamBuf Modifier(oss.ostream());
			Modifier.Replace("jkl", "+++replaced+++");
			
			for (auto ch : sInput)
			{
				oss.Write(ch);
			}
		}

		CHECK ( sExpected == sOutput );
	}

	SECTION("KModifyingOutputStreambuf StartOfLine")
	{
		KStringView sInput    = "abcdefghij\nklmnopqrstuvwxyz\n01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
		KStringView sExpected = "|abcdefghij\n|klmnopqrstuvwxyz\n|01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";

		KString sOutput;
		KOutStringStream oss(sOutput);

		KModifyingOutputStreamBuf Modifier(oss.ostream());
		Modifier.Replace("", "|");

		oss.Write(sInput.front());
		oss.Write(sInput.data() + 1, sInput.size() - 1);

		CHECK ( sExpected == sOutput );
	}

}
