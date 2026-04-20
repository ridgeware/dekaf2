#include <vector>
#include "catch.hpp"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/containers/associative/kprops.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <map>
#include <set>
#include <list>


using namespace dekaf2;
using std::vector;

SCENARIO ( "ksplit unit tests on valid data" )
{
	static constexpr KStringView sEmpty("");
	static constexpr KStringView sComma(",");

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
			KStringViewPair svPair = kSplitToPair(it[0], "=", "");
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
			KStringViewPair svPair = kSplitToPair(it[0], "=", " \t\r\n", '-');
			CHECK( svPair.first == it[1] );
			CHECK( svPair.second == it[2] );
		}
	}

	SECTION("split with quotes (bRespectQuotes=true)")
	{
		// source, left target, right target (quotes stripped)
		std::vector<std::vector<KStringView>> stest
		{
			{ R"(k="v")",                         "k",        "v" },
			{ R"(  "key name"  =  "value here"  )", "key name", "value here" },
			{ R"(k='single quoted')",             "k",        "single quoted" },
			{ R"("mixed"=value)",                 "mixed",    "value" },
			{ R"(key="val with = inside")",       "key",      "val with = inside" },
		};

		for (auto& it : stest)
		{
			INFO(it[0]);
			KStringViewPair svPair = kSplitToPair(it[0], "=", detail::kASCIISpacesSet, '\0', true);
			CHECK( svPair.first  == it[1] );
			CHECK( svPair.second == it[2] );
		}
	}

	SECTION("unmatched quote is not stripped (bRespectQuotes=true)")
	{
		// only matching surrounding quotes are removed; a lone leading quote stays
		auto p1 = kSplitToPair(R"(k="v)",  "=", detail::kASCIISpacesSet, '\0', true);
		CHECK( p1.first  == "k" );
		CHECK( p1.second == R"("v)" );

		auto p2 = kSplitToPair(R"(k=v")",  "=", detail::kASCIISpacesSet, '\0', true);
		CHECK( p2.first  == "k" );
		CHECK( p2.second == R"(v")" );

		// mismatched quote types: '"...'' should NOT be stripped (first != last)
		auto p3 = kSplitToPair(R"(k="v')", "=", detail::kASCIISpacesSet, '\0', true);
		CHECK( p3.first  == "k" );
		CHECK( p3.second == R"("v')" );
	}

	SECTION("no quote stripping when bRespectQuotes=false (default)")
	{
		auto p = kSplitToPair(R"(k="v")", "=", detail::kASCIISpacesSet, '\0', false);
		CHECK( p.first  == "k" );
		CHECK( p.second == R"("v")" );
	}

}

TEST_CASE("kSplit for maps")
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
			auto iCount = kSplit(aMap, it[0]);
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

	SECTION("split default KMap")
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
			KMap<KStringView, KStringView> aMap;
			auto iCount = kSplit(aMap, it[0]);
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
			auto iCount = kSplit(aMap, it[0]);
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
			auto iCount = kSplit(aMap, it[0], ",", "=", "");
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
			auto iCount = kSplit(aMap, it[0], ",", "=", " \t\r\n", '-');
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
			auto iCount = kSplit(aMap, it[0], "&", "=");
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

	SECTION("duplicate key: last value wins (std::map)")
	{
		std::map<KStringView, KStringView> aMap;
		auto iCount = kSplit(aMap, "a=1,a=2,b=3", ",", "=");
		// the return reflects the final container size (unique keys), not the
		// number of pairs parsed
		CHECK( iCount   == 2 );
		CHECK( aMap.size() == 2 );
		CHECK( aMap["a"] == "2" );  // overwritten by later value
		CHECK( aMap["b"] == "3" );
	}

	SECTION("duplicate key: last value wins (KMap)")
	{
		KMap<KStringView, KStringView> aMap;
		kSplit(aMap, "a=1,a=2,b=3", ",", "=");
		CHECK( aMap.size() == 2 );
		CHECK( aMap["a"] == "2" );
		CHECK( aMap["b"] == "3" );
	}

}

