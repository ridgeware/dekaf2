#include "catch.hpp"

#include <dekaf2/util/i18n/kmessageformat.h>
#include <dekaf2/util/i18n/kpluralrules.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/format/kformat.h>
#include <limits>

using namespace dekaf2;

namespace {

// message, language, arguments as JSON, and what intl-messageformat 10 (FormatJS)
// produces for it - generated with Node, see plan-kwebapp-i18n.md section 6
struct Expectation
{
	KStringView sMessage;
	KStringView sLanguage;
	KStringView sArgs;
	KStringView sExpected;
};

constexpr Expectation Expectations[] =
{
	{ "Hello {name}!", "en", R"json({"name":"World"})json", "Hello World!" },
	{ "{count} items", "en", R"json({"count":3})json", "3 items" },
	{ "{price} EUR", "de", R"json({"price":12.5})json", "12.5 EUR" },
	{ "{flag}", "en", R"json({"flag":true})json", "true" },
	{ "{a} {b}", "en", R"json({"a":"x","b":null})json", "x " },
	{ "{0} and {1}", "en", R"json(["first","second"])json", "first and second" },
	{ "l'appel de {name}", "fr", R"json({"name":"Anne"})json", "l'appel de Anne" },
	{ "a '{' b '}' c", "en", R"json({})json", "a { b } c" },
	{ "it''s {x}", "en", R"json({"x":1})json", "it's 1" },
	{ "'{name}' is {name}", "en", R"json({"name":"A"})json", "{name} is A" },
	{ "a '{b c", "en", R"json({})json", "a {b c" },
	{ "'<b>' and <i>{x}</i>", "en", R"json({"x":1})json", "<b> and <i>1</i>" },
	{ "# {x}", "en", R"json({"x":1})json", "# 1" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":1})json", "1 item" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":2})json", "2 items" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":0})json", "0 items" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":1.5})json", "1.5 items" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":"1"})json", "1 item" },
	{ "{n, plural, one {# item} other {# items}}", "en", R"json({"n":1})json", "1 item" },
	{ "{n, plural, offset:1 =0 {nobody} =1 {you} one {you and # other} other {you and # others}}", "en", R"json({"n":0})json", "nobody" },
	{ "{n, plural, offset:1 =0 {nobody} =1 {you} one {you and # other} other {you and # others}}", "en", R"json({"n":1})json", "you" },
	{ "{n, plural, offset:1 =0 {nobody} =1 {you} one {you and # other} other {you and # others}}", "en", R"json({"n":2})json", "you and 1 other" },
	{ "{n, plural, offset:1 =0 {nobody} =1 {you} one {you and # other} other {you and # others}}", "en", R"json({"n":5})json", "you and 4 others" },
	{ "{n, plural, =1 {exact} other {other}}", "en", R"json({"n":1})json", "exact" },
	{ "{n, plural, other {'#' items}}", "en", R"json({"n":3})json", "# items" },
	{ "{n, plural, other {{g, select, other {# items}}}}", "en", R"json({"n":3,"g":"x"})json", "# items" },
	{ "{n, plural, other {{g, select, other {'#' items}}}}", "en", R"json({"n":3,"g":"x"})json", "'#' items" },
	{ "{n, plural, other {{m, plural, other {# inner}} # outer}}", "en", R"json({"n":3,"m":7})json", "7 inner 3 outer" },
	{ "{n, plural, other {{name} has # items}}", "en", R"json({"n":2,"name":"Bob"})json", "Bob has 2 items" },
	{ "{n, plural, one {} other {x}}", "en", R"json({"n":1})json", "" },
	{ "{g, select, male {he} female {she} other {they}}", "en", R"json({"g":"female"})json", "she" },
	{ "{g, select, male {he} female {she} other {they}}", "en", R"json({"g":"robot"})json", "they" },
	{ "{b, select, true {yes} other {no}}", "en", R"json({"b":true})json", "yes" },
	{ "{n, select, 1 {one} other {other}}", "en", R"json({"n":1})json", "one" },
	{ "{g, select, other {don''t}}", "en", R"json({"g":"x"})json", "don't" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":1})json", "1st" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":2})json", "2nd" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":3})json", "3rd" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":4})json", "4th" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":11})json", "11th" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":12})json", "12th" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":13})json", "13th" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":21})json", "21st" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":22})json", "22nd" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":23})json", "23rd" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":101})json", "101st" },
	{ "{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}", "en", R"json({"n":111})json", "111th" },
	{ "{n, selectordinal, one {#er} other {#e}}", "fr", R"json({"n":1})json", "1er" },
	{ "{n, selectordinal, one {#er} other {#e}}", "fr", R"json({"n":2})json", "2e" },
	{ "{count, plural, one {# neue Zeile} other {# neue Zeilen}} in {file}", "de", R"json({"count":1,"file":"app.log"})json", "1 neue Zeile in app.log" },
	{ "{count, plural, one {# neue Zeile} other {# neue Zeilen}} in {file}", "de", R"json({"count":12,"file":"app.log"})json", "12 neue Zeilen in app.log" },
	{ "{n, plural, one {un fichier} other {# fichiers}}", "fr", R"json({"n":0})json", "un fichier" },
	{ "{n, plural, one {un fichier} other {# fichiers}}", "fr", R"json({"n":1})json", "un fichier" },
	{ "{n, plural, one {un fichier} other {# fichiers}}", "fr", R"json({"n":1.5})json", "un fichier" },
	{ "{n, plural, one {un fichier} other {# fichiers}}", "fr", R"json({"n":2})json", "2 fichiers" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "ru", R"json({"n":1})json", "one" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "ru", R"json({"n":3})json", "few" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "ru", R"json({"n":5})json", "many" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "ru", R"json({"n":21})json", "one" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "ru", R"json({"n":1.5})json", "other" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "pl", R"json({"n":2})json", "few" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "pl", R"json({"n":12})json", "many" },
	{ "{n, plural, one {one} few {few} many {many} other {other}}", "pl", R"json({"n":22})json", "few" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":0})json", "zero" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":1})json", "one" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":2})json", "two" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":3})json", "few" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":11})json", "many" },
	{ "{n, plural, zero {zero} one {one} two {two} few {few} many {many} other {other}}", "ar", R"json({"n":100})json", "other" },
	{ "{n, plural, one {one} other {other}}", "ja", R"json({"n":1})json", "other" },
	{ "{n, plural, one {one} other {other}}", "de-DE", R"json({"n":1})json", "one" },
	{ "{n, plural, one {one} other {other}}", "en", R"json({"n":-1})json", "one" },
	{ "{n, plural, one {#} other {#}}", "en", R"json({"n":-3})json", "-3" },
	{ "{ n , plural , one { # x } other { # y } }", "en", R"json({"n":1})json", " 1 x " },
	{ "{n, number}", "en", R"json({"n":7})json", "7" },
};

} // end of anonymous namespace

TEST_CASE("KMessageFormat")
{
	SECTION("parity with intl-messageformat")
	{
		for (const auto& E : Expectations)
		{
			INFO(kFormat("message '{}' in {} with {}", E.sMessage, E.sLanguage, E.sArgs));
			KMessageFormat Message(E.sMessage, E.sLanguage);
			CHECK ( Message.HasError() == false );
			CHECK ( Message.Format(kjson::Parse(E.sArgs)) == E.sExpected );
		}
	}

	SECTION("arguments")
	{
		KMessageFormat Message("{count, plural, one {# item of {total}} other {# items of {total}}} for {name}, {name} again");
		REQUIRE ( Message.HasError() == false );
		CHECK ( (Message.GetArguments() == std::vector<KString>{ "count", "total", "name" }) );
		CHECK ( Message.GetLanguage()  == "en" );
		CHECK ( Message.empty()        == false );
	}

	SECTION("missing and null arguments")
	{
		// intl-messageformat throws here, we show what was meant
		KMessageFormat Message("{a} and {b} and {c, plural, other {# things}}");
		REQUIRE ( Message.HasError() == false );
		CHECK ( (Message.Format({ { "a", 1 } })                  == "1 and {b} and {c}") );
		CHECK ( (Message.Format({ { "a", nullptr }, { "b", 2 } }) == " and 2 and {c}")    );
		CHECK ( Message.Format()                                  == "{a} and {b} and {c}" );
	}

	SECTION("numbers print without grouping in this stage")
	{
		// the browser writes "1,234" here - documented in the header, changes with locale data
		KMessageFormat Message("{n, plural, one {# item} other {# items}}");
		REQUIRE ( Message.HasError() == false );
		CHECK ( (Message.Format({ { "n", 1234 } })   == "1234 items")   );
		CHECK ( (Message.Format({ { "n", 1234.5 } }) == "1234.5 items") );
	}

	SECTION("argument values of all json types")
	{
		KMessageFormat Message("{v}");
		REQUIRE ( Message.HasError() == false );
		CHECK ( (Message.Format({ { "v", "text" } })                               == "text")                 );
		CHECK ( (Message.Format({ { "v", -42 } })                                  == "-42")                  );
		CHECK ( (Message.Format({ { "v", std::numeric_limits<uint64_t>::max() } }) == "18446744073709551615") );
		CHECK ( (Message.Format({ { "v", 3.0 } })                                  == "3")                    );
		CHECK ( (Message.Format({ { "v", true } })                                 == "true")                 );
		CHECK ( (Message.Format({ { "v", nullptr } })                              == "")                     );
		CHECK ( (Message.Format({ { "v", KJSON::array({ 1, 2 }) } })               == "[1,2]")                );
	}

	SECTION("plural with a value that is no number")
	{
		KMessageFormat Message("{n, plural, one {one} other {other: #}}");
		REQUIRE ( Message.HasError() == false );
		CHECK ( (Message.Format({ { "n", "many" } }) == "other: many") );
	}

	SECTION("reparse")
	{
		KMessageFormat Message("{a}");
		REQUIRE ( Message.HasError() == false );
		CHECK ( Message.Parse("{b} {c}", "de") == true );
		CHECK ( (Message.GetArguments() == std::vector<KString>{ "b", "c" }) );
		CHECK ( Message.GetLanguage()  == "de" );
		CHECK ( (Message.Format({ { "b", 1 }, { "c", 2 } }) == "1 2") );
	}

	SECTION("syntax errors")
	{
		struct Bad { KStringView sMessage; KStringView sReason; };

		constexpr Bad Errors[] =
		{
			{ "{n, plural, one {x}}",                 "no other"                },
			{ "{g, select, a {A}}",                   "no other"                },
			{ "{n, plural, one {x} one {y} other {z}}", "duplicate selector"    },
			{ "{n, plural, =1.5 {x} other {y}}",      "exact selector not an integer" },
			{ "{n, plural, some {x} other {y}}",      "no plural category"      },
			{ "{n, plural, one {x} other {y}",        "missing brace"           },
			{ "{n, plural, one {x other {y}}",        "missing brace in branch" },
			{ "a } b",                                "unexpected brace"        },
			{ "{}",                                   "empty name"              },
			{ "{n plural}",                           "no comma"                },
			{ "{n, date}",                            "unsupported type"        },
			{ "{n, number, percent}",                 "unsupported style"       },
			{ "{ n , plural , offset : 1 other {#} }", "space before the offset colon, as in intl-messageformat" },
		};

		for (const auto& E : Errors)
		{
			INFO(kFormat("'{}': {}", E.sMessage, E.sReason));
			KMessageFormat Message(E.sMessage);
			CHECK ( Message.HasError() == true  );
			CHECK ( Message.empty()    == true  );
			CHECK ( (Message.Format({ { "n", 1 }, { "g", "a" } }).empty() == true) );
		}
	}

	SECTION("empty message")
	{
		KMessageFormat Message("");
		CHECK ( Message.HasError() == false );
		CHECK ( Message.empty()    == true  );
		CHECK ( Message.Format()   == ""    );

		KMessageFormat Default;
		CHECK ( Default.empty()    == true  );
	}
}
