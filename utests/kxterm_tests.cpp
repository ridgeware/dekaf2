#include "catch.hpp"
#include <dekaf2/util/cli/kxterm.h>

using namespace dekaf2;

TEST_CASE("KXTermCodes")
{
	SECTION("Named foreground colors")
	{
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Black)   == "\033[30m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Red)     == "\033[31m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Green)   == "\033[32m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Yellow)  == "\033[33m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Blue)    == "\033[34m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Magenta) == "\033[35m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Cyan)    == "\033[36m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::White)   == "\033[37m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::Default) == "\033[39m" );
	}

	SECTION("Named background colors")
	{
		CHECK ( KXTermCodes::BGColor(KXTermCodes::Black)   == "\033[40m" );
		CHECK ( KXTermCodes::BGColor(KXTermCodes::Red)     == "\033[41m" );
		CHECK ( KXTermCodes::BGColor(KXTermCodes::Green)   == "\033[42m" );
		CHECK ( KXTermCodes::BGColor(KXTermCodes::White)   == "\033[47m" );
		CHECK ( KXTermCodes::BGColor(KXTermCodes::Default) == "\033[49m" );
	}

	SECTION("Bright foreground colors")
	{
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightBlack)   == "\033[1;30m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightRed)     == "\033[1;31m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightGreen)   == "\033[1;32m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightYellow)  == "\033[1;33m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightBlue)    == "\033[1;34m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightMagenta) == "\033[1;35m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightCyan)    == "\033[1;36m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::BrightWhite)   == "\033[1;37m" );
	}

	SECTION("Bright background colors")
	{
		CHECK ( KXTermCodes::BGColor(KXTermCodes::BrightBlack)   == "\033[1;40m" );
		CHECK ( KXTermCodes::BGColor(KXTermCodes::BrightWhite)   == "\033[1;47m" );
	}

	SECTION("Combined named colors")
	{
		CHECK ( KXTermCodes::Color(KXTermCodes::Red, KXTermCodes::Blue) == "\033[31;44m" );
		CHECK ( KXTermCodes::Color(KXTermCodes::Default, KXTermCodes::Default) == "\033[39;49m" );
		CHECK ( KXTermCodes::Color(KXTermCodes::BrightGreen, KXTermCodes::Black) == "\033[1;32;40m" );
	}

	SECTION("RGB foreground color")
	{
		CHECK ( KXTermCodes::FGColor(KXTermCodes::RGB(255, 128, 0)) == "\033[38;2;255;128;0m" );
		CHECK ( KXTermCodes::FGColor(KXTermCodes::RGB(0, 0, 0))     == "\033[38;2;0;0;0m"     );
	}

	SECTION("RGB background color")
	{
		CHECK ( KXTermCodes::BGColor(KXTermCodes::RGB(0, 255, 0))   == "\033[48;2;0;255;0m"   );
	}

	SECTION("RGB combined color")
	{
		auto sResult = KXTermCodes::Color(KXTermCodes::RGB(255, 0, 0), KXTermCodes::RGB(0, 0, 255));
		CHECK ( sResult == "\033[38;2;255;0;0m\033[48;2;0;0;255m" );
	}

	SECTION("RGB from hex string")
	{
		KXTermCodes::RGB rgb1("AABB00");
		CHECK ( rgb1.Red   == 0xAA );
		CHECK ( rgb1.Green == 0xBB );
		CHECK ( rgb1.Blue  == 0x00 );

		KXTermCodes::RGB rgb2("#FF8020");
		CHECK ( rgb2.Red   == 0xFF );
		CHECK ( rgb2.Green == 0x80 );
		CHECK ( rgb2.Blue  == 0x20 );

		KXTermCodes::RGB rgb3("aabbcc");
		CHECK ( rgb3.Red   == 0xAA );
		CHECK ( rgb3.Green == 0xBB );
		CHECK ( rgb3.Blue  == 0xCC );
	}

	SECTION("RGB from invalid hex string")
	{
		KXTermCodes::RGB rgb1("short");
		CHECK ( rgb1.Red   == 0 );
		CHECK ( rgb1.Green == 0 );
		CHECK ( rgb1.Blue  == 0 );

		KXTermCodes::RGB rgb2("");
		CHECK ( rgb2.Red   == 0 );
		CHECK ( rgb2.Green == 0 );
		CHECK ( rgb2.Blue  == 0 );
	}
}