TEST_CASE("kSplit")
{
	SECTION("split lines")
	{
		std::vector<KStringView> Parts;

		kSplit(Parts, "line 1\nline 2\nline 3", "\n", " \t\r\b");
		CHECK ( Parts.size() == 3 );
		CHECK ( Parts[0] == "line 1" );
		CHECK ( Parts[1] == "line 2" );
		CHECK ( Parts[2] == "line 3" );
		Parts.clear();

		kSplit(Parts, "line 1  \nline 2\r\nline 3", "\n", " \t\r\b");
		CHECK ( Parts.size() == 3 );
		CHECK ( Parts[0] == "line 1" );
		CHECK ( Parts[1] == "line 2" );
		CHECK ( Parts[2] == "line 3" );
		Parts.clear();

		kSplit(Parts, "line 1  \n\r\nline 3", "\n", " \t\r\b");
		CHECK ( Parts.size() == 3 );
		CHECK ( Parts[0] == "line 1" );
		CHECK ( Parts[1] == "" );
		CHECK ( Parts[2] == "line 3" );
		Parts.clear();
	}

	SECTION("split with strings")
	{
		std::vector<KString> Parts;

		kSplit(Parts, "line 1\nline 2\nline 3"_ks, "\n"_ks, " \t\r\b"_ks);
		CHECK ( Parts.size() == 3 );
		CHECK ( Parts[0] == "line 1" );
		CHECK ( Parts[1] == "line 2" );
		CHECK ( Parts[2] == "line 3" );
	}

	SECTION("split auto range")
	{
		std::vector<KStringView> vresult {
			"line 1",
			"line 2",
			"line 3"
		};

		auto rit = vresult.begin();
		for (auto it : kSplits("line 1\nline 2\nline 3", "\n", " \t\r\b"))
		{
			CHECK ( it == *rit++);
		}
	}

	SECTION("split with quotes")
	{
		std::vector<KStringView> Parts;
		kSplit(Parts, R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter)", " ", detail::kASCIISpacesSet, '\\', false, true);
		CHECK ( Parts.size() == 10 );
		if (Parts.size() == 10)
		{
			CHECK ( Parts[0] == "something");
			CHECK ( Parts[1] == "like this");
			CHECK ( Parts[2] == "should");
			CHECK ( Parts[3] == "be");
			CHECK ( Parts[4] == "coded for");
			CHECK ( Parts[5] == "quotes");
			CHECK ( Parts[6] == "so");
			CHECK ( Parts[7] == "that");
			CHECK ( Parts[8] == R"(the output\'s)");
			CHECK ( Parts[9] == "smarter");
		}
	}

	SECTION("unterminated quote is not treated as a quoted token")
	{
		// bRespectQuotes=true, but the opening quote has no matching closer: the
		// implementation must fall back to the normal delimiter-based split so
		// the leading quote stays with the token.
		std::vector<KStringView> Parts;
		kSplit(Parts, R"(foo "bar baz)", " ", detail::kASCIISpacesSet, '\0', false, true);
		CHECK ( Parts.size() == 3 );
		if (Parts.size() == 3)
		{
			CHECK ( Parts[0] == "foo" );
			CHECK ( Parts[1] == R"("bar)" );   // leading quote retained
			CHECK ( Parts[2] == "baz" );
		}
	}

	SECTION("escape inside a quoted token hides the quote char")
	{
		// escape before closing quote inside the quotes: quote-finder must skip the
		// escaped quote and find the real closing one
		std::vector<KStringView> Parts;
		kSplit(Parts, R"("like\"this" end)", " ", detail::kASCIISpacesSet, '\\', false, true);
		CHECK ( Parts.size() == 2 );
		if (Parts.size() == 2)
		{
			CHECK ( Parts[0] == R"(like\"this)" );
			CHECK ( Parts[1] == "end" );
		}
	}

	SECTION("kSplitArgs")
	{
		std::vector<KStringView> Parts;
		kSplitArgs(Parts, R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter)");
		CHECK ( Parts.size() == 10 );
		if (Parts.size() == 10)
		{
			CHECK ( Parts[0] == "something");
			CHECK ( Parts[1] == "like this");
			CHECK ( Parts[2] == "should");
			CHECK ( Parts[3] == "be");
			CHECK ( Parts[4] == "coded for");
			CHECK ( Parts[5] == "quotes");
			CHECK ( Parts[6] == "so");
			CHECK ( Parts[7] == "that");
			CHECK ( Parts[8] == R"(the output\'s)");
			CHECK ( Parts[9] == "smarter");
		}
	}

	SECTION("kSplitsArgs")
	{
		auto Parts = kSplitsArgs(R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter)");
		CHECK ( Parts.size() == 10 );
		if (Parts.size() == 10)
		{
			CHECK ( Parts[0] == "something");
			CHECK ( Parts[1] == "like this");
			CHECK ( Parts[2] == "should");
			CHECK ( Parts[3] == "be");
			CHECK ( Parts[4] == "coded for");
			CHECK ( Parts[5] == "quotes");
			CHECK ( Parts[6] == "so");
			CHECK ( Parts[7] == "that");
			CHECK ( Parts[8] == R"(the output\'s)");
			CHECK ( Parts[9] == "smarter");
		}
	}

	SECTION("split into map with quoted values")
	{
		// reproduces a typical HTTP Digest Authorization header parse:
		// comma-separated parameters, values optionally double-quoted
		KStringView sInput =
			R"(username="Mufasa", realm="testrealm@host.com", nonce="abcd1234", qop=auth, nc=00000001)";

		KMap<KStringView, KStringView> aMap;
		kSplit(aMap, sInput,
		       KFindSetOfChars(","), // outer delimiter
		       "=",                  // pair delimiter
		       detail::kASCIISpacesSet,
		       '\0',                 // no escape
		       false,                // don't combine delimiters
		       true                  // bRespectQuotes -> strip surrounding quotes
		);
		CHECK ( aMap.size() == 5 );
		CHECK ( aMap["username"] == "Mufasa" );
		CHECK ( aMap["realm"]    == "testrealm@host.com" );
		CHECK ( aMap["nonce"]    == "abcd1234" );
		CHECK ( aMap["qop"]      == "auth" );        // unquoted value works too
		CHECK ( aMap["nc"]       == "00000001" );
	}

	SECTION("split into map with quoted values via member Split<>()")
	{
		KStringView sInput =
			R"(username="Mufasa", realm="testrealm@host.com", response="0123abcd")";

		auto aMap = sInput.Split<KMap<KStringView, KStringView>>(
		                KFindSetOfChars(","), "=",
		                detail::kASCIISpacesSet, '\0', false, true);
		CHECK ( aMap.size() == 3 );
		CHECK ( aMap["username"] == "Mufasa" );
		CHECK ( aMap["realm"]    == "testrealm@host.com" );
		CHECK ( aMap["response"] == "0123abcd" );
	}

	SECTION("split into map with single quotes and whitespace")
	{
		KStringView sInput = R"( a = 'one' , b = 'two words' , c = 3 )";

		KMap<KStringView, KStringView> aMap;
		kSplit(aMap, sInput, KFindSetOfChars(","), "=",
		       detail::kASCIISpacesSet, '\0', false, true);
		CHECK ( aMap.size() == 3 );
		CHECK ( aMap["a"] == "one" );
		CHECK ( aMap["b"] == "two words" );
		CHECK ( aMap["c"] == "3" );
	}

	SECTION("split into map without quote stripping leaves quotes in place")
	{
		// same input as before, but with bRespectQuotes=false (default) — we expect
		// the quotes to stay on the values, documenting the opt-in nature of the feature
		KStringView sInput = R"(k1="v1", k2="v2")";

		KMap<KStringView, KStringView> aMap;
		kSplit(aMap, sInput, KFindSetOfChars(","), "=");
		CHECK ( aMap.size() == 2 );
		CHECK ( aMap["k1"] == R"("v1")" );
		CHECK ( aMap["k2"] == R"("v2")" );
	}
}

