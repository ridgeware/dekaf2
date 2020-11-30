#include "catch.hpp"
#include <dekaf2/kversion.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KVersion")
{
	SECTION("Parse")
	{
		KVersion Version { 1, 2, 3, 4 };
		CHECK ( Version.Serialize() == "1.2.3.4" );
		CHECK ( Version.Serialize(',') == "1,2,3,4" );

		Version = { 7, 2, 4 };
		CHECK ( Version.Serialize() == "7.2.4" );

		Version = KVersion("1.273.3.4986");
		CHECK ( Version.Serialize() == "1.273.3.4986" );

		Version = KVersion("1.273.3.4986"_ks);
		CHECK ( Version.Serialize() == "1.273.3.4986" );

		Version = KVersion("1.273.3.4986"_ksv);
		CHECK ( Version.Serialize() == "1.273.3.4986" );

		Version = KVersion("1 . 2 . 3 .4");
		CHECK ( Version.Serialize() == "1.2.3.4" );

		Version = KVersion("1.3a.7b");
		CHECK ( Version.Serialize() == "1.3.7" );

		Version.clear();
		CHECK ( Version.empty() );
		CHECK ( Version.size() == 0 );
		CHECK ( Version.Serialize() == "" );
	}

	SECTION("Comparison")
	{
		KVersion Version;
		CHECK ( Version.empty() );
		CHECK ( Version.size() == 0 );
		CHECK ( Version.Serialize() == "" );

		Version = KVersion("1.2.1");
		CHECK ( Version.size() == 3    );
		CHECK ( Version <  KVersion("1.3")       );
		CHECK ( Version >  KVersion("1.2")       );
		CHECK ( Version >  KVersion("1.2.0.100") );
		CHECK ( Version >= KVersion("1.2.1")     );
		CHECK ( Version >= KVersion("1.2.1.0.0") );
		CHECK ( Version == KVersion("1.2.1")     );
		CHECK ( Version == KVersion("1.2.1.0")   );
		CHECK ( Version != KVersion("1.2.1.1")   );
		CHECK ( Version == (KVersion { 1, 2, 1 }) );

		Version = KVersion("1.2.3.44");
		CHECK ( Version[0]     == 1  );
		CHECK ( Version[1]     == 2  );
		CHECK ( Version[2]     == 3  );
		CHECK ( Version[3]     == 44 );
		CHECK ( Version.size() == 4 );
		CHECK ( Version[10]    == 0  );
		CHECK ( Version.size() == 11 );
		CHECK ( Version.Serialize() == "1.2.3.44.0.0.0.0.0.0.0" );
		CHECK ( Version.Trim().Serialize() == "1.2.3.44" );

		Version = KVersion("0.0.0.0.0");
		CHECK ( Version == KVersion("0.0.0.0.0") );
		Version.Trim();
		CHECK ( Version == KVersion("") );
		CHECK ( Version.empty() );

		Version = KVersion("0.0.0.0.0.1");
		CHECK ( Version == KVersion("0.0.0.0.0.1") );
		Version.Trim();
		CHECK ( Version == KVersion("0.0.0.0.0.1") );

		CHECK ( Version > KVersion("") );
		CHECK ( Version > KVersion("0000") );
	}

}

