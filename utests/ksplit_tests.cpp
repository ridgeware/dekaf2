#include <vector>
#include "catch.hpp"
#include <dekaf2/dekaf2.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kprops.h>
#include <map>


using namespace dekaf2;
using std::vector;

//typedef KSplit<vector, KStringView> delim_t;

SCENARIO ( "ksplit unit tests on valid data" )
{
	static std::string sEmpty("");
	static std::string sComma(",");

	GIVEN ( "a valid simple comma-delimited string" )
	{
		WHEN ( "parse into a delimited list" )
		{
			KString abcde_normal{"a,b,c,d,e"};
			KStringView abcde = abcde_normal;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,    // where to put the parsed tokens
					abcde,          // the original string to parse
					sComma,         // the preferred delimiter
					sEmpty          // don't trim whitespace
			);                      // don't recognize escape character (default)
			THEN ( "examine the list for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				KStringView e = vsContainer[4];
				CHECK(iCount == 5);
				CHECK(a == "a");
				CHECK(b == "b");
				CHECK(c == "c");
				CHECK(d == "d");
				CHECK(e == "e");
			}
		}

		WHEN ( "parse into a delimited list from one escaped delimiter" )
		{
			KString abcde_escape{R"test(a,b,c,d\,e)test"};
			KStringView abcde = abcde_escape;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,
					abcde,
					",",
					"",     // don't trim whitespace
					'\\'    // use escape character
			);
			THEN ( "examine the escaped delimited list for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				CHECK(iCount == 4);
				CHECK(a == "a");
				CHECK(b == "b");
				CHECK(c == "c");
				CHECK(d == R"expect(d\,e)expect");
			}
		}

		WHEN ( "parse into delimited list from string with multiple escapes" )
		{
			//                             1  2   3    4
			KString abcde_escape{R"test(a\,b\\,c\\\,d\\\\,e)test"};
			KStringView abcde = abcde_escape;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,
					abcde,
					",",
					"",
					'\\'
			);
			THEN ( "examine multiple escape results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				CHECK(iCount == 3);
				CHECK(a == R"expect(a\,b\\)expect");
				CHECK(b == R"expect(c\\\,d\\\\)expect");
				CHECK(c == "e");
			}
		}

		WHEN ( "parse into delimited list, without trim, up to count limit" )
		{
			KString abcde_normal{" a , b , c , d , e "};
			KStringView abcde = abcde_normal;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,        // where to put the parsed tokens
					abcde,              // the original string to parse
					",",                // the preferred delimiter
					""                  // trim whitespace
			);                          // no escape character (default)
			THEN ( "examine count limited parse results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				KStringView e = vsContainer[4];
				CHECK(iCount == 5);
				CHECK(a == " a ");
				CHECK(b == " b ");
				CHECK(c == " c ");
				CHECK(d == " d ");
				CHECK(e == " e ");
			}
		}

		WHEN ( "parse into delimited list trimming all kinds of whitespace" )
		{
			KString abcde_normal{" a , b\t,\t\tc\r,\r\rd\n,\n\ne\n\n\n"};
			KStringView abcde = abcde_normal;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,        // where to put the parsed tokens
					abcde,              // the original string to parse
					",",                // the preferred delimiter
					" \t\r\n"           // trim whitespace
			);                          // no escape character (default)
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				KStringView e = vsContainer[4];
				CHECK(iCount == 5);
				CHECK(a == "a");
				CHECK(b == "b");
				CHECK(c == "c");
				CHECK(d == "d");
				CHECK(e == "e");
			}
		}

		WHEN ( "parse multi-delimited lists with variable length tokens trimming whitespace" )
		{
			KString abcde_normal{"a,bb;ccc:dddd|eeeee"};
			KStringView abcde = abcde_normal;
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(
					vsContainer,        // where to put the parsed tokens
					abcde,              // the original string to parse
					",|:;",             // the delimiters to use
					" \t\r\n"           // trim whitespace
			);                          // no escape character (default)
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				KStringView e = vsContainer[4];
				CHECK(iCount == 5);
				CHECK(a == "a");
				CHECK(b == "bb");
				CHECK(c == "ccc");
				CHECK(d == "dddd");
				CHECK(e == "eeeee");
			}
		}

		WHEN ( "parse string containing only trimmable characters" )
		{
			KStringView buffer{" \t\n\r "};
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(vsContainer, buffer);
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				CHECK(iCount == 1);
				CHECK(a == "");
			}
		}

		WHEN ( "parse string containing two fields of trimmable characters" )
		{
			KStringView buffer{" \t\n\r,\t\r "};
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(vsContainer, buffer);
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				CHECK(iCount == 2);
				CHECK(a == "");
				CHECK(b == "");
			}
		}

		WHEN ( "parse string with spaces as delimiter" )
		{
			KStringView buffer{"  word1  word2 word3     \t             word4 "};
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(vsContainer,
			                       buffer,
			                       " ",
			                       " \t");
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				CHECK(iCount == 4);
				CHECK(a == "word1");
				CHECK(b == "word2");
				CHECK(c == "word3");
				CHECK(d == "word4");
			}
		}

		WHEN ( "parse string with combining delimiters" )
		{
			KStringView buffer{".-word1--word2.word3-+.+.-\t.+-.+word4."};
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(vsContainer,
			                       buffer,
			                       ".-+",
			                       " \t\r\n.-+",
			                       0,
			                       true);
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[1];
				KStringView c = vsContainer[2];
				KStringView d = vsContainer[3];
				CHECK(iCount == 4);
				CHECK(a == "word1");
				CHECK(b == "word2");
				CHECK(c == "word3");
				CHECK(d == "word4");
			}
		}

		WHEN ( "parse web log" )
		{
			KStringView buffer{"66.249.71.172 - - [28/Mar/2010:04:02:12 -0500] \"GET /Support/SupportForums/tabid/213/view/topic/postid/1251/language/en-US/support.aspx HTTP/1.1\" 200 - \"-\" \"Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)\" pt.ncomputing.com 313 41094"};
			vector<KStringView> vsContainer;
			size_t iCount = kSplit(vsContainer,
			                       buffer,
			                       " ",
			                       "",
			                       0,
			                       false,
			                       true);
			THEN ( "examine results for defects" )
			{
				KStringView a = vsContainer[0];
				KStringView b = vsContainer[3];
				KStringView c = vsContainer[5];
				KStringView d = vsContainer[9];
				KStringView e = vsContainer[12];
				CHECK(iCount == 13);
				CHECK(a == "66.249.71.172");
				CHECK(b == "[28/Mar/2010:04:02:12");
				CHECK(c.StartsWith("GET /Support") == true);
				CHECK(c.EndsWith("HTTP/1.1") == true);
				CHECK(d.StartsWith("Mozilla") == true);
				CHECK(d.EndsWith("bot.html)") == true);
				CHECK(e == "41094");
			}
		}

	}
}

