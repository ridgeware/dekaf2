#include "catch.hpp"

#include <dekaf2/kwords.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KWords") {

	SECTION("KSimpleWordCounter")
	{
		struct test_t
		{
			KStringView sInput;
			size_t iCount;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few words", 3 },
			{ "a<>tad.@more?7ee1", 4 },
			{ "   , ", 0 },
			{ "", 0 }
		};

		for (auto& it : stest)
		{
			KSimpleWordCounter Words(it.sInput);
			INFO (it.sInput);
			CHECK ( it.iCount == Words->size() );
		}
		{
			KSimpleWordCounter A("one two three");
			KSimpleWordCounter B("four five six");
			A += B;
			CHECK ( A->size() == 6 );
		}
	}

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

	SECTION("KSimpleHTMLWordCounter")
	{
		struct test_t
		{
			KStringView sInput;
			size_t iCount;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few words", 3 },
			{ "a<a href = \"test\">tad<b>.@more</b>?7ee1", 4 },
			{ "a<sCript> some stuff here, x<a>c + \"</a>\" </scrIpt><a href = \"test\">tad<b>.@more</b>?7ee1", 4 },
			{ "Hello <span attr=\"something\">World.</span>  It is 'cold' outside.", 6 },
			{ "   , ", 0 },
			{ "", 0 }
		};

		for (auto& it : stest)
		{
			KSimpleHTMLWordCounter Words(it.sInput);

			INFO (it.sInput);
			CHECK ( it.iCount == Words->size() );
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
			{ "a<sCript> some stuff here, x<a>c + \"</a>\" </scrIpt><a href = \"test\">tad<b>.@more</b>?7ee1",
				{
					{ "a", "" },
					{ "tad", "<sCript> some stuff here, x<a>c + \"</a>\" </scrIpt><a href = \"test\">" },
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

	SECTION("KNormalizingHTMLSkeletonWords")
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
					{ "It", ".</span> " },
					{ "is", " " },
					{ "cold", " '" },
					{ "outside", "' "},
					{ "", "." }
				}
			},
			{ "a \n  few \t  words",
				{
					{ "a", ""},
					{"few", " "},
					{"words", " "}
				}
			},
			{ "a  <a href = \"test\">tad<b>.  @more </b> ?7ee1",
				{
					{ "a", "" },
					{ "tad", " <a href = \"test\">" },
					{ "more", "<b>. @" },
					{ "7ee1", " </b> ?" }
				}
			},
			{ "a  <sCript> some stuff here,  x<a>c + \"</a>\" </scrIpt> <a href = \"test\">tad<b>.  @more </b> ?7ee1",
				{
					{ "a", "" },
					{ "tad", " <sCript> some stuff here,  x<a>c + \"</a>\" </scrIpt> <a href = \"test\">" },
					{ "more", "<b>. @" },
					{ "7ee1", " </b> ?" }
				}
			},
			{ "Hello  \t\t  <span attr=\"something\">   World \r\n .  </span>\t  It\t  is  '  cold  '  outside . ",
				{
					{ "Hello", "" },
					{ "World", " <span attr=\"something\"> " },
					{ "It", " . </span> " },
					{ "is", " " },
					{ "cold", " ' " },
					{ "outside", " ' "},
					{ "", " . " }
				}
			},
			{ "   , ",
				{
					{ "", " , "}
				}
			},
			{ "",
				{
				}
			}
		};

		for (auto& it : stest)
		{
			KNormalizingHTMLSkeletonWords Words(it.sInput);

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

	SECTION("KSimpleHTMLSkeletonWords, operator=()")
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
			KSimpleHTMLSkeletonWords Words;
			Words = it.sInput;
			// repeat the assignment - should remove the first set of results
			Words = it.sInput;

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

	SECTION("KSimpleHTMLSkeletonWords, operator=+(KStringView)")
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
			KSimpleHTMLSkeletonWords Words;
			Words = it.sInput;
			// add again - should duplicate the first set of results
			Words += it.sInput;

			CHECK ( it.sOutput.size() * 2 == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit.first  == sit->first  );
				CHECK ( wit.second == sit->second );
				++sit;
				if (sit == it.sOutput.end())
				{
					sit = it.sOutput.begin();
				}
			}

		}
	}

	SECTION("KSimpleHTMLSkeletonWords, operator=+(KWords)")
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
			KSimpleHTMLSkeletonWords Words1;
			Words1 += it.sInput;
			KSimpleHTMLSkeletonWords Words = Words1;
			// add again - should duplicate the first set of results
			Words += Words1;

			CHECK ( it.sOutput.size() * 2 == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit.first  == sit->first  );
				CHECK ( wit.second == sit->second );
				++sit;
				if (sit == it.sOutput.end())
				{
					sit = it.sOutput.begin();
				}
			}

		}
	}


}

