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


#include <dekaf2/util/i18n/kpluralrules.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include "../../../from/cldr/kcldr_plurals.h"
#include <cmath>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

namespace {

// the categories in the order of TR35
constexpr KStringView sCategories[] = { "zero", "one", "two", "few", "many", "other" };

//-----------------------------------------------------------------------------
// one relation of a rule: operand [% modulus] (= | !=) value or range list
struct Relation
//-----------------------------------------------------------------------------
{
	char    chOperand { 'n' };
	int64_t iModulus  { 0 };     // 0 is none
	bool    bNegate   { false }; // != instead of =
	std::vector<std::pair<int64_t, int64_t>> Ranges; // single values have lo == hi
};

// a rule: relations joined with 'and', groups of them joined with 'or'
using Condition = std::vector<std::vector<Relation>>;

//-----------------------------------------------------------------------------
// the rules of one language: category and condition, "other" has none
struct LanguageRules
//-----------------------------------------------------------------------------
{
	std::vector<std::pair<KStringView, Condition>> Rules;
	std::vector<KStringView>                       Categories;
};

using RuleTable = KUnorderedMap<KString, LanguageRules>;

//-----------------------------------------------------------------------------
// parses a rule in TR35 syntax - false when the text is not a rule
bool ParseRule(KStringView sRule, Condition& Cond)
//-----------------------------------------------------------------------------
{
	Cond.clear();
	Cond.emplace_back();

	// a rule ends at its samples
	auto iAt = sRule.find('@');

	if (iAt != KStringView::npos)
	{
		sRule = sRule.substr(0, iAt);
	}

	sRule.Trim();

	if (sRule.empty())
	{
		// "other": always true
		return true;
	}

	// tokens are separated by spaces in the CLDR data, operators may touch numbers
	auto Tokens = sRule.Split(" ");
	std::size_t i = 0;

	auto Peek = [&](KStringView sWhat) { return i < Tokens.size() && Tokens[i] == sWhat; };

	// each round parses one relation, then an 'and' or 'or'
	while (i < Tokens.size())
	{
		Relation Rel;

		// operand
		if (Tokens[i].size() != 1 || KStringView("nivwfte").find(Tokens[i].front()) == KStringView::npos)
		{
			return false;
		}

		Rel.chOperand = Tokens[i++].front();

		// optional modulus
		if (Peek("%") || Peek("mod"))
		{
			++i;

			if (i >= Tokens.size() || !kIsInteger(Tokens[i], false))
			{
				return false;
			}

			Rel.iModulus = Tokens[i++].Int64();
		}

		// = or != (TR35 also knows 'is', 'in', 'within', 'not' - the CLDR data use = and !=)
		if (Peek("="))
		{
			++i;
		}
		else if (Peek("!="))
		{
			++i;
			Rel.bNegate = true;
		}
		else
		{
			return false;
		}

		// the range list, "0,1" or "2..4" or "0..1,5,7..9" - one token in the data
		if (i >= Tokens.size())
		{
			return false;
		}

		for (auto sRange : Tokens[i++].Split(","))
		{
			auto iDots = sRange.find("..");

			if (iDots == KStringView::npos)
			{
				if (!kIsInteger(sRange, false))
				{
					return false;
				}

				auto iValue = sRange.Int64();
				Rel.Ranges.emplace_back(iValue, iValue);
			}
			else
			{
				auto sLo = sRange.substr(0, iDots);
				auto sHi = sRange.substr(iDots + 2);

				if (!kIsInteger(sLo, false) || !kIsInteger(sHi, false))
				{
					return false;
				}

				Rel.Ranges.emplace_back(sLo.Int64(), sHi.Int64());
			}
		}

		Cond.back().push_back(std::move(Rel));

		if (i >= Tokens.size())
		{
			break;
		}

		if (Peek("and"))
		{
			++i;
		}
		else if (Peek("or"))
		{
			++i;
			Cond.emplace_back();
		}
		else
		{
			return false;
		}

		if (i >= Tokens.size())
		{
			// a dangling and/or
			return false;
		}
	}

	return true;

} // ParseRule

//-----------------------------------------------------------------------------
bool Evaluate(const Relation& Rel, const KPluralRules::Operands& Ops)
//-----------------------------------------------------------------------------
{
	bool bMatch { false };

	if (Rel.chOperand == 'n')
	{
		// the only operand that may have a fraction
		double x = Ops.n;

		if (Rel.iModulus)
		{
			x = std::fmod(x, static_cast<double>(Rel.iModulus));
		}

		bool bInteger = x == std::floor(x);

		for (const auto& Range : Rel.Ranges)
		{
			if (Range.first == Range.second)
			{
				bMatch = x == static_cast<double>(Range.first);
			}
			else
			{
				// a range holds integers only
				bMatch = bInteger && x >= static_cast<double>(Range.first) && x <= static_cast<double>(Range.second);
			}

			if (bMatch)
			{
				break;
			}
		}
	}
	else
	{
		int64_t x { 0 };

		switch (Rel.chOperand)
		{
			case 'i': x = Ops.i; break;
			case 'v': x = Ops.v; break;
			case 'w': x = Ops.w; break;
			case 'f': x = Ops.f; break;
			case 't': x = Ops.t; break;
			case 'e':
			case 'c': x = Ops.e; break;
		}

		if (Rel.iModulus)
		{
			x %= Rel.iModulus;
		}

		for (const auto& Range : Rel.Ranges)
		{
			if (x >= Range.first && x <= Range.second)
			{
				bMatch = true;
				break;
			}
		}
	}

	return Rel.bNegate ? !bMatch : bMatch;

} // Evaluate

//-----------------------------------------------------------------------------
bool Evaluate(const Condition& Cond, const KPluralRules::Operands& Ops)
//-----------------------------------------------------------------------------
{
	for (const auto& AndGroup : Cond)
	{
		bool bAll = true;

		for (const auto& Rel : AndGroup)
		{
			if (!Evaluate(Rel, Ops))
			{
				bAll = false;
				break;
			}
		}

		if (bAll)
		{
			return true;
		}
	}

	return false;

} // Evaluate

//-----------------------------------------------------------------------------
// the canonical category name from the table, so that Select() returns views into static storage
KStringView CategoryName(KStringView sCategory)
//-----------------------------------------------------------------------------
{
	for (auto sName : sCategories)
	{
		if (sName == sCategory)
		{
			return sName;
		}
	}

	return sCategories[5];

} // CategoryName

//-----------------------------------------------------------------------------
// compiles one of the CLDR tables into rules per language
template<std::size_t N>
RuleTable Compile(const KCLDRPluralRule (&Table)[N])
//-----------------------------------------------------------------------------
{
	RuleTable Rules;

	for (const auto& Entry : Table)
	{
		Condition Cond;

		if (!ParseRule(Entry.sText, Cond))
		{
			kDebug(1, "cannot parse CLDR plural rule for {} {}: '{}'", Entry.sLocale, Entry.sCategory, Entry.sText);
			continue;
		}

		auto& Language = Rules[KString(Entry.sLocale)];
		auto  sName    = CategoryName(Entry.sCategory);
		Language.Categories.push_back(sName);

		if (sName != "other")
		{
			Language.Rules.emplace_back(sName, std::move(Cond));
		}
	}

	// the categories in the order of TR35
	for (auto& Language : Rules)
	{
		std::vector<KStringView> Sorted;

		for (auto sName : sCategories)
		{
			for (auto sHave : Language.second.Categories)
			{
				if (sHave == sName)
				{
					Sorted.push_back(sName);
				}
			}
		}

		Language.second.Categories = std::move(Sorted);
	}

	return Rules;

} // Compile

//-----------------------------------------------------------------------------
const RuleTable& Rules(KPluralRules::Type Type)
//-----------------------------------------------------------------------------
{
	static const RuleTable s_Cardinal = Compile(kCLDRCardinalRules);
	static const RuleTable s_Ordinal  = Compile(kCLDROrdinalRules);

	return Type == KPluralRules::Type::Ordinal ? s_Ordinal : s_Cardinal;

} // Rules

//-----------------------------------------------------------------------------
// the rules for a language tag: the tag as the CLDR writes it ("pt-PT"), then
// its language subtag, then English
const LanguageRules* FindLanguage(const RuleTable& Table, KStringView sLanguage, bool bFallbackToEnglish)
//-----------------------------------------------------------------------------
{
	KString sTag(sLanguage);
	sTag.Trim();
	sTag.Replace('_', '-');

	auto iDash = sTag.find('-');

	if (iDash != KString::npos)
	{
		// language lowercase, region uppercase - the data has no script subtags
		KString sNormalized = KStringView(sTag).substr(0, iDash).ToLowerASCII();
		sNormalized += '-';
		sNormalized += KStringView(sTag).substr(iDash + 1).ToUpperASCII();

		auto it = Table.find(sNormalized);

		if (it != Table.end())
		{
			return &it->second;
		}

		sTag.erase(iDash);
	}

	sTag.MakeLowerASCII();

	auto it = Table.find(sTag);

	if (it != Table.end())
	{
		return &it->second;
	}

	if (!bFallbackToEnglish)
	{
		return nullptr;
	}

	kDebug(1, "no CLDR plural rules for '{}', using English", sLanguage);

	it = Table.find("en");
	return it != Table.end() ? &it->second : nullptr;

} // FindLanguage

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KPluralRules::Operands KPluralRules::GetOperands(KStringView sNumber)
//-----------------------------------------------------------------------------
{
	Operands Ops;

	sNumber.Trim();

	if (!sNumber.empty() && (sNumber.front() == '-' || sNumber.front() == '+'))
	{
		sNumber.remove_prefix(1);
	}

	// a compact exponent ("1c6" or "1e6") does not occur in what we format,
	// but the CLDR samples have it - take the mantissa and the exponent
	auto iExp = sNumber.find_first_of("ce");

	if (iExp != KStringView::npos)
	{
		Ops.e = static_cast<int32_t>(sNumber.substr(iExp + 1).Int64());
		sNumber = sNumber.substr(0, iExp);
	}

	auto iDot    = sNumber.find('.');
	auto sInt    = iDot == KStringView::npos ? sNumber : sNumber.substr(0, iDot);
	auto sFrac   = iDot == KStringView::npos ? KStringView{} : sNumber.substr(iDot + 1);

	if (Ops.e > 0)
	{
		// shift the fraction digits into the integer part
		KString sShifted(sInt);
		auto iShift = static_cast<std::size_t>(Ops.e);
		sShifted += sFrac.substr(0, iShift);
		sShifted.append(iShift > sFrac.size() ? iShift - sFrac.size() : 0, '0');
		sFrac = sFrac.size() > iShift ? sFrac.substr(iShift) : KStringView{};
		Ops.i = sShifted.Int64();
	}
	else
	{
		Ops.i = sInt.Int64();
	}

	auto sTrimmed = sFrac;

	while (!sTrimmed.empty() && sTrimmed.back() == '0')
	{
		sTrimmed.remove_suffix(1);
	}

	Ops.v = static_cast<int32_t>(sFrac.size());
	Ops.w = static_cast<int32_t>(sTrimmed.size());
	Ops.f = sFrac.empty()    ? 0 : sFrac.Int64();
	Ops.t = sTrimmed.empty() ? 0 : sTrimmed.Int64();
	Ops.n = static_cast<double>(Ops.i);

	if (Ops.v)
	{
		Ops.n += static_cast<double>(Ops.f) / std::pow(10.0, Ops.v);
	}

	return Ops;

} // GetOperands

//-----------------------------------------------------------------------------
KPluralRules::Operands KPluralRules::GetOperands(double dNumber)
//-----------------------------------------------------------------------------
{
	// the shortest decimal text that reads back to the same value, like
	// JavaScript's Number to string - 1.0 becomes "1", 1.5 stays "1.5"
	auto sNumber = kFormat("{}", std::fabs(dNumber));

	if (sNumber.contains('e') || sNumber.contains("inf") || sNumber.contains("nan"))
	{
		// too large, too small or not a number for a decimal text: the integer part has to do
		Operands Ops;
		Ops.n = std::isfinite(dNumber) ? std::fabs(dNumber) : 0;
		Ops.i = std::isfinite(dNumber) && std::fabs(dNumber) < 9.2e18 ? static_cast<int64_t>(std::fabs(dNumber)) : 0;
		return Ops;
	}

	return GetOperands(sNumber);

} // GetOperands

//-----------------------------------------------------------------------------
KStringView KPluralRules::Select(KStringView sLanguage, const Operands& Ops, Type Type)
//-----------------------------------------------------------------------------
{
	auto* Language = FindLanguage(Rules(Type), sLanguage, true);

	if (Language)
	{
		for (const auto& Rule : Language->Rules)
		{
			if (Evaluate(Rule.second, Ops))
			{
				return Rule.first;
			}
		}
	}

	return sCategories[5];

} // Select

//-----------------------------------------------------------------------------
KStringView KPluralRules::Select(KStringView sLanguage, double dNumber, Type Type)
//-----------------------------------------------------------------------------
{
	return Select(sLanguage, GetOperands(dNumber), Type);

} // Select

//-----------------------------------------------------------------------------
KStringView KPluralRules::Select(KStringView sLanguage, KStringView sNumber, Type Type)
//-----------------------------------------------------------------------------
{
	return Select(sLanguage, GetOperands(sNumber), Type);

} // Select

//-----------------------------------------------------------------------------
std::vector<KStringView> KPluralRules::GetCategories(KStringView sLanguage, Type Type)
//-----------------------------------------------------------------------------
{
	auto* Language = FindLanguage(Rules(Type), sLanguage, true);

	if (!Language)
	{
		return { sCategories[5] };
	}

	return Language->Categories;

} // GetCategories

//-----------------------------------------------------------------------------
bool KPluralRules::HasLanguage(KStringView sLanguage, Type Type)
//-----------------------------------------------------------------------------
{
	return FindLanguage(Rules(Type), sLanguage, false) != nullptr;

} // HasLanguage

//-----------------------------------------------------------------------------
bool KPluralRules::Matches(KStringView sRule, const Operands& Ops)
//-----------------------------------------------------------------------------
{
	Condition Cond;

	if (!ParseRule(sRule, Cond))
	{
		kDebug(1, "not a plural rule: '{}'", sRule);
		return false;
	}

	return Evaluate(Cond, Ops);

} // Matches

//-----------------------------------------------------------------------------
KStringViewZ KPluralRules::GetCLDRVersion()
//-----------------------------------------------------------------------------
{
	return kCLDRVersion;

} // GetCLDRVersion

DEKAF2_NAMESPACE_END
