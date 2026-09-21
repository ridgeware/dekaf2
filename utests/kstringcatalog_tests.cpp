#include "catch.hpp"

#include <dekaf2/util/i18n/kstringcatalog.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/strings/kutf.h>
// the catalogs in utests/strings/, embedded by dekaf2_embed_strings() in the CMake file
#include <utest_strings.h>

using namespace dekaf2;

namespace {

constexpr KStringView sEnglish = R"json({
	"login.title":    "Sign in",
	"login.user":     "User name",
	"login.failed":   "Wrong user name or password.",
	"tail.connected": "Connected",
	"tail.newLines":  "{count, plural, one {# new line} other {# new lines}} in {file}",
	"note.savedBy":   "Note saved by {user, select, window {this window} other {{user}}}",
	"only.english":   "English only"
})json";

constexpr KStringView sGerman = R"json({
	"login.title":    "Anmeldung",
	"login.user":     "Benutzername",
	"login.failed":   "Benutzername oder Passwort falsch.",
	"tail.connected": "Verbunden",
	"tail.newLines":  "{count, plural, one {# neue Zeile} other {# neue Zeilen}} in {file}",
	"note.savedBy":   "Notiz gespeichert von {user, select, window {diesem Fenster} other {{user}}}"
})json";

constexpr KStringView sAustrian = R"json({
	"login.title":    "Anmeldung (AT)"
})json";

constexpr KStringView sFrench = R"json({
	"login.title":    "Connexion",
	"login.user":     "Nom d'utilisateur",
	"login.failed":   "Nom d'utilisateur ou mot de passe incorrect.",
	"tail.connected": "Connecté",
	"tail.newLines":  "{count, plural, one {# nouvelle ligne} other {# nouvelles lignes}} dans {file}",
	"note.savedBy":   "Note enregistrée par {user, select, window {cette fenêtre} other {{user}}}",
	"only.english":   "L'anglais n'est pas le seul"
})json";

// the catalog holds a mutex and is neither copied nor moved - it is filled in place
void FillCatalog(KStringCatalog& Catalog)
{
	REQUIRE ( Catalog.AddLanguage("en",    kjson::Parse(sEnglish))  == true );
	REQUIRE ( Catalog.AddLanguage("de",    kjson::Parse(sGerman))   == true );
	REQUIRE ( Catalog.AddLanguage("de-AT", kjson::Parse(sAustrian)) == true );
	REQUIRE ( Catalog.AddLanguage("fr-FR", kjson::Parse(sFrench))   == true );
}

} // end of anonymous namespace

TEST_CASE("KStringCatalog")
{
	SECTION("language tags")
	{
		CHECK ( KStringCatalog::CanonicalTag("de")          == "de"         );
		CHECK ( KStringCatalog::CanonicalTag("DE")          == "de"         );
		CHECK ( KStringCatalog::CanonicalTag("de-at")       == "de-AT"      );
		CHECK ( KStringCatalog::CanonicalTag("de_AT")       == "de-AT"      );
		CHECK ( KStringCatalog::CanonicalTag(" en-US ")     == "en-US"      );
		CHECK ( KStringCatalog::CanonicalTag("zh-hant-tw")  == "zh-Hant-TW" );
		CHECK ( KStringCatalog::CanonicalTag("es-419")      == "es-419"     );
		CHECK ( KStringCatalog::CanonicalTag("")            == ""           );
		CHECK ( KStringCatalog::CanonicalTag("locales")     == "locales"    ); // looks like a tag, the loader checks the content
		CHECK ( KStringCatalog::CanonicalTag("de-")         == ""           );
		CHECK ( KStringCatalog::CanonicalTag("d3")          == ""           );
		CHECK ( KStringCatalog::CanonicalTag("toolongsubtag") == ""         );
		CHECK ( KStringCatalog::LanguageOf("de-AT")         == "de"         );
		CHECK ( KStringCatalog::LanguageOf("de")            == "de"         );
		CHECK ( KStringCatalog::LanguageOf("zh_Hant_TW")    == "zh"         );

		// the script a region implies, from the CLDR data
		CHECK ( KStringCatalog::LikelyScript("zh")          == "Hans"       );
		CHECK ( KStringCatalog::LikelyScript("zh", "CN")    == "Hans"       );
		CHECK ( KStringCatalog::LikelyScript("zh", "SG")    == "Hans"       );
		CHECK ( KStringCatalog::LikelyScript("zh", "TW")    == "Hant"       );
		CHECK ( KStringCatalog::LikelyScript("zh", "HK")    == "Hant"       );
		CHECK ( KStringCatalog::LikelyScript("ZH", "tw")    == "Hant"       ); // any case
		CHECK ( KStringCatalog::LikelyScript("sr")          == "Cyrl"       );
		CHECK ( KStringCatalog::LikelyScript("sr", "ME")    == "Latn"       );
		CHECK ( KStringCatalog::LikelyScript("de")          == ""           ); // one script
		CHECK ( KStringCatalog::LikelyScript("de", "AT")    == ""           );
		CHECK ( KStringCatalog::LikelyScript("")            == ""           );

		// the tags a lookup tries, most specific first
		CHECK ( (KStringCatalog::Fallbacks("de")         == std::vector<KString>{ "de" }) );
		CHECK ( (KStringCatalog::Fallbacks("de-AT")      == std::vector<KString>{ "de-AT", "de" }) );
		CHECK ( (KStringCatalog::Fallbacks("de_at")      == std::vector<KString>{ "de-AT", "de" }) );
		CHECK ( (KStringCatalog::Fallbacks("de-Latn-AT") == std::vector<KString>{ "de-Latn-AT", "de-AT", "de-Latn", "de" }) );
		CHECK ( (KStringCatalog::Fallbacks("es-419")     == std::vector<KString>{ "es-419", "es" }) );
		CHECK ( (KStringCatalog::Fallbacks("zh")         == std::vector<KString>{ "zh", "zh-Hans" }) );
		CHECK ( (KStringCatalog::Fallbacks("zh-CN")      == std::vector<KString>{ "zh-CN", "zh-Hans-CN", "zh-Hans", "zh" }) );
		CHECK ( (KStringCatalog::Fallbacks("zh-TW")      == std::vector<KString>{ "zh-TW", "zh-Hant-TW", "zh-Hant", "zh" }) );
		CHECK ( (KStringCatalog::Fallbacks("zh-Hant-TW") == std::vector<KString>{ "zh-Hant-TW", "zh-TW", "zh-Hant", "zh" }) );
		CHECK ( (KStringCatalog::Fallbacks("zh-Hans-TW") == std::vector<KString>{ "zh-Hans-TW", "zh-Hans", "zh" }) ); // Simplified for Taiwan is not zh-TW
		CHECK ( (KStringCatalog::Fallbacks("zh-Hant")    == std::vector<KString>{ "zh-Hant", "zh" }) );
		CHECK ( (KStringCatalog::Fallbacks("sr-ME")      == std::vector<KString>{ "sr-ME", "sr-Latn-ME", "sr-Latn", "sr" }) );
		CHECK ( KStringCatalog::Fallbacks("d3").empty()  == true );
	}

	KStringCatalog Catalog("en");
	FillCatalog(Catalog);

	SECTION("languages")
	{
		CHECK ( (Catalog.GetLanguages() == std::vector<KString>{ "de", "de-AT", "en", "fr-FR" }) );
		CHECK ( Catalog.GetDefaultLanguage() == "en" );
		CHECK ( Catalog.HasLanguage("de")    == true  );
		CHECK ( Catalog.HasLanguage("de-CH") == true  ); // through "de"
		CHECK ( Catalog.HasLanguage("fr-FR") == true  );
		CHECK ( Catalog.HasLanguage("fr")    == false ); // only fr-FR is there, HasLanguage asks for the tag or its base
		CHECK ( Catalog.HasLanguage("it")    == false );
		CHECK ( Catalog.HasLanguage("")      == false );
	}

	SECTION("lookup with fallback")
	{
		CHECK ( Catalog.Get("de",    "login.title")    == "Anmeldung"      );
		CHECK ( Catalog.Get("de-AT", "login.title")    == "Anmeldung (AT)" ); // the region has it
		CHECK ( Catalog.Get("de-AT", "login.user")     == "Benutzername"   ); // the language has it
		CHECK ( Catalog.Get("de-AT", "only.english")   == "English only"   ); // the default has it
		CHECK ( Catalog.Get("de-CH", "login.title")    == "Anmeldung"      ); // no de-CH, its language
		CHECK ( Catalog.Get("de",    "no.such.text")   == "no.such.text"   ); // nobody has it: the id
		CHECK ( Catalog.Get("it",    "login.title")    == "Sign in"        ); // unknown language: the default
		CHECK ( Catalog.Get("",      "login.title")    == "Sign in"        );
		CHECK ( Catalog.Get("fr",    "login.title")    == "Sign in"        ); // fr is not fr-FR for a lookup, Negotiate() maps it
		CHECK ( Catalog.Get("fr-FR", "login.user")     == "Nom d'utilisateur" );
	}

	SECTION("negotiation")
	{
		CHECK ( Catalog.Negotiate("de")                        == "de"    );
		CHECK ( Catalog.Negotiate("de-AT")                     == "de-AT" );
		CHECK ( Catalog.Negotiate("de-CH")                     == "de"    );
		CHECK ( Catalog.Negotiate("fr")                        == "fr-FR" ); // a regional catalog serves the language
		CHECK ( Catalog.Negotiate("fr-CA")                     == "fr-FR" );
		CHECK ( Catalog.Negotiate("it")                        == "en"    ); // nothing acceptable: the default
		CHECK ( Catalog.Negotiate("")                          == "en"    );
		CHECK ( Catalog.Negotiate("*")                         == "en"    );
		CHECK ( Catalog.Negotiate("it, de;q=0.8, en;q=0.9")    == "en"    ); // by weight
		CHECK ( Catalog.Negotiate("it, de;q=0.9, en;q=0.8")    == "de"    );
		CHECK ( Catalog.Negotiate("de-AT,de;q=0.9,en;q=0.8")   == "de-AT" );
		CHECK ( Catalog.Negotiate("fr-CH, fr;q=0.9, en;q=0.8") == "fr-FR" );
		CHECK ( Catalog.Negotiate("de;q=0, en")                == "en"    ); // q=0 excludes
		CHECK ( Catalog.Negotiate("xx, yy;q=0.5, *;q=0.1")     == "en"    );
		CHECK ( Catalog.Negotiate("DE_at")                     == "de-AT" ); // canonicalized
	}

	SECTION("languages with more than one script")
	{
		// catalogs named by script, as Apple does it
		KStringCatalog Chinese("en");
		Chinese.AddLanguage("en",      kjson::Parse(R"({ "login.title": "Sign in" })"));
		Chinese.AddLanguage("zh-Hans", kjson::Parse(R"({ "login.title": "登录" })"));
		Chinese.AddLanguage("zh-Hant", kjson::Parse(R"({ "login.title": "登入" })"));

		CHECK ( Chinese.Negotiate("zh")         == "zh-Hans" );
		CHECK ( Chinese.Negotiate("zh-CN")      == "zh-Hans" );
		CHECK ( Chinese.Negotiate("zh-SG")      == "zh-Hans" );
		CHECK ( Chinese.Negotiate("zh-TW")      == "zh-Hant" );
		CHECK ( Chinese.Negotiate("zh-HK")      == "zh-Hant" );
		CHECK ( Chinese.Negotiate("zh-Hant-TW") == "zh-Hant" );
		CHECK ( Chinese.Negotiate("zh-TW, zh;q=0.9, en;q=0.8") == "zh-Hant" );
		CHECK ( Chinese.Get("zh-TW", "login.title") == "登入" );
		CHECK ( Chinese.Get("zh-CN", "login.title") == "登录" );
		CHECK ( Chinese.Get("zh",    "login.title") == "登录" );
		CHECK ( Chinese.HasLanguage("zh-TW") == true );
		CHECK ( Chinese.HasLanguage("zh")    == true );

		// catalogs named by region, as the web does it
		KStringCatalog Regional("en");
		Regional.AddLanguage("en",    kjson::Parse(R"({ "login.title": "Sign in" })"));
		Regional.AddLanguage("zh-CN", kjson::Parse(R"({ "login.title": "登录" })"));
		Regional.AddLanguage("zh-TW", kjson::Parse(R"({ "login.title": "登入" })"));

		CHECK ( Regional.Negotiate("zh")         == "zh-CN" ); // the variant in the language's usual script
		CHECK ( Regional.Negotiate("zh-Hans")    == "zh-CN" );
		CHECK ( Regional.Negotiate("zh-Hant")    == "zh-TW" ); // the variant in the requested script
		CHECK ( Regional.Negotiate("zh-Hant-TW") == "zh-TW" );
		CHECK ( Regional.Negotiate("zh-HK")      == "zh-TW" ); // no zh-HK, the variant in its script
		CHECK ( Regional.Get("zh-Hant-TW", "login.title") == "登入"    );
		CHECK ( Regional.Get("zh-Hant",    "login.title") == "Sign in" ); // a lookup does not search variants, Negotiate() does
	}

	SECTION("format")
	{
		CHECK ( (Catalog.Format("en", "tail.newLines", { { "count", 1 }, { "file", "app.log" } }) == "1 new line in app.log")     );
		CHECK ( (Catalog.Format("de", "tail.newLines", { { "count", 3 }, { "file", "app.log" } }) == "3 neue Zeilen in app.log")  );
		CHECK ( (Catalog.Format("de-AT", "tail.newLines", { { "count", 1 }, { "file", "x" } })    == "1 neue Zeile in x")         );
		CHECK ( (Catalog.Format("fr-FR", "tail.newLines", { { "count", 0 }, { "file", "x" } })    == "0 nouvelle ligne dans x")   ); // French: 0 is singular
		CHECK ( (Catalog.Format("en", "note.savedBy", { { "user", "window" } })                   == "Note saved by this window") );
		CHECK ( (Catalog.Format("de", "note.savedBy", { { "user", "alice" } })                    == "Notiz gespeichert von alice") );
		CHECK ( Catalog.Format("de", "login.title")                                                == "Anmeldung"   );
		CHECK ( Catalog.Format("de", "no.such.text")                                               == "no.such.text" );
		// French apostrophes are text, not quotes
		CHECK ( Catalog.Format("fr-FR", "login.failed")                                            == "Nom d'utilisateur ou mot de passe incorrect." );
		CHECK ( Catalog.Format("fr-FR", "only.english")                                            == "L'anglais n'est pas le seul" );
	}

	SECTION("namespace for the page")
	{
		auto jTail = Catalog.GetNamespace("de-AT", "tail");
		CHECK ( jTail.is_object()              == true );
		CHECK ( jTail.size()                   == 2    );
		CHECK ( jTail["connected"].String()    == "Verbunden" );
		CHECK ( jTail["newLines"].String()     == "{count, plural, one {# neue Zeile} other {# neue Zeilen}} in {file}" );

		// the chain fills what a language lacks: de-AT has only the title, the rest comes from de and en
		auto jLogin = Catalog.GetNamespace("de-AT", "login.");
		CHECK ( jLogin.size()                  == 3    );
		CHECK ( jLogin["title"].String()       == "Anmeldung (AT)" );
		CHECK ( jLogin["user"].String()        == "Benutzername"   );

		auto jOnly = Catalog.GetNamespace("de", "only");
		CHECK ( jOnly["english"].String()      == "English only" );

		CHECK ( Catalog.GetNamespace("de", "nothing").empty() == true );
		// the whole catalog with an empty prefix
		CHECK ( Catalog.GetNamespace("en", "").size() == 7 );
	}

	SECTION("bound language")
	{
		auto T = Catalog.For("de-AT");
		CHECK ( T.GetTag()          == "de-AT"          );
		CHECK ( T("login.title")    == "Anmeldung (AT)" );
		CHECK ( T("login.user")     == "Benutzername"   );
		CHECK ( (T.Format("tail.newLines", { { "count", 2 }, { "file", "y" } }) == "2 neue Zeilen in y") );
		CHECK ( T.GetNamespace("tail").size() == 2 );

		auto Default = Catalog.For("");
		CHECK ( Default.GetTag()       == "en"      );
		CHECK ( Default("login.title") == "Sign in" );

		auto Negotiated = Catalog.For(Catalog.Negotiate("fr-CA, en;q=0.5"));
		CHECK ( Negotiated.GetTag()       == "fr-FR"     );
		CHECK ( Negotiated("login.title") == "Connexion" );

		KStringCatalog::Language Unbound;
		CHECK ( Unbound("x.y") == "x.y" );
		CHECK ( Unbound.Format("x.y") == "x.y" );
	}

	SECTION("check: complete languages")
	{
		KStringCatalog Complete("en");
		Complete.AddLanguage("en", kjson::Parse(sEnglish));
		Complete.AddLanguage("fr", kjson::Parse(sFrench));
		CHECK ( Complete.Check().empty() == true );
	}

	SECTION("check: findings")
	{
		// de lacks only.english; de-AT lacks everything but the title
		auto jCheck = Catalog.Check();
		CHECK ( jCheck.contains("en")  == false );
		CHECK ( jCheck.contains("fr-FR") == false );
		CHECK ( (jCheck["de"]["missing"] == KJSON::array({ "only.english" })) );
		CHECK ( jCheck["de"].contains("surplus")   == false );
		CHECK ( jCheck["de-AT"]["missing"].size()  == 6     );

		// a message with other arguments, a surplus id, and a broken message
		KStringCatalog Broken("en");
		Broken.AddLanguage("en", kjson::Parse(sEnglish));
		CHECK ( Broken.AddLanguage("de", kjson::Parse(R"json({
			"login.title":    "Anmeldung",
			"login.user":     "Benutzername",
			"login.failed":   "Fehler",
			"tail.connected": "Verbunden",
			"tail.newLines":  "{n, plural, one {# Zeile} other {# Zeilen}}",
			"note.savedBy":   "Gespeichert von {user, select, window {Fenster}}",
			"only.english":   "x",
			"extra.id":       "Extra",
			"not.a.string":   42
		})json")) == false );

		jCheck = Broken.Check();
		CHECK ( (jCheck["de"]["arguments"] == KJSON::array({ "tail.newLines" })) );
		CHECK ( (jCheck["de"]["surplus"]   == KJSON::array({ "extra.id" }))      );
		CHECK ( (jCheck["de"]["invalid"]   == KJSON::array({ "note.savedBy" }))  );
		CHECK ( jCheck["de"].contains("missing") == false ); // not.a.string was skipped, it never was an id
		// a broken message shows as it is, and still parses nothing
		CHECK ( Broken.Format("de", "note.savedBy", { { "user", "x" } }) == "Gespeichert von {user, select, window {Fenster}}" );

		// no default language at all
		KStringCatalog Empty("xx");
		CHECK ( Empty.Check().contains("error") == true );
	}

	SECTION("metadata is ignored")
	{
		// an identifier with a leading underscore is a note, not a message: it is
		// neither parsed (braces in prose) nor missing in the other languages
		KStringCatalog Notes("en");
		CHECK ( Notes.AddLanguage("en", kjson::Parse(R"json({
			"_meta.notes":     "identifiers are stable; values are ICU messages like {name} or {count, plural, ..}",
			"_section.errors": "returned by the server as {message: \"..\"}",
			"a":               "one"
		})json")) == true );
		CHECK ( Notes.AddLanguage("de", kjson::Parse(R"({ "a": "eins" })")) == true );
		CHECK ( Notes.Check().empty()        == true );
		CHECK ( Notes.Get("en", "_meta.notes") == "_meta.notes" );
		CHECK ( Notes.GetNamespace("en", "_meta").empty() == true );
	}

	SECTION("adding twice overrides")
	{
		KStringCatalog Twice("en");
		Twice.AddLanguage("en", kjson::Parse(R"({ "a": "one", "b": "two" })"));
		Twice.AddLanguage("en", kjson::Parse(R"({ "a": "uno" })"));
		CHECK ( Twice.Get("en", "a") == "uno" );
		CHECK ( Twice.Get("en", "b") == "two" );
		CHECK ( Twice.AddLanguage("en", kjson::Parse(R"(["not", "an", "object"])")) == false );
		CHECK ( Twice.AddLanguage("not a tag", kjson::Parse(R"({ "a": "x" })"))       == false );
	}

	SECTION("catalog text with a byte order mark")
	{
		// the text of a catalog file in UTF-8 with BOM, UTF-16 and UTF-32 - as an
		// embedded file arrives when it was saved that way
		KStringCatalog Marked("en");
		CHECK ( Marked.AddLanguage("en", KStringView(kutf::Encode<KString>(sEnglish, kutf::Encoding::UTF8)))    == true );
		CHECK ( Marked.AddLanguage("de", KStringView(kutf::Encode<KString>(sGerman,  kutf::Encoding::UTF16BE))) == true );
		CHECK ( Marked.AddLanguage("fr", KStringView(kutf::Encode<KString>(sFrench,  kutf::Encoding::UTF32LE))) == true );
		CHECK ( Marked.Get("en", "login.title") == "Sign in"   );
		CHECK ( Marked.Get("de", "login.title") == "Anmeldung" );
		CHECK ( Marked.Get("fr", "login.title") == "Connexion" );
		CHECK ( Marked.Check().empty() == false ); // fr lacks nothing but de lacks only.english - as in the fixture
	}

	SECTION("embedded by CMake")
	{
		// two files in utests/strings/, byte for byte
		CHECK ( sizeof(utest_strings::kEmbeddedStrings) / sizeof(utest_strings::kEmbeddedStrings[0]) == 2 );
		CHECK ( utest_strings::kEmbeddedStrings[0].sLanguage == "de-DE" );
		CHECK ( utest_strings::kEmbeddedStrings[1].sLanguage == "en"    );

		KString sFile;
		KInFile(kFormat("{}/strings/de-DE.json", DEKAF2_UTEST_SOURCE_DIR)).ReadRemaining(sFile);
		CHECK ( sFile.empty() == false );
		CHECK ( utest_strings::kEmbeddedStrings[0].sJSON == sFile );

		KStringCatalog Embedded("en");
		CHECK ( utest_strings::AddEmbeddedStrings(Embedded) == true );
		CHECK ( (Embedded.GetLanguages() == std::vector<KString>{ "de-DE", "en" }) );
		CHECK ( Embedded.Check().empty() == true );
		// only de-DE is embedded: a lookup for "de" needs the negotiated tag
		CHECK ( Embedded.Negotiate("de") == "de-DE" );
		CHECK ( (Embedded.Format(Embedded.Negotiate("de"), "greeting", { { "name", "Welt" } }) == "Hallo Welt!") );
		CHECK ( (Embedded.Format("en", "lines",    { { "count", 2 } })          == "2 lines")     );
		CHECK ( Embedded.Format("de-DE", "quoted")                               == "Es ist eine {Klammer} und ein <tag>" );

		// a broken JSON text is refused
		CHECK ( Embedded.AddLanguage("fr", KStringView("{ not json")) == false );
	}

	SECTION("load a directory")
	{
		KTempDir Dir;
		kWriteFile(kFormat("{}/en-US.json", Dir.Name()), sEnglish);
		kWriteFile(kFormat("{}/de-DE.json", Dir.Name()), sGerman);
		kWriteFile(kFormat("{}/locales.json", Dir.Name()), R"({ "default": "en-US", "available": ["en-US", "de-DE"] })");
		kWriteFile(kFormat("{}/readme.txt", Dir.Name()), "not a catalog");
		kWriteFile(kFormat("{}/it-IT.json", Dir.Name()), "{ this is not json");
		// a file saved as UTF-16 LE with BOM, as Windows editors do it
		kWriteFile(kFormat("{}/fr-FR.json", Dir.Name()), kutf::Encode<KString>(sFrench, kutf::Encoding::UTF16LE));

		KStringCatalog Loaded("en");
		CHECK ( Loaded.LoadDirectory(Dir.Name()) == true );
		CHECK ( (Loaded.GetLanguages() == std::vector<KString>{ "de-DE", "en-US", "fr-FR" }) );
		CHECK ( Loaded.Get("fr-FR", "login.title") == "Connexion" );

		// the default "en" is not there, but en-US is: negotiation and lookup find it
		CHECK ( Loaded.Negotiate("en")                    == "en-US" );
		CHECK ( Loaded.Negotiate("de")                    == "de-DE" );
		CHECK ( Loaded.Get("de-DE", "login.title")        == "Anmeldung" );
		CHECK ( Loaded.Get(Loaded.Negotiate("en-GB"), "login.title") == "Sign in" );

		KStringCatalog Nothing("en");
		CHECK ( Nothing.LoadDirectory(kFormat("{}/does-not-exist", Dir.Name())) == false );
	}
}
