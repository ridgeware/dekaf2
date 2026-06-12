#include "catch.hpp"

#include <dekaf2/core/strings/kwords.h>
#include <dekaf2/core/strings/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KWords") {

	SECTION("KSimpleSpacedWordCounter")
	{
		struct test_t
		{
			KStringView sInput;
			size_t iCount;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few_words", 2 },
			{ "a few words", 3 },
			{ "a<>tad.@more?7ee1", 1 },
			{ "   , ", 1 },
			{ "", 0 }
		};

		for (auto& it : stest)
		{
			KSimpleSpacedWordCounter Words(it.sInput);
			INFO (it.sInput);
			CHECK ( it.iCount == Words->size() );
		}
		{
			KSimpleSpacedWordCounter A("one two_three");
			KSimpleSpacedWordCounter B("four five six");
			A += B;
			CHECK ( A->size() == 5 );
		}
	}

	SECTION("KSimpleSpacedWords")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringView> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "a few_words",
				{
					"a",
					"few_words"
				}
			},
			{ "a few words",
				{
					"a",
					"few",
					"words"
				}
			},
			{ "a<>tad.@more?7ee1",
				{
					"a<>tad.@more?7ee1"
				}
			},
			{ "   , ",
				{
					","
				}
			},
			{ "",
				{
				}
			}
		};

		for (auto& it : stest)
		{
			KSimpleSpacedWords Words(it.sInput);

			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				CHECK ( wit  == *sit  );
				++sit;
			}
		}
	}

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
			{ "a few_words", 3 },
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
			KSimpleWordCounter A("one two_three");
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
			{ "a few_words",
				{
					"a",
					"few",
					"words"
				}
			},
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

			INFO (it.sInput);
			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				if (sit == it.sOutput.end())
				{
					CHECK ( wit == "" );
				}
				else
				{
					CHECK ( wit  == *sit  );
					++sit;
				}
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

	SECTION("KSimpleWordCounter with combining marks")
	{
		// combining marks (Mn, Mc, Me) continue a word, but never start one -
		// otherwise Thai (Mn vowels/tones), Indic scripts (Mc/Mn matras, virama),
		// vocalized Arabic (Mn harakat) and NFD input get shredded at every mark

		struct test_t
		{
			KStringView sInput;
			size_t iCount;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "น้ำ", 1 },               // Thai "water": U+0E19, U+0E49 (Mn), U+0E33
			{ "หิว มาก", 2 },           // Thai with space: U+0E34 (Mn) inside first word
			{ "हिन्दी भाषा", 2 },          // Hindi: U+093F/U+0940 (Mc), U+094D (Mn) inside words
			{ "cafe\xcc\x81 bar", 2 },  // NFD Latin: e + U+0301 (Mn)
			{ "مُحَمَّد", 1 },              // vocalized Arabic: U+064E/U+064F/U+0651 (Mn)
			{ " \xcc\x81 ", 0 },        // orphaned mark stays in the skeleton
			{ "\xcc\x81", 0 }           // orphaned mark only
		};

		for (auto& it : stest)
		{
			KSimpleWordCounter Words(it.sInput);
			INFO (it.sInput);
			CHECK ( it.iCount == Words->size() );
		}
	}

	SECTION("KSimpleWords with combining marks")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringView> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "น้ำดื่ม",
				{
					"น้ำดื่ม"
				}
			},
			{ "हिन्दी भाषा",
				{
					"हिन्दी",
					"भाषा"
				}
			},
			{ "cafe\xcc\x81 bar",
				{
					"cafe\xcc\x81",
					"bar"
				}
			},
			{ "\xcc\x81" "abc",
				{
					"abc"
				}
			}
		};

		for (auto& it : stest)
		{
			KSimpleWords Words(it.sInput);

			INFO (it.sInput);
			CHECK ( it.sOutput.size() == Words->size() );

			auto sit = it.sOutput.begin();

			for (auto& wit : *Words)
			{
				if (sit == it.sOutput.end())
				{
					CHECK ( wit == "" );
				}
				else
				{
					CHECK ( wit  == *sit  );
					++sit;
				}
			}
		}
	}

	SECTION("KSimpleSkeletonWords with combining marks")
	{
		struct test_t
		{
			KStringView sInput;
			std::vector<KStringViewPair> sOutput;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "हिन्दी भाषा",
				{
					{ "हिन्दी", "" },
					{ "भाषा", " " }
				}
			},
			{ "a \xcc\x81" "xyz",
				{
					{ "a", "" },
					{ "xyz", " \xcc\x81" }
				}
			},
			{ "น้ำ.",
				{
					{ "น้ำ", "" },
					{ "", "." }
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

	SECTION("KSimpleHTMLWordCounter with combining marks")
	{
		struct test_t
		{
			KStringView sInput;
			size_t iCount;
		};

		// source, target
		std::vector<test_t> stest
		{
			{ "<p>हिन्दी</p> น้ำ", 2 },
			{ "cafe\xcc\x81<b>!</b>", 1 }
		};

		for (auto& it : stest)
		{
			KSimpleHTMLWordCounter Words(it.sInput);

			INFO (it.sInput);
			CHECK ( it.iCount == Words->size() );
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

