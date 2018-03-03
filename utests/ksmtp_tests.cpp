#include "catch.hpp"

#include <dekaf2/ksmtp.h>

using namespace dekaf2;

TEST_CASE("KSMTP") {

	SECTION("KSMTP dotted escapes")
	{
		KMail mail;
		mail.Message(".Hello World");
		mail += "\r\n.\r\nHello";
		mail += "\n.\nHello";
		mail += "\n.\n";
		mail += "\n.";
		mail += "\r\n.";
		mail += "\r\n";
		mail += ".";
		mail.Append("Hello again");
		CHECK ( mail.Message() == "..Hello World\r\n..\r\nHello\n..\nHello\n..\n\n..\r\n..\r\n..Hello again" );
	}

}
