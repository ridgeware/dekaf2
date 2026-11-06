#include "catch.hpp"

#include <dekaf2/io/streams/kstreambufadaptor.h>
#include <dekaf2/io/streams/kinstringstream.h>

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

// an adaptor that passes each char unchanged and leaves Inspect(char*, size) to the base class
class PassThroughStreamBuf : public KInStreamBufAdaptor
{
public:
	using KInStreamBufAdaptor::KInStreamBufAdaptor;
protected:
	virtual int_type Inspect(char_type ch) override { return traits_type::to_int_type(ch); }
};

// flips the case of ASCII letters, so the base class Inspect(char*, size) must write the result back
class FlipCaseStreamBuf : public KInStreamBufAdaptor
{
public:
	using KInStreamBufAdaptor::KInStreamBufAdaptor;
protected:
	virtual int_type Inspect(char_type ch) override { return traits_type::to_int_type(static_cast<char_type>(ch ^ 0x20)); }
};

// ends the input at the first '|'
class StopAtBarStreamBuf : public KInStreamBufAdaptor
{
public:
	using KInStreamBufAdaptor::KInStreamBufAdaptor;
protected:
	virtual int_type Inspect(char_type ch) override { return ch == '|' ? traits_type::eof() : traits_type::to_int_type(ch); }
};

} // namespace

TEST_CASE("KInStreamBufAdaptor")
{
	SECTION("Inspect(char*, size) with 0xff bytes")
	{
		auto sInput = AllBytes();
		KInStringStream iss(sInput);
		PassThroughStreamBuf PassThrough(iss.istream());
		KString sOutput(sInput.size(), '\0');
		// read() goes through xsgetn() and the base class Inspect(char*, size)
		iss.istream().read(&sOutput[0], static_cast<std::streamsize>(sOutput.size()));
		CHECK ( iss.istream().gcount() == static_cast<std::streamsize>(sInput.size()) );
		CHECK ( sOutput == sInput );
	}

	SECTION("Inspect(char*, size) writes the inspected chars back")
	{
		KString sInput = "abcdef";
		KInStringStream iss(sInput);
		FlipCaseStreamBuf Flip(iss.istream());
		KString sOutput(sInput.size(), '\0');
		iss.istream().read(&sOutput[0], static_cast<std::streamsize>(sOutput.size()));
		CHECK ( sOutput == "ABCDEF" );
	}

	SECTION("Inspect(char*, size) stops at eof")
	{
		KString sInput = "abc|def";
		KInStringStream iss(sInput);
		StopAtBarStreamBuf Stop(iss.istream());
		KString sOutput(sInput.size(), '\0');
		iss.istream().read(&sOutput[0], static_cast<std::streamsize>(sOutput.size()));
		CHECK ( iss.istream().gcount() == 3 );
		CHECK ( KStringView(sOutput).substr(0, 3) == "abc" );
	}

	SECTION("read() after peek() inspects every char once")
	{
		KString sInput = "abcdef";
		KInStringStream iss(sInput);
		FlipCaseStreamBuf Flip(iss.istream());
		CHECK ( iss.istream().peek() == 'A' );
		KString sOutput(sInput.size(), '\0');
		iss.istream().read(&sOutput[0], static_cast<std::streamsize>(sOutput.size()));
		CHECK ( iss.istream().gcount() == static_cast<std::streamsize>(sInput.size()) );
		CHECK ( sOutput == "ABCDEF" );
	}
}
