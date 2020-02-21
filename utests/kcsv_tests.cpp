#include "catch.hpp"
#include <dekaf2/kcsv.h>
#include <dekaf2/kstack.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCSV")
{
	SECTION("Vectors")
	{
		std::vector<std::pair<KCSV::Record, KStringView>> tests
		{
			{
				{ },
				{ }
			},
			{
				{ "" },
				{ "\r\n" }
			},
			{
				{ " col 1 " },
				{ " col 1 \r\n" }
			},
			{
				{ " col 1 ", " col2 " },
				{ " col 1 , col2 \r\n" }
			},
			{
				{ "col\r\n1" },
				{ "\"col\r\n1\"\r\n" }
			},
			{
				{ "\"col3" },
				{ "\"\"\"col3\"\r\n" }
			},
			{
				{ "col1", "col2", "col3", "col4" },
				{ "col1,col2,col3,col4\r\n" }
			},
			{
				{ "col,1", ",col2", "\"col3", "co\"l4", "col5" },
				{ "\"col,1\",\",col2\",\"\"\"col3\",\"co\"\"l4\",col5\r\n" }
			}
		};

		SECTION("Write")
		{
			for (const auto& test : tests)
			{
				CHECK ( KCSV().Write(test.first) == test.second );
			}
		}

		SECTION("Read")
		{
			for (const auto& test : tests)
			{
				CHECK ( KCSV().Read(test.second) == test.first );
			}
		}
	}

	SECTION("KStack")
	{
		std::vector<std::pair<KStack<KString>, KStringView>> tests
		{
			{
				{ },
				{ }
			},
			{
				{ "" },
				{ "\r\n" }
			},
			{
				{ " col 1 " },
				{ " col 1 \r\n" }
			},
			{
				{ " col 1 ", " col2 " },
				{ " col 1 , col2 \r\n" }
			},
			{
				{ "col\r\n1" },
				{ "\"col\r\n1\"\r\n" }
			},
			{
				{ "\"col3" },
				{ "\"\"\"col3\"\r\n" }
			},
			{
				{ "col1", "col2", "col3", "col4" },
				{ "col1,col2,col3,col4\r\n" }
			},
			{
				{ "col,1", ",col2", "\"col3", "co\"l4", "col5" },
				{ "\"col,1\",\",col2\",\"\"\"col3\",\"co\"\"l4\",col5\r\n" }
			}
		};

		SECTION("Write")
		{
			for (const auto& test : tests)
			{
				CHECK ( KCSV().Write(test.first) == test.second );
			}
		}

		SECTION("Read")
		{
			for (const auto& test : tests)
			{
				CHECK ( KCSV().Read<KStack<KString>>(test.second) == test.first );
			}
		}
	}
}
