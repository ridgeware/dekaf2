#include <map>
#include <vector>
#include <tuple>
#include <sstream>
#include <iomanip>

#include "dekaf2/dekaf2.h"
#include "catch.hpp"
#include "dekaf2/ksplit.h"

using namespace dekaf2;
using std::vector;
using std::get;
using std::map;
using std::tuple;
using std::stringstream;
using std::hex;

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
