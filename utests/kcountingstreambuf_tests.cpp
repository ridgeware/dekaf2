#include "catch.hpp"

#include <dekaf2/io/streams/kcountingstreambuf.h>
#include <dekaf2/io/streams/kinstringstream.h>
#include <dekaf2/io/streams/koutstringstream.h>

using namespace dekaf2;

namespace {

// every byte value, twice, so that 0xff is followed by more input
KString AllBytes()
{
	KString sBytes;
	for (int iRound = 0; iRound < 2; ++iRound)
	{
		for (int ch = 0; ch < 256; ++ch)
		{
			sBytes += static_cast<char>(ch);
		}
	}
	return sBytes;
}

} // namespace

TEST_CASE("KCountingStreambuf")
{
	SECTION("KCountingInputStreambuf with 0xff bytes")
	{
		auto sInput = AllBytes();

		{
			// char by char, through underflow()
			KInStringStream iss(sInput);
			KCountingInputStreamBuf Counter(iss.istream());
			KString sOutput;
			for (int ch; (ch = iss.istream().get()) != std::char_traits<char>::eof(); )
			{
				sOutput += static_cast<char>(ch);
			}
			CHECK ( sOutput.size() == sInput.size() );
			CHECK ( sOutput == sInput );
			CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
		}
		{
			// copied as a whole, as KOutHTTPFilter::Write copies a stream of unknown length
			KInStringStream iss(sInput);
			KCountingInputStreamBuf Counter(iss.istream());
			KString sOutput;
			KOutStringStream oss(sOutput);
			oss.ostream() << iss.istream().rdbuf();
			oss.Flush();
			CHECK ( sOutput.size() == sInput.size() );
			CHECK ( sOutput == sInput );
			CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
		}
	}

	SECTION("KCountingInputStreambuf with a 0xff byte inside a line read with getline")
	{
		KString sInput = "X-Name: a\xff" "b\r\nnext\r\n";
		KInStringStream iss(sInput);
		KCountingInputStreamBuf Counter(iss.istream());
		std::string sLine;
		REQUIRE ( std::getline(iss.istream(), sLine) );
		CHECK ( sLine == "X-Name: a\xff" "b\r" );
		REQUIRE ( std::getline(iss.istream(), sLine) );
		CHECK ( sLine == "next\r" );
		CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
	}

	SECTION("KCountingInputStreambuf with 0xff alone and at the end")
	{
		for (KStringView sInput : { KStringView("\xff"), KStringView("ab\xff"), KStringView("\xff\xff\xff") })
		{
			KInStringStream iss(sInput);
			KCountingInputStreamBuf Counter(iss.istream());
			KString sOutput;
			KOutStringStream oss(sOutput);
			oss.ostream() << iss.istream().rdbuf();
			oss.Flush();
			CHECK ( sOutput == sInput );
			CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
		}
	}

	SECTION("KCountingInputStreambuf with read()")
	{
		auto sInput = AllBytes();
		KInStringStream iss(sInput);
		KCountingInputStreamBuf Counter(iss.istream());
		KString sOutput(10, '\0');
		iss.istream().read(&sOutput[0], 10);
		CHECK ( iss.istream().gcount() == 10 );
		CHECK ( sOutput == sInput.substr(0, 10) );
		CHECK ( Counter.Count() == 10 );
	}

	SECTION("KCountingInputStreambuf with read() after peek()")
	{
		auto sInput = AllBytes();
		KInStringStream iss(sInput);
		KCountingInputStreamBuf Counter(iss.istream());
		CHECK ( iss.istream().peek() == 0 );
		KString sOutput(10, '\0');
		iss.istream().read(&sOutput[0], 10);
		CHECK ( iss.istream().gcount() == 10 );
		CHECK ( sOutput == sInput.substr(0, 10) );
		CHECK ( Counter.Count() == 10 );
	}

	SECTION("KCountingInputStreambuf")
	{
		KStringView sInput = "abcdefghijklmnopqrstuvwxyz01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

		KInStringStream iss(sInput);
		KString sOutput;

		KCountingInputStreamBuf Counter(iss.istream());

		sOutput += iss.Read();
		sOutput += iss.ReadRemaining();

		CHECK ( sInput == sOutput );
		CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
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
		CHECK ( static_cast<KStringView::size_type>(Counter.Count()) == sInput.size() );
	}

}
