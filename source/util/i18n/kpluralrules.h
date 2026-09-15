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

/// @file kpluralrules.h
/// the plural categories of the Unicode CLDR: which form of a message a number takes in a language

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstdint>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The plural rules of the Unicode CLDR, for cardinal numbers ("1 file",
/// "2 files") and ordinal numbers ("1st", "2nd"). Select() names the category
/// a number falls into in a language: "zero", "one", "two", "few", "many" or
/// "other". The rules of all CLDR languages are compiled into the library from
/// from/cldr, no ICU is involved; the rule syntax is the one of Unicode TR35,
/// and Matches() evaluates a rule given as text.
///
/// A language tag falls back to its language subtag ("de-AT" uses the rules of
/// "de"), an unknown language uses the English rules, as a browser does without
/// data for it.
class DEKAF2_PUBLIC KPluralRules
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum class Type : uint8_t { Cardinal, Ordinal };

	/// the operands of a number as the rules see them (TR35): n is the absolute
	/// value, i its integer digits, v and w the number of visible fraction digits
	/// with and without trailing zeros, f and t these digits as integers, e the
	/// exponent of a compact notation - always 0 here. "-1.50" gives n 1.5, i 1,
	/// v 2, w 1, f 50, t 5
	struct Operands
	{
		double  n { 0 };
		int64_t i { 0 };
		int64_t f { 0 };
		int64_t t { 0 };
		int32_t v { 0 };
		int32_t w { 0 };
		int32_t e { 0 };
	};

	/// the category of a number in a language, from the number's shortest decimal
	/// representation - 1.0 is 1, as in JavaScript
	static KStringView Select(KStringView sLanguage, double dNumber, Type Type = Type::Cardinal);
	/// the category of a number given as decimal text - here "1.0" keeps its fraction
	/// digit, which some languages distinguish from 1
	static KStringView Select(KStringView sLanguage, KStringView sNumber, Type Type = Type::Cardinal);
	/// the category for operands prepared with GetOperands()
	static KStringView Select(KStringView sLanguage, const Operands& Operands, Type Type = Type::Cardinal);
	/// the operands of a number in decimal text, e.g. "-1.50"
	static Operands GetOperands(KStringView sNumber);
	/// the operands of a number, from its shortest decimal representation
	static Operands GetOperands(double dNumber);
	/// the categories a language uses, in the order zero, one, two, few, many, other
	static std::vector<KStringView> GetCategories(KStringView sLanguage, Type Type = Type::Cardinal);
	/// does the CLDR have rules for this language, or for its language subtag?
	static bool HasLanguage(KStringView sLanguage, Type Type = Type::Cardinal);
	/// evaluates one rule in TR35 syntax, e.g. "i = 1 and v = 0" or
	/// "n % 10 = 2..4 and n % 100 != 12..14", for the operands. A rule that does
	/// not parse is false
	static bool Matches(KStringView sRule, const Operands& Operands);
	/// the version of the CLDR data compiled in
	static KStringViewZ GetCLDRVersion();

}; // KPluralRules

DEKAF2_NAMESPACE_END