TEST_CASE("kSplitToPair")
{

	SECTION("split default")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key1=val1", "key1", "val1" },
			{ "key1=val1=val2", "key1", "val1=val2" },
			{ " \t key1    =\t  val1 \n", "key1", "val1" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1    =\t  val 1 \n", "key 1", "val 1" },
		};

		for (auto& it : stest)
		{
			INFO(it[0]);
			KStringViewPair svPair = kSplitToPair(it[0]);
			CHECK( svPair.first == it[1] );
			CHECK( svPair.second == it[2] );
		}
	}

	SECTION("split no trimming")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key1=val1", "key1", "val1" },
			{ "key1=val1=val2", "key1", "val1=val2" },
			{ " \t key1    =\t  val1 \n", " \t key1    ", "\t  val1 \n" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1    =\t  val 1 \n", " \t key 1    ", "\t  val 1 \n" },
		};

		for (auto& it : stest)
		{
			INFO(it[0]);
			KStringViewPair svPair = kSplitToPair(it[0],'=', "");
			CHECK( svPair.first == it[1] );
			CHECK( svPair.second == it[2] );
		}
	}

	SECTION("split escaped")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key-=1=val1", "key-=1", "val1" },
			{ "key1-=val1=val2", "key1-=val1", "val2" },
			{ " \t-= key1    =\t  val1 \n", "-= key1", "val1" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1-==\t  val 1 \n", "key 1-=", "val 1" },
		};

		for (auto& it : stest)
		{
			INFO(it[0]);
			KStringViewPair svPair = kSplitToPair(it[0],'=', " \t\r\n", '-');
			CHECK( svPair.first == it[1] );
			CHECK( svPair.second == it[2] );
		}
	}

}

