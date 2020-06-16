#include "catch.hpp"

#ifdef DEKAF2_HAS_LIBZIP

#include <dekaf2/kzip.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

TEST_CASE("KZip") {

	SECTION("Read")
	{
/*		KString sFileName;
		KTempDir Directory(false); // TODO remove false

		KZip Zip(sFileName);
		for (const auto& File : Zip)
		{
			Zip.Read(kFormat("{}/{}", Directory.Name(), File.sName), File);
		}
*/
	}

}

#endif