TEST_CASE("kSplit char-delim fast path")
{
	// covers the single-char overload at ksplit.h ~L71-144:
	// kSplit(Container, KStringView, char) -- no trim, no escape, no quotes, no combine

	SECTION("non-space delim, basic")
	{
		std::vector<KStringView> v;
		auto n = kSplit(v, "a,b,c,d,e", ',');
		CHECK ( n == 5 );
		CHECK ( v.size() == 5 );
		CHECK ( v[0] == "a" );
		CHECK ( v[4] == "e" );
	}

	SECTION("non-space delim, empty input")
	{
		std::vector<KStringView> v;
		auto n = kSplit(v, "", ',');
		CHECK ( n == 0 );
		CHECK ( v.empty() );
	}

	SECTION("non-space delim, no delimiter in input")
	{
		std::vector<KStringView> v;
		auto n = kSplit(v, "abc", ',');
		CHECK ( n == 1 );
		CHECK ( v[0] == "abc" );
	}

	SECTION("non-space delim, leading delimiter -> empty first element")
	{
		std::vector<KStringView> v;
		kSplit(v, ",a,b", ',');
		CHECK ( v.size() == 3 );
		CHECK ( v[0].empty() );
		CHECK ( v[1] == "a" );
		CHECK ( v[2] == "b" );
	}

	SECTION("non-space delim, trailing delimiter -> empty last element")
	{
		std::vector<KStringView> v;
		kSplit(v, "a,b,", ',');
		CHECK ( v.size() == 3 );
		CHECK ( v[0] == "a" );
		CHECK ( v[1] == "b" );
		CHECK ( v[2].empty() );
	}

	SECTION("non-space delim, adjacent delimiters preserve empty elements")
	{
		std::vector<KStringView> v;
		kSplit(v, "a,,b,,,c", ',');
		CHECK ( v.size() == 6 );
		CHECK ( v[0] == "a" );
		CHECK ( v[1].empty() );
		CHECK ( v[2] == "b" );
		CHECK ( v[3].empty() );
		CHECK ( v[4].empty() );
		CHECK ( v[5] == "c" );
	}

	SECTION("space delim collapses consecutive spaces (not empty elements)")
	{
		std::vector<KStringView> v;
		kSplit(v, "foo  bar   baz", ' ');
		CHECK ( v.size() == 3 );
		CHECK ( v[0] == "foo" );
		CHECK ( v[1] == "bar" );
		CHECK ( v[2] == "baz" );
	}

	SECTION("space delim: leading and trailing spaces both collapse")
	{
		// space-as-delim follows Python split() / Go strings.Fields / shell
		// word-splitting semantics: all runs of spaces (leading, internal,
		// trailing) collapse to a single separator -- no empty tokens
		std::vector<KStringView> v;
		kSplit(v, "  foo bar  ", ' ');
		CHECK ( v.size() == 2 );
		CHECK ( v[0] == "foo" );
		CHECK ( v[1] == "bar" );
	}

	SECTION("space delim, only-spaces input yields no tokens")
	{
		std::vector<KStringView> v;
		auto n = kSplit(v, "     ", ' ');
		CHECK ( n == 0 );
		CHECK ( v.empty() );
	}

	SECTION("space delim, single leading space")
	{
		std::vector<KStringView> v;
		kSplit(v, " foo", ' ');
		CHECK ( v.size() == 1 );
		CHECK ( v[0] == "foo" );
	}

	SECTION("space delim, single trailing space")
	{
		std::vector<KStringView> v;
		kSplit(v, "foo ", ' ');
		CHECK ( v.size() == 1 );
		CHECK ( v[0] == "foo" );
	}

	SECTION("space delim, no spaces in input")
	{
		std::vector<KStringView> v;
		kSplit(v, "abc", ' ');
		CHECK ( v.size() == 1 );
		CHECK ( v[0] == "abc" );
	}
}