TEST_CASE("kSplitPairs")
{

	SECTION("split default map")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key1=val1", "key1", "val1" },
			{ "key1=val1=val2", "key1", "val1=val2" },
			{ " \t key1    =\t  val1 \n", "key1", "val1" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1    =\t  val 1 \n", "key 1", "val 1" },
		};

		int ct = 0;
		for (auto& it : stest)
		{
			INFO(it[0]);
			std::map<KStringView, KStringView> aMap;
			auto iCount = kSplitPairs(aMap, it[0]);
			if (ct == 0) CHECK (iCount == 0);
			else
			{
				CHECK( iCount == 1 );
				CHECK( aMap.size() == 1 );
				CHECK( aMap.begin()->first == it[1] );
				CHECK( aMap.begin()->second == it[2] );
			}
			++ct;
		}
	}

	SECTION("split default kprops")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key1=val1", "key1", "val1" },
			{ "key1=val1=val2", "key1", "val1=val2" },
			{ " \t key1    =\t  val1 \n", "key1", "val1" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1    =\t  val 1 \n", "key 1", "val 1" },
		};

		int ct = 0;
		for (auto& it : stest)
		{
			INFO(it[0]);
			KProps<KStringView, KStringView> aMap;
			auto iCount = kSplitPairs(aMap, it[0]);
			if (ct == 0) CHECK (iCount == 0);
			else
			{
				CHECK( iCount == 1 );
				CHECK( aMap.size() == 1 );
				CHECK( aMap.begin()->first == it[1] );
				CHECK( aMap.begin()->second == it[2] );
			}
			++ct;
		}
	}

	SECTION("split no trimming")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key1=val1", "key1", "val1" },
			{ "key1=val1=val2", "key1", "val1=val2" },
			{ " \t key1    =\t  val1 \n", " \t key1    ", "\t  val1 \n" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1    =\t  val 1 \n", " \t key 1    ", "\t  val 1 \n" },
		};

		int ct = 0;
		for (auto& it : stest)
		{
			INFO(it[0]);
			KProps<KStringView, KStringView> aMap;
			auto iCount = kSplitPairs(aMap, it[0],'=', ",", "");
			if (ct == 0) CHECK (iCount == 0);
			else
			{
				CHECK( iCount == 1 );
				CHECK( aMap.size() == 1 );
				CHECK( aMap.begin()->first == it[1] );
				CHECK( aMap.begin()->second == it[2] );
			}
			++ct;
		}
	}

	SECTION("split escaped")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "", "", "" },
			{ "key-=1=val1", "key-=1", "val1" },
			{ "key1-=val1=val2", "key1-=val1", "val2" },
			{ " \t-= key1    =\t  val1 \n", "-= key1", "val1" },
			{ "key 1=val 1", "key 1", "val 1" },
			{ " \t key 1-==\t  val 1 \n", "key 1-=", "val 1" },
		};

		int ct = 0;
		for (auto& it : stest)
		{
			INFO(it[0]);
			KProps<KStringView, KStringView> aMap;
			auto iCount = kSplitPairs(aMap, it[0], '=', ",", " \t\r\n", '-');
			if (ct == 0) CHECK (iCount == 0);
			else
			{
				CHECK( iCount == 1 );
				CHECK( aMap.size() == 1 );
				CHECK( aMap.begin()->first == it[1] );
				CHECK( aMap.begin()->second == it[2] );
			}
			++ct;
		}
	}

	SECTION("split default map consecutive")
	{
		// source, left target, right target
		std::vector<std::vector<KStringView>> stest
		{
			{ "key1=val1&key2=val2&key3=val3", "key1", "val1" , "key2", "val2" , "key3", "val3" },
			{ "   key1 = val1    &   key2 =val2 &\nkey3=\tval3", "key1", "val1" , "key2", "val2" , "key3", "val3" },
		};

		for (auto& it : stest)
		{
			INFO(it[0]);
			std::map<KStringView, KStringView> aMap;
			auto iCount = kSplitPairs(aMap, it[0], '=', "&");
			CHECK( iCount == 3 );
			CHECK( aMap.size() == 3 );
			auto i = aMap.begin();
			CHECK( i->first == it[1] );
			CHECK( i->second == it[2] );
			++i;
			CHECK( i->first == it[3] );
			CHECK( i->second == it[4] );
			++i;
			CHECK( i->first == it[5] );
			CHECK( i->second == it[6] );
		}
	}


}


SCENARIO ( "ksplit unit tests on invalid data" )
{
	GIVEN ( "a valid string" )
	{
		WHEN ( "parse simple delimited lists" )
		{
			THEN ( "examine the results" )
			{
			}
		}
	}
}
