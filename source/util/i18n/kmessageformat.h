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

/// @file kmessageformat.h
/// messages in the ICU MessageFormat syntax: arguments, plural, select

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <cstdint>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A message in the ICU MessageFormat syntax, as intl-messageformat (FormatJS)
/// reads it, so that one catalog of texts serves the browser and C++ alike:
///
/// - arguments: `{name}` - or `{0}` with an array of arguments
/// - plural: `{count, plural, offset:1 =0 {nobody} one {# person} other {# people}}`,
///   `#` is the count (less the offset), the categories come from KPluralRules
/// - ordinal: `{n, selectordinal, one {#st} two {#nd} few {#rd} other {#th}}`
/// - select: `{gender, select, male {he} female {she} other {they}}`
/// - nested messages with further arguments in every branch; `other` is required
/// - ICU quoting: `'{'`, `'}'`, `'#'` (in a plural branch) and `''` are escapes,
///   any other apostrophe is text ("l'appel")
/// - `<b>...</b>` is text, tags are not interpreted
///
/// The message is parsed once; Format() fills it with the values of a JSON
/// object. Numbers print as their shortest decimal text, without grouping
/// separators - 1234 is "1234" here, where the browser writes "1,234"; locale
/// specific number, date and time formats are not part of this stage, a bare
/// `{n, number}` prints like `{n}`, styles and `{d, date}`, `{t, time}` are
/// rejected. A missing argument prints as `{name}`, a null argument as nothing.
/// Unlike intl-messageformat, which throws on a missing argument.
class DEKAF2_PUBLIC KMessageFormat : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KMessageFormat() = default;
	/// parses the message for the language (a BCP 47 tag, it decides the plural
	/// categories) - check HasError() afterwards
	KMessageFormat(KStringView sMessage, KStringView sLanguage = "en");

	/// parses a new message
	/// @return false on a syntax error, with the error set
	bool Parse(KStringView sMessage, KStringView sLanguage = "en");
	/// the message with the arguments filled in
	/// @param jArgs an object with the argument names, or an array for positional names
	KString Format(const KJSON& jArgs = KJSON{}) const;
	/// the argument names the message uses, in the order of their first appearance
	const std::vector<KString>& GetArguments() const { return m_Arguments; }
	/// the message text as given
	const KString& GetMessage()  const { return m_sMessage;  }
	/// the language the message was parsed for
	const KString& GetLanguage() const { return m_sLanguage; }
	/// true without a message, or without one that parsed
	bool empty() const { return m_Nodes.empty(); }

//----------
private:
//----------

	struct Node;

	struct Branch
	{
		KString           sSelector;
		std::vector<Node> Message;
	};

	struct Node
	{
		enum class Type : uint8_t { Text, Argument, Pound, Plural, SelectOrdinal, Select };

		Type                Type { Type::Text };
		KString             sText;         // the text, or the argument name
		int64_t             iOffset { 0 }; // plural only
		std::vector<Branch> Branches;      // plural and select only
	};

	using Nodes = std::vector<Node>;

	class Parser;
	friend class Parser;

	void    AddArgument(KStringView sName);
	void    FormatNodes(const Nodes& Nodes, const KJSON& jArgs, const KString* pPound, KString& sOut) const;
	KString FormatBranch(const Node& Node, const KJSON& jArgs, KString sValueText, const KString* pPound) const;

	static const KJSON* Lookup(const KJSON& jArgs, KStringView sName);
	static KString      ValueText(const KJSON& jValue);
	static KString      NumberText(double dNumber);

	Nodes                m_Nodes;
	std::vector<KString> m_Arguments;
	KString              m_sMessage;
	KString              m_sLanguage;

}; // KMessageFormat

DEKAF2_NAMESPACE_END