TEST_CASE("kSplitEmpty")
{
	SECTION("SplitEmptyElements")
	{
		std::vector<KStringView> Part;

		kSplit(Part, "empty,  ");
		CHECK ( Part.size() == 2 );
		CHECK ( Part[1] == "" );
		Part.clear();

		kSplit(Part, "empty,  ", ",", "");
		CHECK ( Part.size() == 2 );
		CHECK ( Part[1] == "  " );
		Part.clear();

		kSplit(Part, ",empty,elements,,begin,middle,end,");
		CHECK ( Part.size() == 8 );
		CHECK ( Part[0].empty() );
		CHECK ( Part[3].empty() );
		CHECK ( Part[6] == "end" );
		CHECK ( Part[7].empty() );
		Part.clear();

		kSplit(Part, "/empty/elements//begin/middle/end///", "/", "", 0, true, false);
		CHECK ( Part.size() == 7 );
		CHECK ( Part[0].empty() );
		CHECK ( Part[1] == "empty" );
		CHECK ( Part[5] == "end" );
		CHECK ( Part[6].empty() );
		Part.clear();

		kSplit(Part, "/empty/elements//begin/middle/end///", "/", "/", 0, true, false);
		CHECK ( Part.size() == 5 );
		CHECK ( Part[0] == "empty" );
		CHECK ( Part[4] == "end" );
		Part.clear();

		kSplit(Part, " empty\t    elements  \t  begin middle      end      ", " ", " \t", 0, false, false);
		CHECK ( Part.size() == 5 );
		CHECK ( Part[0] == "empty" );
		CHECK ( Part[1] == "elements" );
		CHECK ( Part[2] == "begin" );
		CHECK ( Part[3] == "middle" );
		CHECK ( Part[4] == "end" );
		Part.clear();

		kSplit(Part, " empty    elements\t    begin middle      end      ", " ", " \t", 0, true, false);
		CHECK ( Part.size() == 5 );
		CHECK ( Part[0] == "empty" );
		CHECK ( Part[1] == "elements" );
		CHECK ( Part[2] == "begin" );
		CHECK ( Part[3] == "middle" );
		CHECK ( Part[4] == "end" );
		Part.clear();

		kSplit(Part, " empty\t    elements  \t  begin middle      end      \t", " \t", " \t", 0, false, false);
		CHECK ( Part.size() == 6 );
		CHECK ( Part[0] == "empty" );
		CHECK ( Part[1] == "elements" );
		CHECK ( Part[2] == "begin" );
		CHECK ( Part[3] == "middle" );
		CHECK ( Part[4] == "end" );
		CHECK ( Part[5].empty() );
		Part.clear();

		kSplit(Part, " empty    elements\t    begin middle      end      \t", " \t", " \t", 0, true, false);
		CHECK ( Part.size() == 5 );
		CHECK ( Part[0] == "empty" );
		CHECK ( Part[1] == "elements" );
		CHECK ( Part[2] == "begin" );
		CHECK ( Part[3] == "middle" );
		CHECK ( Part[4] == "end" );
		Part.clear();
	}
}

