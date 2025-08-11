#include "catch.hpp"
#include <dekaf2/kuuid.h>

#if DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringstream.h>

using namespace dekaf2;


TEST_CASE("KUUID")
{
	SECTION("Creation")
	{
		KUUID uuid;
		CHECK ( uuid.empty() == false );
		CHECK ( uuid.ToString().size() == 36 );
//		CHECK ( uuid.ToString() == "" );

		uuid = KUUID::Create(KUUID::MACTime);
		CHECK ( uuid.empty() == false );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid != KStringView("") );

		uuid = KUUID::Create(KUUID::Null);
		CHECK ( uuid.empty() == true );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid == "00000000-0000-0000-0000-000000000000" );

		uuid = KUUID::Parse("697a9b88-a9ce-4383-b9d2-1db91a60758f");
		CHECK ( uuid.empty() == false );
		CHECK ( uuid == "697a9b88-a9ce-4383-b9d2-1db91a60758f");
		// this is actually an invalid variant 4 UUID - but the linux
		// implementation parses it anyways
		auto uuid2 = KUUID::Parse("697a9b88-a9ce-4383-19d2-1db91a60758f");
		CHECK ( uuid2.empty() == false );
		CHECK ( uuid2 == "697a9b88-a9ce-4383-19d2-1db91a60758f" );

		uuid = "85841df6-12d2-43a1-9866-8287eba94deb";
		CHECK ( uuid.empty() == false );
		CHECK ( uuid == "85841df6-12d2-43a1-9866-8287eba94deb" );
		CHECK ( uuid != uuid2 );

		uuid.clear();
		CHECK ( uuid.empty() == true );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid == "00000000-0000-0000-0000-000000000000" );
	}

	SECTION("formatting (fmt)")
	{
		KUUID uuid1;
		auto uuid2 = uuid1;
		auto s = kFormat("{}", uuid1);
		CHECK ( s == uuid2.ToString() );
	}

	SECTION("formatting (std::ostream)")
	{
		KUUID uuid1;
		auto uuid2 = uuid1;
		KString s;
		KOutStringStream oss(s);
		oss << uuid1;
		CHECK ( s == uuid2.ToString() );
	}

}

#endif // DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS
