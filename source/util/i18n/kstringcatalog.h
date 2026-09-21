/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
 //
 // +-------------------------------------------------------------------------+
 // | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
 // |/+---------------------------------------------------------------------+/|
 // |/|                                                                     |/|
 // |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
 // |/|                                                                     |/|
 // |\|   OPEN SOURCE LICENSE                                               |\|
 // |/|                                                                     |/|
 // |\|   Permission is hereby granted, free of charge, to any person       |\|
 // |/|   obtaining a copy of this software and associated                  |/|
 // |\|   documentation files (the "Software"), to deal in the              |\|
 // |/|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |\|
 // |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
 // |\|   and to permit persons to whom the Software is furnished to        |\|
 // |/|   do so, subject to the following conditions:                       |/|
 // |\|                                                                     |\|
 // |/|   The above copyright notice and this permission notice shall       |/|
 // |\|   be included in all copies or substantial portions of the          |\|
 // |/|   Software.                                                         |/|
 // |\|                                                                     |\|
 // |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
 // |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */


#pragma once

/// @file kstringcatalog.h
/// catalogs of UI texts, one per language, with fallback and negotiation

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/util/i18n/kmessageformat.h>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The UI texts of an application in several languages: one catalog per
/// language, a flat JSON object of identifier to message, the messages in the
/// ICU MessageFormat syntax of KMessageFormat. The same files serve a browser
/// with intl-messageformat and C++ here.
///
/// Languages are BCP 47 tags ("de", "de-AT", "en-US", "zh-Hant"). A lookup
/// falls back along the tag: "de-AT" to "de", then to the default language,
/// then to the identifier itself, which is shown so that a missing text is
/// visible. For a language that is written in more than one script the chain
/// includes the script CLDR expects for the region: "zh-TW" to "zh-Hant-TW",
/// "zh-Hant", "zh" - see Fallbacks().
/// Negotiate() picks the best available language for an Accept-Language
/// header. After the languages are added the catalog is read-only and may be
/// shared by all threads.
///
/// @code
/// KStringCatalog Catalog("en");
/// Catalog.AddLanguage("en", kjson::Parse(R"({ "list.name": "Name", "tail.lines": "{count, plural, one {# line} other {# lines}}" })"));
/// Catalog.AddLanguage("de", kjson::Parse(R"({ "list.name": "Name", "tail.lines": "{count, plural, one {# Zeile} other {# Zeilen}}" })"));
/// auto T = Catalog.For(Catalog.Negotiate(HTTP.Request.Headers.Get(KHTTPHeader::ACCEPT_LANGUAGE)));
/// Page.Add<html::TableHeader>(T("list.name"));
/// Status.AddText(T.Format("tail.lines", { { "count", 3 } }));
/// @endcode
class DEKAF2_PUBLIC KStringCatalog
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the catalog bound to one language, for a page builder
	class DEKAF2_PUBLIC Language
	{
	public:
		Language() = default;
		Language(const KStringCatalog& Catalog, KStringView sLanguage);

		/// the message for the identifier, raw
		KStringView operator()(KStringView sID) const;
		/// the message formatted with named arguments
		KString     Format(KStringView sID, const KJSON& jArgs = KJSON{}) const;
		/// the messages of a namespace as a JSON object, for the page script
		KJSON       GetNamespace(KStringView sPrefix) const;
		/// the language tag this view is bound to
		const KString& GetTag() const { return m_sLanguage; }

	private:
		const KStringCatalog* m_Catalog { nullptr };
		KString               m_sLanguage;

	}; // Language

	/// @param sDefaultLanguage the language a lookup falls back to, and the
	/// reference for Check()
	explicit KStringCatalog(KStringView sDefaultLanguage = "en");

	/// add the messages of one language: a JSON object of identifier to message
	/// (a string). A second call for the same language overrides existing
	/// identifiers. Identifiers that start with an underscore are metadata
	/// (notes for translators, section markers) and are ignored without a
	/// report; other values that are not strings and messages that do not
	/// parse are reported and skipped
	/// @return false when a value was skipped
	bool AddLanguage(KStringView sLanguage, const KJSON& jStrings);
	/// the same, from the JSON text of a catalog file - e.g. one embedded by
	/// dekaf2_embed_strings(). UTF-8, or UTF-16/UTF-32 with a byte order mark
	bool AddLanguage(KStringView sLanguage, KStringView sJSON);
	/// take over every language and message of another catalog - its messages
	/// override ours, its languages are added
	void Merge(const KStringCatalog& Other);
	/// add every <tag>.json in the directory, e.g. en.json, de-DE.json, in UTF-8 or in
	/// UTF-16/UTF-32 with a byte order mark. Files whose name is not a language tag are
	/// ignored
	/// @return true when at least one language was added
	bool LoadDirectory(KStringViewZ sDirectory);

	/// the best available language for an Accept-Language header value
	/// ("de-AT,de;q=0.9,en;q=0.8"), or for a comma separated list of tags. A
	/// requested "de" is served by an available "de-DE" when there is no "de",
	/// a requested "zh-Hant" by an available "zh-TW"; nothing acceptable gives
	/// the default language
	KString Negotiate(KStringView sAcceptLanguage) const;
	/// the available languages, canonical tags, sorted
	std::vector<KString> GetLanguages() const;
	/// is this language available, directly or through one of its Fallbacks()?
	/// "de-CH" is when "de" is there, "fr" is not when only "fr-FR" is there
	bool HasLanguage(KStringView sLanguage) const;
	/// the default language
	const KString& GetDefaultLanguage() const { return m_sDefaultLanguage; }

	/// the raw message for the identifier, with the fallback chain - the
	/// identifier itself when no language has it
	KStringView Get(KStringView sLanguage, KStringView sID) const;
	/// the message formatted with named arguments, e.g. { { "count", 3 }, { "file", "app.log" } }
	KString Format(KStringView sLanguage, KStringView sID, const KJSON& jArgs = KJSON{}) const;
	/// the raw messages of one namespace ("tail" for "tail.connected", "tail.gone", ..)
	/// as a JSON object with the identifiers without the prefix - complete along
	/// the fallback chain, so that the page has every text
	KJSON GetNamespace(KStringView sLanguage, KStringView sPrefix) const;
	/// the catalog bound to one language
	Language For(KStringView sLanguage) const;

	/// compares every language with the default language: identifiers missing
	/// or surplus, messages that do not parse, messages whose argument names
	/// differ from the default language's
	/// @return an object with one entry per language that has findings, e.g.
	/// { "de": { "missing": [..], "surplus": [..], "invalid": [..], "arguments": [..] } } -
	/// empty when every language is complete
	KJSON Check() const;

	/// the canonical form of a language tag: "de_at" is "de-AT", "zh-hant-tw" is
	/// "zh-Hant-TW". Empty for text that is no tag
	static KString CanonicalTag(KStringView sLanguage);
	/// the language subtag of a tag: "de" for "de-AT"
	static KStringView LanguageOf(KStringView sLanguage);
	/// the script CLDR expects for a language in a region, for languages that are
	/// written in more than one script: "Hans" for zh, "Hant" for zh in TW, "Latn"
	/// for sr in ME. Empty for languages with one script
	static KStringView LikelyScript(KStringView sLanguage, KStringView sRegion = KStringView{});
	/// the tags a lookup tries for a language, most specific first: the canonical
	/// tag, the tag with the script of LikelyScript(), the shorter tags - "zh-TW",
	/// "zh-Hant-TW", "zh-Hant", "zh". Empty for text that is no tag
	static std::vector<KString> Fallbacks(KStringView sLanguage);

//----------
private:
//----------

	using Messages = KUnorderedMap<KString, KMessageFormat>;

	// the languages to look in for a tag, most specific first
	std::vector<const Messages*> Chain(KStringView sLanguage) const;
	const KMessageFormat*        Find (KStringView sLanguage, KStringView sID) const;
	void                         ReportMissing(KStringView sLanguage, KStringView sID) const;

	KUnorderedMap<KString, Messages> m_Languages;
	KString                          m_sDefaultLanguage;
	mutable std::mutex               m_ReportMutex;
	mutable KUnorderedSet<KString>   m_Reported;

}; // KStringCatalog

DEKAF2_NAMESPACE_END
