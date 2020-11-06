#include "catch.hpp"
#include <dekaf2/kcsv.h>
#include <dekaf2/kstack.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCSV")
{
	SECTION("Vectors")
	{
		std::vector<std::pair<std::vector<KString>, KStringView>> tests
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
        
        SECTION("KInCSV")
        {
            KString sInputBuffer;
            
            for (const auto& test : tests)
            {
                KInCSV CSV(test.second);
                CHECK ( CSV.Read() == test.first );
                // prepare the next test
                sInputBuffer += test.second;
            }
            
            {
                auto test = tests.begin();
                // the first test is empty, we will not see it in the buffer so skip it
                ++test;
                for (const auto& row : KInCSV(sInputBuffer))
                {
                    CHECK ( test != tests.end() );
                    if (test != tests.end())
                    {
                        CHECK ( row == test->first );
                        ++test;
                    }
                }
                CHECK ( test == tests.end() );
            }

            {
                auto test = tests.begin();
                // the first test is empty, we will not see it in the buffer so skip it
                ++test;
                KInStringStream iss(sInputBuffer);
                for (const auto& row : KInCSV(iss))
                {
                    CHECK ( test != tests.end() );
                    if (test != tests.end())
                    {
                        CHECK ( row == test->first );
                        ++test;
                    }
                }
                CHECK ( test == tests.end() );
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
        
        SECTION("KInCSV")
        {
            KString sInputBuffer;
            
            for (const auto& test : tests)
            {
                KInCSV<KStack<KString>> CSV(test.second);
                CHECK ( CSV.Read() == test.first );
                // prepare the next test
                sInputBuffer += test.second;
            }
            
            auto test = tests.begin();
            // the first test is empty, we will not see it in the buffer so skip it
            ++test;
            KInCSV<KStack<KString>> CSV(sInputBuffer);
            for (const auto& row : CSV)
            {
                CHECK ( test != tests.end() );
                if (test != tests.end())
                {
                    CHECK ( row == test->first );
                    ++test;
                }
            }
            CHECK ( test == tests.end() );
        }
	}
}
