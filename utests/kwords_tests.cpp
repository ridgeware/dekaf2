#include "catch.hpp"

#include <dekaf2/kwords.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KWords") {

	SECTION("KSimpleWords")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringView> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few words",
				{
					"a",
					"few",
					"words"
				}
			},
			{ "a<>tad.@more?7ee1",
				{
					"a",
					"tad",
					"more",
					"7ee1"
				}
			},
			{ "   , ",
				{
				}
			},
			{ "",
				{
				}
			}
		};

		for (auto& it : stest)
		{
			KSimpleWords Words(it.sInput);

			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit  == *sit  );
				++sit;
			}

		}
	}

	SECTION("KSimpleSkeletonWords")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringViewPair> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few words",
				{
					{ "a", ""},
					{"few", " "},
					{"words", " "}
				}
			},
			{ "a<>tad.@more?7ee1",
				{
					{ "a", "" },
					{ "tad", "<>" },
					{ "more", ".@" },
					{ "7ee1", "?" }
				}
			},
			{ "   , ",
				{
					{ "", "   , "}
				}
			},
			{ "",
				{
				}
			}
		};

		for (auto& it : stest)
		{
			KSimpleSkeletonWords Words(it.sInput);

			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit.first  == sit->first  );
				CHECK ( wit.second == sit->second );
				++sit;
			}

		}
	}

	SECTION("KSimpleHTMLSkeletonWords")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringViewPair> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few words",
				{
					{ "a", ""},
					{"few", " "},
					{"words", " "}
				}
			},
			{ "a<a href = \"test\">tad<b>.@more</b>?7ee1",
				{
					{ "a", "" },
					{ "tad", "<a href = \"test\">" },
					{ "more", "<b>.@" },
					{ "7ee1", "</b>?" }
				}
			},
			{ "Hello <span attr=\"something\">World.</span>  It is 'cold' outside.",
				{
					{ "Hello", "" },
					{ "World", " <span attr=\"something\">" },
					{ "It", ".</span>  " },
					{ "is", " " },
					{ "cold", " '" },
					{ "outside", "' "},
					{ "", "." }
				}
			},
			{ "   , ",
				{
					{ "", "   , "}
				}
			},
			{ "",
				{
				}
			}
		};

		for (auto& it : stest)
		{
			KSimpleHTMLSkeletonWords Words(it.sInput);

			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit.first  == sit->first  );
				CHECK ( wit.second == sit->second );
				++sit;
			}

		}
	}


}

