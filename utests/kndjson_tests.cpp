#include "catch.hpp"

#include <dekaf2/data/json/kndjson.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <ostream>
#include <sstream>
#include <vector>

using namespace dekaf2;

namespace {

/// @return the values that the parser has ready, each printed in one line
KString Drain(KNDJSONParser& Parser)
{
	KString sValues;
	KJSON   Value;

	while (Parser.Next(Value))
	{
		sValues += Value.dump();
		sValues += '\n';
	}

	return sValues;
}

/// @return all values of the stream, each printed in one line
KString Drain(KNDJSONReader& Reader)
{
	KString sValues;
	KJSON   Value;

	while (Reader.Next(Value))
	{
		sValues += Value.dump();
		sValues += '\n';
	}

	return sValues;
}

/// a string buffer that counts how often it is flushed
class CountingBuffer : public std::stringbuf
{
public:

	int iFlushed { 0 };

protected:

	int sync() override
	{
		++iFlushed;
		return std::stringbuf::sync();
	}
};

} // end of anonymous namespace

TEST_CASE("KNDJSON")
{
	SECTION("the parser returns the values of complete lines")
	{
		KNDJSONParser Parser;
		KJSON         Value;

		CHECK ( Parser.Next(Value) == false );
		CHECK ( Parser.GetLine()   == 0     );

		Parser.Add("{\"a\":1}\n{\"a\":2}\n{\"a\"");

		REQUIRE ( Parser.Next(Value) );
		CHECK   ( Value.dump()     == R"({"a":1})" );
		CHECK   ( Parser.GetLine() == 1 );

		REQUIRE ( Parser.Next(Value) );
		CHECK   ( Value.dump()     == R"({"a":2})" );
		CHECK   ( Parser.GetLine() == 2 );

		// the third line is not complete
		CHECK ( Parser.Next(Value) == false );
		CHECK ( Parser.HasError()  == false );
		CHECK ( Parser.GetLine()   == 2     );

		Parser.Add(":3}\n");

		REQUIRE ( Parser.Next(Value) );
		CHECK   ( Value.dump()     == R"({"a":3})" );
		CHECK   ( Parser.GetLine() == 3 );

		CHECK ( Parser.Next(Value) == false );
		CHECK ( Parser.HasError()  == false );
	}

	SECTION("the input in pieces of any size")
	{
		KString sInput  = "{\"a\":1}\r\n\n[1,2,3]\n   \n\"text\" \nnull\n  42\n{\"b\":{\"c\":[true,false]}}";
		KString sExpect = "{\"a\":1}\n[1,2,3]\n\"text\"\nnull\n42\n{\"b\":{\"c\":[true,false]}}\n";

		for (std::size_t iPiece = 1; iPiece <= sInput.size(); ++iPiece)
		{
			KNDJSONParser Parser;
			KString       sValues;
			KStringView   sRest(sInput);

			while (!sRest.empty())
			{
				Parser.Add(sRest.substr(0, iPiece));
				sRest.remove_prefix(std::min(iPiece, sRest.size()));
				sValues += Drain(Parser);
			}

			Parser.Finish();
			sValues += Drain(Parser);

			INFO  ( iPiece );
			CHECK ( sValues == sExpect );
			CHECK ( Parser.HasError() == false );
			CHECK ( Parser.GetLine()  == 8 );
		}
	}

	SECTION("a string that holds the whole input is parsed in place")
	{
		KString sInput = "{\"a\":1}\r\n\n[1,2,3]\n  \n\"text\"\nnull";

		{
			KNDJSONParser Parser(sInput);

			// no Finish() is needed for the last line
			CHECK ( Drain(Parser) == "{\"a\":1}\n[1,2,3]\n\"text\"\nnull\n" );
			CHECK ( Parser.HasError() == false );
			CHECK ( Parser.GetLine()  == 6     );

			// the input is complete
			Parser.Add("1\n");

			CHECK ( Drain(Parser) == "" );
		}

		{
			// the parser reads from the string of the caller
			KNDJSONParser Parser(sInput);
			KJSON         Value;

			REQUIRE ( Parser.Next(Value) );
			CHECK   ( Value.dump() == R"({"a":1})" );

			sInput[11] = '7';

			REQUIRE ( Parser.Next(Value) );
			CHECK   ( Value.dump() == "[7,2,3]" );
		}

		{
			KNDJSONParser Parser("");

			CHECK ( Drain(Parser) == "" );
			CHECK ( Parser.HasError() == false );
			CHECK ( Parser.GetLine()  == 0     );
		}

		{
			// errors, skipped lines and the largest length of a line as with pieces
			KNDJSONParser Parser("1\n{\"a\":}\n3\n");

			CHECK ( Drain(Parser) == "1\n" );
			CHECK ( Parser.GetLastError().contains("parse error at line 2, column 6") );

			KNDJSONParser Skipping("1\n{\"a\":}\n3\n", KNDJSONParser::DefaultMaxLineLength, true);

			CHECK ( Drain(Skipping) == "1\n3\n" );
			CHECK ( Skipping.GetSkippedLines() == 1 );

			KNDJSONParser Limited("1\n[1,2,3,4,5,6]\n3\n", 10);

			CHECK ( Drain(Limited) == "1\n" );
			CHECK ( Limited.GetLastError() == "line 2 is longer than 10 bytes" );
		}

		{
			// after clear() the parser takes input in pieces
			KNDJSONParser Parser("1\n2\n");

			CHECK ( Drain(Parser) == "1\n2\n" );

			Parser.clear();
			Parser.Add("3\n4");

			CHECK ( Drain(Parser) == "3\n" );

			Parser.Finish();

			CHECK ( Drain(Parser) == "4\n" );
		}
	}

	SECTION("all values as one array")
	{
		{
			auto Values = KNDJSONParser("{\"a\":1}\n1\n\"text\"\nnull\n[1,2]\ntrue").GetAll();

			CHECK ( Values.dump() == R"([{"a":1},1,"text",null,[1,2],true])" );
		}

		{
			// an array for any count of lines
			auto None = KNDJSONParser("").GetAll();

			CHECK ( None.is_array() );
			CHECK ( None.empty()    );

			auto Spaces = KNDJSONParser("\n \n").GetAll();

			CHECK ( Spaces.is_array() );
			CHECK ( Spaces.empty()    );

			auto One = KNDJSONParser("{\"a\":1}\n").GetAll();

			CHECK ( One.dump() == R"([{"a":1}])" );
		}

		{
			// the values that Next() has not returned yet
			KNDJSONParser Parser("1\n2\n3\n");
			KJSON         Value;

			REQUIRE ( Parser.Next(Value) );
			CHECK   ( Parser.GetAll().dump() == "[2,3]" );
			CHECK   ( Parser.GetAll().dump() == "[]"    );
		}

		{
			// null if the input failed
			KNDJSONParser Parser("{\"a\":1}\n{\"a\":}\n{\"a\":3}\n");

			CHECK ( Parser.GetAll().is_null() );
			CHECK ( Parser.HasError() );
			CHECK ( Parser.GetLastError().contains("parse error at line 2, column 6") );
		}

		{
			// from input in pieces: the lines that are complete
			KNDJSONParser Parser;

			Parser.Add("1\n2\n3");

			CHECK ( Parser.GetAll().dump() == "[1,2]" );

			Parser.Finish();

			CHECK ( Parser.GetAll().dump() == "[3]" );
		}

		{
			// from a stream
			KInStringStream Stream("{\"a\":1}\n{\"a\":2}");
			KNDJSONReader   Reader(Stream);

			CHECK ( Reader.GetAll().dump() == R"([{"a":1},{"a":2}])" );
			CHECK ( Reader.GetAll().dump() == "[]" );

			KInStringStream Broken("{\"a\":1}\n{\"a\":");
			KNDJSONReader   BrokenReader(Broken);

			CHECK ( BrokenReader.GetAll().is_null() );
			CHECK ( BrokenReader.HasError() );
		}
	}

	SECTION("errors")
	{
		{
			// the column counts the white space before the value
			KNDJSONParser Parser("1\n  \t{\"a\":}\n");

			CHECK ( Parser.GetAll().is_null() );
			CHECK ( Parser.GetLastError() == "parse error at line 2, column 9: syntax error while parsing value - unexpected '}'; expected '[', '{', or a literal" );
		}

		{
			// two values in one line
			KNDJSONParser Parser("{\"a\":1} {\"a\":2}\n");

			CHECK ( Parser.GetAll().is_null() );
			CHECK ( Parser.GetLastError().contains("parse error at line 1, column 9") );
		}

		{
			// the limit of the nesting depth holds for every line
			KString sDeep = "[1]\n";
			sDeep.append(kjson::MaxParseDepth + 1, '[');
			sDeep.append(kjson::MaxParseDepth + 1, ']');

			KNDJSONParser Parser(sDeep);

			CHECK ( Parser.GetAll().is_null() );
			CHECK ( Parser.GetLastError() == kFormat("parse error at line 2: nesting depth exceeds {}", kjson::MaxParseDepth) );

			KString sLimit = "[1]\n";
			sLimit.append(kjson::MaxParseDepth, '[');
			sLimit.append(kjson::MaxParseDepth, ']');

			CHECK ( KNDJSONParser(sLimit).GetAll().size() == 2 );
		}

		{
			// on request an error throws
			KNDJSONParser Parser("1\n{\"a\":}\n");
			Parser.SetThrowOnError(true);

			KJSON Value;

			REQUIRE ( Parser.Next(Value) );
			CHECK_THROWS_AS ( Parser.Next(Value), const KError& );

			KNDJSONParser All("1\n{\"a\":}\n");
			All.SetThrowOnError(true);

			CHECK_THROWS_AS ( All.GetAll(), const KError& );
		}
	}

	SECTION("the last line without a line break needs Finish()")
	{
		KNDJSONParser Parser;

		Parser.Add("1\n2");

		CHECK ( Drain(Parser) == "1\n" );

		Parser.Finish();

		CHECK ( Drain(Parser) == "2\n" );
		CHECK ( Parser.GetLine() == 2 );

		// a finished input takes no more
		Parser.Add("3\n");

		CHECK ( Drain(Parser) == "" );

		// an input that ends with a line break has no further line
		Parser.clear();
		Parser.Add("1\n");
		Parser.Finish();

		CHECK ( Drain(Parser) == "1\n" );
		CHECK ( Parser.GetLine() == 1 );
	}

	SECTION("a line that is no JSON value fails the input")
	{
		KNDJSONParser Parser;

		Parser.Add("{\"a\":1}\n\n{\"a\":}\n{\"a\":4}\n");

		CHECK ( Drain(Parser) == "{\"a\":1}\n" );
		CHECK ( Parser.HasError() );
		CHECK ( Parser.GetLastError().contains("parse error at line 3, column 6") );
		CHECK ( Parser.GetLine() == 3 );

		// a failed input takes no more
		Parser.Add("{\"a\":5}\n");

		CHECK ( Drain(Parser) == "" );
		CHECK ( Parser.HasError() );

		Parser.clear();

		CHECK ( Parser.HasError() == false );
		CHECK ( Parser.GetLine()  == 0     );

		Parser.Add("{\"a\":6}\n");

		CHECK ( Drain(Parser) == "{\"a\":6}\n" );

		// a value that spans lines ends with its first line
		Parser.clear();
		Parser.Add("{\n\"a\":1\n}\n");

		CHECK ( Drain(Parser) == "" );
		CHECK ( Parser.GetLastError().contains("parse error at line 1, column 2") );

		// a number out of range names its line as well
		Parser.clear();
		Parser.Add("1\n1e999\n");

		CHECK ( Drain(Parser) == "1\n" );
		CHECK ( Parser.GetLastError().contains("line 2") );
	}

	SECTION("lines that are no JSON value are skipped on request")
	{
		KNDJSONParser Parser(KNDJSONParser::DefaultMaxLineLength, true);

		Parser.Add("{\"a\":1}\n{\"a\":}\nno json\n{\"a\":4}\n");

		CHECK ( Drain(Parser) == "{\"a\":1}\n{\"a\":4}\n" );
		CHECK ( Parser.HasError()        == false );
		CHECK ( Parser.GetSkippedLines() == 2     );
		CHECK ( Parser.GetLine()         == 4     );
	}

	SECTION("the largest length of a line")
	{
		{
			KNDJSONParser Parser(10);

			// a line of 10 bytes passes
			Parser.Add("[1,2,3,45]\n");

			CHECK ( Drain(Parser) == "[1,2,3,45]\n" );
			CHECK ( Parser.HasError() == false );

			// a line of 11 bytes does not
			Parser.Add("[1,2,3,456]\n");

			CHECK ( Drain(Parser) == "" );
			CHECK ( Parser.HasError() );
			CHECK ( Parser.GetLastError() == "line 2 is longer than 10 bytes" );
			CHECK ( Parser.GetLine() == 2 );
		}

		{
			// also before its line break has arrived
			KNDJSONParser Parser(10);

			Parser.Add("[1,2,3,4,5,");

			CHECK ( Drain(Parser) == "" );
			CHECK ( Parser.HasError() );
			CHECK ( Parser.GetLastError() == "line 1 is longer than 10 bytes" );
		}

		{
			// and when lines are skipped
			KNDJSONParser Parser(10, true);

			Parser.Add("1\nabcdefghijklmnop\n2\n");

			CHECK ( Drain(Parser) == "1\n" );
			CHECK ( Parser.HasError() );
		}

		{
			KNDJSONParser Parser(KString::npos);
			KString       sLine = "\"";

			sLine.append(100000, 'x');
			sLine += "\"\n";

			Parser.Add(sLine);

			KJSON Value;

			REQUIRE ( Parser.Next(Value) );
			CHECK   ( Value.is_string() );
			CHECK   ( Value.dump().size() == 100002 );
		}
	}

	SECTION("the reader reads a stream to its end")
	{
		{
			KInStringStream Stream("{\"a\":1}\n\n{\"a\":2}\r\n{\"a\":3}");
			KNDJSONReader   Reader(Stream);

			CHECK ( Drain(Reader) == "{\"a\":1}\n{\"a\":2}\n{\"a\":3}\n" );
			CHECK ( Reader.HasError() == false );
			CHECK ( Reader.GetLine()  == 4     );

			// and stays at the end
			KJSON Value;

			CHECK ( Reader.Next(Value) == false );
		}

		{
			KInStringStream Stream("");
			KNDJSONReader   Reader(Stream);

			CHECK ( Drain(Reader) == "" );
			CHECK ( Reader.HasError() == false );
			CHECK ( Reader.GetLine()  == 0     );
		}

		{
			// more input than one read returns
			KString sInput;
			KString sExpect;

			for (int i = 0; i < 3000; ++i)
			{
				sInput  += kFormat("{{\"line\":{},\"text\":\"some text to fill the line\"}}\n", i);
				sExpect += kFormat("{{\"line\":{},\"text\":\"some text to fill the line\"}}\n", i);
			}

			KInStringStream Stream(sInput);
			KNDJSONReader   Reader(Stream);

			CHECK ( Drain(Reader) == sExpect );
			CHECK ( Reader.GetLine() == 3000 );
		}
	}

	SECTION("the reader stops at a line that is no JSON value")
	{
		{
			KInStringStream Stream("{\"a\":1}\n{\"a\":\n{\"a\":3}\n");
			KNDJSONReader   Reader(Stream);

			CHECK ( Drain(Reader) == "{\"a\":1}\n" );
			CHECK ( Reader.HasError() );
			CHECK ( Reader.GetLastError().contains("parse error at line 2") );
			CHECK ( Reader.GetLine() == 2 );
		}

		{
			// or skips it on request
			KInStringStream Stream("{\"a\":1}\n{\"a\":\n{\"a\":3}\n");
			KNDJSONReader   Reader(Stream, KNDJSONParser::DefaultMaxLineLength, true);

			CHECK ( Drain(Reader) == "{\"a\":1}\n{\"a\":3}\n" );
			CHECK ( Reader.HasError()        == false );
			CHECK ( Reader.GetSkippedLines() == 1     );
		}

		{
			KInStringStream Stream("1\n[1,2,3,4,5,6,7,8,9]\n");
			KNDJSONReader   Reader(Stream, 10);

			CHECK ( Drain(Reader) == "1\n" );
			CHECK ( Reader.GetLastError() == "line 2 is longer than 10 bytes" );
		}
	}

	SECTION("the writer writes one value per line")
	{
		KString          sOut;
		KOutStringStream Out(sOut);
		KNDJSONWriter    Writer(Out);

		KJSON Object;
		Object["text"]   = "two\nlines";
		Object["nested"] = KJSON::array({ 1, 2, KJSON::object({{ "deep", true }}) });

		CHECK ( Writer.GetLines() == 0 );

		CHECK ( Writer.Write(Object)          );
		CHECK ( Writer.Write(KJSON(42))       );
		CHECK ( Writer.Write(KJSON("text"))   );
		CHECK ( Writer.Write(KJSON())         );
		CHECK ( Writer.Write(KJSON::array())  );

		Out.Flush();

		CHECK ( sOut == "{\"nested\":[1,2,{\"deep\":true}],\"text\":\"two\\nlines\"}\n42\n\"text\"\nnull\n[]\n" );
		CHECK ( Writer.GetLines() == 5     );
		CHECK ( Writer.HasError() == false );

		// and the parser reads the same values
		auto Values = KNDJSONParser(sOut).GetAll();

		REQUIRE ( Values.size() == 5 );
		CHECK   ( Values[0] == Object );
		CHECK   ( Values[3].is_null() );
	}

	SECTION("the writer writes the elements of an array")
	{
		KString          sOut;
		KOutStringStream Out(sOut);
		KNDJSONWriter    Writer(Out);

		auto Values = KNDJSONParser("{\"a\":1}\n[1,2]\n\"text\"\nnull\n").GetAll();

		CHECK ( Writer.WriteAll(Values) );

		Out.Flush();

		CHECK ( sOut == "{\"a\":1}\n[1,2]\n\"text\"\nnull\n" );
		CHECK ( Writer.GetLines() == 4 );

		// an array without elements has no lines
		CHECK ( Writer.WriteAll(KJSON::array()) );
		CHECK ( Writer.GetLines() == 4 );

		// what is no array is not written
		CHECK ( Writer.WriteAll(KJSON()) == false );
		CHECK ( Writer.HasError() );
		CHECK ( Writer.GetLastError() == "the values to write are no array" );
		CHECK ( Writer.WriteAll(Values[0]) == false );

		Out.Flush();

		CHECK ( sOut == "{\"a\":1}\n[1,2]\n\"text\"\nnull\n" );
		CHECK ( Writer.GetLines() == 4 );
	}

	SECTION("a value that is no UTF-8 leaves nothing in the stream")
	{
		KJSON Broken;
		Broken["first"]  = "good";
		Broken["second"] = KString("bad \xff byte");

		{
			KString          sOut;
			KOutStringStream Out(sOut);
			KNDJSONWriter    Writer(Out);

			CHECK ( Writer.Write(KJSON(1)) );
			CHECK ( Writer.Write(Broken) == false );
			CHECK ( Writer.HasError() );
			CHECK ( Writer.GetLastError().starts_with("cannot write line 2: ") );
			CHECK ( Writer.GetLines() == 1 );

			// the next value is written
			CHECK ( Writer.Write(KJSON(3)) );

			Out.Flush();

			CHECK ( sOut == "1\n3\n" );
			CHECK ( Writer.GetLines() == 2 );
		}

		{
			// the elements of an array up to the first one that cannot be written
			KString          sOut;
			KOutStringStream Out(sOut);
			KNDJSONWriter    Writer(Out);

			KJSON Values = KJSON::array();
			Values.push_back(1);
			Values.push_back(Broken);
			Values.push_back(3);

			CHECK ( Writer.WriteAll(Values) == false );

			Out.Flush();

			CHECK ( sOut == "1\n" );
		}

		{
			// on request the bytes are replaced
			KString          sOut;
			KOutStringStream Out(sOut);
			KNDJSONWriter    Writer(Out, false, true);

			CHECK ( Writer.Write(Broken) );

			Out.Flush();

			CHECK ( sOut == "{\"first\":\"good\",\"second\":\"bad \xef\xbf\xbd byte\"}\n" );
			CHECK ( Writer.HasError() == false );
		}

		{
			// on request an error throws
			KString          sOut;
			KOutStringStream Out(sOut);
			KNDJSONWriter    Writer(Out);
			Writer.SetThrowOnError(true);

			CHECK_THROWS_AS ( Writer.Write(Broken), const KError& );

			Out.Flush();

			CHECK ( sOut.empty() );
		}
	}

	SECTION("the writer flushes the lines on request")
	{
		{
			CountingBuffer Buffer;
			std::ostream   Stream(&Buffer);
			KOutStream     Out(Stream);
			KNDJSONWriter  Writer(Out, true);

			CHECK ( Writer.Write(KJSON(1)) );
			CHECK ( Buffer.iFlushed == 1 );
			CHECK ( Writer.Write(KJSON(2)) );
			CHECK ( Buffer.iFlushed == 2 );
			CHECK ( Buffer.str() == "1\n2\n" );
		}

		{
			CountingBuffer Buffer;
			std::ostream   Stream(&Buffer);
			KOutStream     Out(Stream);
			KNDJSONWriter  Writer(Out);

			CHECK ( Writer.Write(KJSON(1)) );
			CHECK ( Writer.Write(KJSON(2)) );
			CHECK ( Buffer.iFlushed == 0 );
		}
	}

	SECTION("a stream that failed")
	{
		std::ostringstream Stream;
		KOutStream         Out(Stream);
		KNDJSONWriter      Writer(Out);

		CHECK ( Writer.Write(KJSON(1)) );

		Stream.setstate(std::ios_base::badbit);

		CHECK ( Writer.Write(KJSON(2)) == false );
		CHECK ( Writer.GetLastError() == "cannot write line 2 to the stream" );
		CHECK ( Writer.GetLines() == 1 );
		CHECK ( Stream.str() == "1\n" );
	}
}