TEST_CASE("String split to pairs")
{
	KString sInput = "Cookie1 = value 1; Cookie2= value2 ;Cookie3=value3";
	auto Cookies = sInput.Split<std::vector<KStringViewPair>>(";", "=");
	CHECK ( Cookies.size() == 3 );
	CHECK ( Cookies[0].first  == "Cookie1" );
	CHECK ( Cookies[0].second == "value 1" );
	CHECK ( Cookies[1].first  == "Cookie2" );
	CHECK ( Cookies[1].second == "value2"  );
	CHECK ( Cookies[2].first  == "Cookie3" );
	CHECK ( Cookies[2].second == "value3"  );
}

TEST_CASE("kSplits<vector<pair>> free-function return variant")
{
	// kSplits(...) that returns a sequence-of-pairs container directly
	// (exercises the PushBackPair adapter path)
	auto v = kSplits<std::vector<KStringViewPair>>("a=1,b=2,c=3", ",", "=");
	CHECK ( v.size() == 3 );
	CHECK ( v[0].first  == "a" );
	CHECK ( v[0].second == "1" );
	CHECK ( v[1].first  == "b" );
	CHECK ( v[1].second == "2" );
	CHECK ( v[2].first  == "c" );
	CHECK ( v[2].second == "3" );

	// duplicate keys preserve all pairs (unlike map)
	auto dup = kSplits<std::vector<KStringViewPair>>("a=1,a=2,b=3", ",", "=");
	CHECK ( dup.size() == 3 );
	CHECK ( dup[0].first  == "a" );
	CHECK ( dup[0].second == "1" );
	CHECK ( dup[1].first  == "a" );
	CHECK ( dup[1].second == "2" );

	// with bRespectQuotes=true
	auto q = kSplits<std::vector<KStringViewPair>>(
	             R"(u="Mufasa", r="realm")",
	             ",", "=",
	             detail::kASCIISpacesSet, '\0', false, true);
	CHECK ( q.size() == 2 );
	CHECK ( q[0].second == "Mufasa" );
	CHECK ( q[1].second == "realm" );
}


