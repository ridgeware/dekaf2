#include "catch.hpp"

#include <dekaf2/util/mail/ksmtp.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

TEST_CASE("KSMTP")
{
	SECTION("STARTTLS on the submission port")
	{
		KSMTP SMTP;

		if (!SMTP.Connect("smtps://smtp.gmail.com:587"))
		{
			WARN("cannot reach smtp.gmail.com:587: " << SMTP.GetLastError());
			return;
		}

		CHECK ( SMTP.Good() );
	}

	SECTION("implicit TLS on port 465")
	{
		KSMTP SMTP;

		if (!SMTP.Connect("smtps://smtp.gmail.com:465"))
		{
			WARN("cannot reach smtp.gmail.com:465: " << SMTP.GetLastError());
			return;
		}

		CHECK ( SMTP.Good() );
	}

	SECTION("implicit TLS on port 465 without a scheme")
	{
		KSMTP SMTP;

		if (!SMTP.Connect("smtp.gmail.com:465"))
		{
			WARN("cannot reach smtp.gmail.com:465: " << SMTP.GetLastError());
			return;
		}

		CHECK ( SMTP.Good() );
	}
}