TEST_CASE("kSplitArgsInPlace")
{
	SECTION("split")
	{
		KTempDir TempDir;
		std::vector<const char*> argVector;
		KString sCommand(kFormat("/bin/sh 'random arg' -c \\\"hi! \"echo 'some random data' > {}/kinpipetest.file 2>&1\"", TempDir.Name()));//
		CHECK( kSplitArgsInPlace(argVector, sCommand) );
		CHECK(argVector.size() == 5);
		std::vector<KString> compVector = {"/bin/sh", "random arg", "-c", "\"hi!", kFormat("echo 'some random data' > {}/kinpipetest.file 2>&1", TempDir.Name())};
		CHECK ( compVector.size() == argVector.size() );
		if (compVector.size() == argVector.size())
		{
			for (size_t i = 0; i < argVector.size() - 1; i++)
			{
				INFO (argVector[i]);
				CHECK( strcmp(compVector[i].c_str(), argVector[i]) == 0 );
			}
		}
	}

	SECTION("split with quotes")
	{
		std::vector<KStringView> Parts;
		KString sCommand(R"(something "like this" should be 'coded for' quotes so that 'the output\'s' smarter)");
		kSplitArgsInPlace(Parts, sCommand);
		CHECK ( Parts.size() == 10 );
		if (Parts.size() == 10)
		{
			CHECK ( Parts[0] == "something");
			CHECK ( Parts[1] == "like this");
			CHECK ( Parts[2] == "should");
			CHECK ( Parts[3] == "be");
			CHECK ( Parts[4] == "coded for");
			CHECK ( Parts[5] == "quotes");
			CHECK ( Parts[6] == "so");
			CHECK ( Parts[7] == "that");
			CHECK ( Parts[8] == R"(the output\'s)");
			CHECK ( Parts[9] == "smarter");
		}
	}
}

TEST_CASE("kSplit for sets")
{
	SECTION("split default set")
	{
		auto Result = kSplits<std::set<KStringView>>("aaa,ccc , bbb ,  ddd", ",");
		CHECK (Result.size() == 4);
		if (Result.size() == 4)
		{
			auto it = Result.begin();
			CHECK (*it++ == "aaa");
			CHECK (*it++ == "bbb");
			CHECK (*it++ == "ccc");
			CHECK (*it++ == "ddd");
		}
	}

	SECTION("split KSet")
	{
		auto Result = kSplits<KSet<KStringView>>("aaa,ccc , bbb ,  ddd", ",");
		CHECK (Result.size() == 4);
		if (Result.size() == 4)
		{
			auto it = Result.begin();
			CHECK (*it++ == "aaa");
			CHECK (*it++ == "bbb");
			CHECK (*it++ == "ccc");
			CHECK (*it++ == "ddd");
		}
	}

	SECTION("split multiset")
	{
		auto Result = kSplits<std::multiset<KStringView>>("aaa,ccc , bbb ,  ddd", ",");
		CHECK (Result.size() == 4);
		if (Result.size() == 4)
		{
			auto it = Result.begin();
			CHECK (*it++ == "aaa");
			CHECK (*it++ == "bbb");
			CHECK (*it++ == "ccc");
			CHECK (*it++ == "ddd");
		}
	}

	SECTION("split KMultiSet")
	{
		auto Result = kSplits<KMultiSet<KStringView>>("aaa,ccc , bbb ,  ddd", ",");
		CHECK (Result.size() == 4);
		if (Result.size() == 4)
		{
			auto it = Result.begin();
			CHECK (*it++ == "aaa");
			CHECK (*it++ == "bbb");
			CHECK (*it++ == "ccc");
			CHECK (*it++ == "ddd");
		}
	}
}
