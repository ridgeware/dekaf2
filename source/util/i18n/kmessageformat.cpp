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


#include <dekaf2/util/i18n/kmessageformat.h>
#include <dekaf2/util/i18n/kpluralrules.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <cmath>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the parser: one pass over the message, recursive for the branches. The
/// grammar and the quoting follow intl-messageformat's parser, measured against
/// it - see the unit tests
class KMessageFormat::Parser
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	// what encloses a message: the top level, a plural branch, or a select branch -
	// '#' is the count in a plural branch only, and only directly in it
	enum class Context : uint8_t { Top, Plural, Select };

	//-----------------------------------------------------------------------------
	Parser(KStringView sMessage, KMessageFormat& Owner)
	//-----------------------------------------------------------------------------
	: m_sMessage(sMessage)
	, m_Owner(Owner)
	{
	}

	//-----------------------------------------------------------------------------
	bool Parse(Nodes& Result)
	//-----------------------------------------------------------------------------
	{
		if (!ParseMessage(Context::Top, Result))
		{
			return false;
		}

		if (!AtEnd())
		{
			return Fail("unexpected '}'");
		}

		return true;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	bool AtEnd() const
	//-----------------------------------------------------------------------------
	{
		return m_iPos >= m_sMessage.size();
	}

	//-----------------------------------------------------------------------------
	char Peek(std::size_t iAhead = 0) const
	//-----------------------------------------------------------------------------
	{
		return m_iPos + iAhead < m_sMessage.size() ? m_sMessage[m_iPos + iAhead] : '\0';
	}

	//-----------------------------------------------------------------------------
	bool Fail(KStringView sWhat)
	//-----------------------------------------------------------------------------
	{
		m_Owner.SetError(kFormat("{} at offset {} in message '{}'", sWhat, m_iPos, m_sMessage));
		return false;
	}

	//-----------------------------------------------------------------------------
	void SkipSpace()
	//-----------------------------------------------------------------------------
	{
		while (!AtEnd() && KASCII::kIsSpace(Peek()))
		{
			++m_iPos;
		}
	}

	//-----------------------------------------------------------------------------
	// an argument name, type or selector: up to white space or a syntax character
	KStringView ReadWord()
	//-----------------------------------------------------------------------------
	{
		auto iStart = m_iPos;

		while (!AtEnd())
		{
			auto ch = Peek();

			if (KASCII::kIsSpace(ch) || ch == '{' || ch == '}' || ch == ',' || ch == '\'')
			{
				break;
			}

			++m_iPos;
		}

		return m_sMessage.substr(iStart, m_iPos - iStart);
	}

	//-----------------------------------------------------------------------------
	static void AddText(Nodes& Result, KStringView sText)
	//-----------------------------------------------------------------------------
	{
		if (sText.empty())
		{
			return;
		}

		if (!Result.empty() && Result.back().Type == Node::Type::Text)
		{
			Result.back().sText += sText;
		}
		else
		{
			Node Text;
			Text.Type  = Node::Type::Text;
			Text.sText = sText;
			Result.push_back(std::move(Text));
		}
	}

	//-----------------------------------------------------------------------------
	// an apostrophe: an escape when a syntax character follows, else text
	void ParseQuote(Context Ctx, Nodes& Result)
	//-----------------------------------------------------------------------------
	{
		auto chNext = Peek(1);

		if (chNext == '\'')
		{
			// two apostrophes are one
			m_iPos += 2;
			AddText(Result, "'");
			return;
		}

		bool bQuotable = chNext == '{' || chNext == '}' || chNext == '<' || chNext == '>'
		                 || (chNext == '#' && Ctx == Context::Plural);

		if (!bQuotable)
		{
			++m_iPos;
			AddText(Result, "'");
			return;
		}

		// quoted text up to the closing apostrophe, which may be missing;
		// a doubled apostrophe inside is one apostrophe
		++m_iPos;
		KString sQuoted;

		while (!AtEnd())
		{
			auto ch = Peek();

			if (ch == '\'')
			{
				if (Peek(1) == '\'')
				{
					sQuoted += '\'';
					m_iPos += 2;
					continue;
				}

				++m_iPos;
				break;
			}

			sQuoted += ch;
			++m_iPos;
		}

		AddText(Result, sQuoted);
	}

	//-----------------------------------------------------------------------------
	// text and elements up to the end, or up to the '}' that closes a branch
	bool ParseMessage(Context Ctx, Nodes& Result)
	//-----------------------------------------------------------------------------
	{
		while (!AtEnd())
		{
			auto ch = Peek();

			if (ch == '{')
			{
				if (!ParseArgument(Result))
				{
					return false;
				}
			}
			else if (ch == '}')
			{
				if (Ctx == Context::Top)
				{
					return Fail("unexpected '}'");
				}

				// the branch ends, the caller consumes the brace
				return true;
			}
			else if (ch == '#' && Ctx == Context::Plural)
			{
				Node Pound;
				Pound.Type = Node::Type::Pound;
				Result.push_back(std::move(Pound));
				++m_iPos;
			}
			else if (ch == '\'')
			{
				ParseQuote(Ctx, Result);
			}
			else
			{
				// plain text up to the next syntax character
				auto iStart = m_iPos;

				while (!AtEnd())
				{
					ch = Peek();

					if (ch == '{' || ch == '}' || ch == '\'' || (ch == '#' && Ctx == Context::Plural))
					{
						break;
					}

					++m_iPos;
				}

				AddText(Result, m_sMessage.substr(iStart, m_iPos - iStart));
			}
		}

		if (Ctx != Context::Top)
		{
			return Fail("missing '}'");
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	// after the opening brace: {name}, {name, type} or {name, type, ...}
	bool ParseArgument(Nodes& Result)
	//-----------------------------------------------------------------------------
	{
		++m_iPos; // the '{'
		SkipSpace();

		auto sName = ReadWord();

		if (sName.empty())
		{
			return Fail("expected an argument name");
		}

		m_Owner.AddArgument(sName);
		SkipSpace();

		Node Arg;
		Arg.sText = sName;

		if (Peek() == '}')
		{
			++m_iPos;
			Arg.Type = Node::Type::Argument;
			Result.push_back(std::move(Arg));
			return true;
		}

		if (Peek() != ',')
		{
			return Fail("expected ',' or '}' after the argument name");
		}

		++m_iPos;
		SkipSpace();

		auto sType = ReadWord();
		SkipSpace();

		if (sType == "plural" || sType == "selectordinal")
		{
			Arg.Type = sType == "plural" ? Node::Type::Plural : Node::Type::SelectOrdinal;
			return ParseBranches(Arg, Context::Plural, Result);
		}

		if (sType == "select")
		{
			Arg.Type = Node::Type::Select;
			return ParseBranches(Arg, Context::Select, Result);
		}

		if (sType == "number")
		{
			// without a style the number prints like a plain argument
			if (Peek() != '}')
			{
				return Fail("number styles are not supported yet");
			}

			++m_iPos;
			Arg.Type = Node::Type::Argument;
			Result.push_back(std::move(Arg));
			return true;
		}

		return Fail(kFormat("argument type '{}' is not supported", sType));
	}

	//-----------------------------------------------------------------------------
	// the ", offset:1 =0 {...} one {...} other {...}" part of a plural or select
	bool ParseBranches(Node& Arg, Context Ctx, Nodes& Result)
	//-----------------------------------------------------------------------------
	{
		if (Peek() != ',')
		{
			return Fail("expected ',' after the argument type");
		}

		++m_iPos;
		SkipSpace();

		if (Ctx == Context::Plural && m_sMessage.substr(m_iPos).starts_with("offset:"))
		{
			m_iPos += 7;
			SkipSpace();
			auto sOffset = ReadWord();

			if (!kIsInteger(sOffset))
			{
				return Fail("expected a number after 'offset:'");
			}

			Arg.iOffset = sOffset.Int64();
			SkipSpace();
		}

		bool bHaveOther { false };

		while (Peek() != '}')
		{
			if (AtEnd())
			{
				return Fail("missing '}'");
			}

			auto sSelector = ReadWord();

			if (sSelector.empty())
			{
				return Fail("expected a selector");
			}

			if (Ctx == Context::Plural)
			{
				if (sSelector.front() == '=')
				{
					// an exact match, integer digits only - like intl-messageformat
					if (!kIsInteger(sSelector.substr(1), false))
					{
						return Fail(kFormat("expected digits after '=' in selector '{}'", sSelector));
					}
				}
				else if (sSelector != "zero" && sSelector != "one" && sSelector != "two"
				         && sSelector != "few" && sSelector != "many" && sSelector != "other")
				{
					return Fail(kFormat("'{}' is not a plural category", sSelector));
				}
			}

			for (const auto& Have : Arg.Branches)
			{
				if (Have.sSelector == sSelector)
				{
					return Fail(kFormat("duplicate selector '{}'", sSelector));
				}
			}

			SkipSpace();

			if (Peek() != '{')
			{
				return Fail(kFormat("expected '{{' after selector '{}'", sSelector));
			}

			++m_iPos;

			Branch NewBranch;
			NewBranch.sSelector = sSelector;

			if (!ParseMessage(Ctx, NewBranch.Message))
			{
				return false;
			}

			// ParseMessage stopped at the closing brace of the branch
			++m_iPos;

			if (sSelector == "other")
			{
				bHaveOther = true;
			}

			Arg.Branches.push_back(std::move(NewBranch));
			SkipSpace();
		}

		++m_iPos; // the '}' of the argument

		if (!bHaveOther)
		{
			return Fail(kFormat("'{}' has no 'other' branch", Arg.sText));
		}

		Result.push_back(std::move(Arg));
		return true;
	}

	KStringView     m_sMessage;
	KMessageFormat& m_Owner;
	std::size_t     m_iPos { 0 };

}; // KMessageFormat::Parser

//-----------------------------------------------------------------------------
KMessageFormat::KMessageFormat(KStringView sMessage, KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	Parse(sMessage, sLanguage);

} // ctor

//-----------------------------------------------------------------------------
void KMessageFormat::AddArgument(KStringView sName)
//-----------------------------------------------------------------------------
{
	for (const auto& sHave : m_Arguments)
	{
		if (sHave == sName)
		{
			return;
		}
	}

	m_Arguments.emplace_back(sName);

} // AddArgument

//-----------------------------------------------------------------------------
bool KMessageFormat::Parse(KStringView sMessage, KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	ClearError();
	m_Nodes.clear();
	m_Arguments.clear();
	m_sMessage  = sMessage;
	m_sLanguage = sLanguage;

	Parser Parser(m_sMessage, *this);
	Nodes  Result;

	if (!Parser.Parse(Result))
	{
		m_Arguments.clear();
		return false;
	}

	m_Nodes = std::move(Result);
	return true;

} // Parse

//-----------------------------------------------------------------------------
const KJSON* KMessageFormat::Lookup(const KJSON& jArgs, KStringView sName)
//-----------------------------------------------------------------------------
{
	if (jArgs.is_object())
	{
		return jArgs.find(sName) != jArgs.end() ? &jArgs[sName] : nullptr;
	}

	if (jArgs.is_array() && kIsInteger(sName, false))
	{
		auto iIndex = sName.UInt64();
		return iIndex < jArgs.size() ? &jArgs[iIndex] : nullptr;
	}

	return nullptr;

} // Lookup

//-----------------------------------------------------------------------------
KString KMessageFormat::NumberText(double dNumber)
//-----------------------------------------------------------------------------
{
	// the shortest decimal text, integers without a fraction - like JavaScript
	if (std::isfinite(dNumber) && dNumber == std::floor(dNumber) && std::fabs(dNumber) < 9.0e15)
	{
		return kFormat("{}", static_cast<int64_t>(dNumber));
	}

	return kFormat("{}", dNumber);

} // NumberText

//-----------------------------------------------------------------------------
KString KMessageFormat::ValueText(const KJSON& jValue)
//-----------------------------------------------------------------------------
{
	switch (jValue.type())
	{
		case KJSON::value_t::string:
			return jValue.String();

		case KJSON::value_t::number_integer:
			return kFormat("{}", jValue.Int64());

		case KJSON::value_t::number_unsigned:
			return kFormat("{}", jValue.UInt64());

		case KJSON::value_t::number_float:
			return NumberText(jValue.Float());

		case KJSON::value_t::boolean:
			return jValue.Bool() ? "true" : "false";

		case KJSON::value_t::null:
			return {};

		case KJSON::value_t::object:
		case KJSON::value_t::array:
		case KJSON::value_t::binary:
		case KJSON::value_t::discarded:
			return jValue.dump();
	}

	return {};

} // ValueText

//-----------------------------------------------------------------------------
KString KMessageFormat::FormatBranch(const Node& Node, const KJSON& jArgs, KString sValueText, const KString* pPound) const
//-----------------------------------------------------------------------------
{
	// the branch a plural or select takes for the value
	const Branch* pOther  { nullptr };
	const Branch* pChosen { nullptr };
	KString       sPound;

	if (Node.Type == Node::Type::Select)
	{
		for (const auto& Br : Node.Branches)
		{
			if (Br.sSelector == "other")
			{
				pOther = &Br;
			}
			else if (Br.sSelector == sValueText)
			{
				pChosen = &Br;
				break;
			}
		}
	}
	else
	{
		// the number: from the JSON number or a numeric text - else it is no
		// category at all, and prints as given
		double dValue { 0 };
		bool   bNumber = kIsFloat(sValueText) || kIsInteger(sValueText);

		if (bNumber)
		{
			dValue = sValueText.Double();
		}

		// the count in the branches, less the offset
		sPound = bNumber ? NumberText(dValue - static_cast<double>(Node.iOffset)) : sValueText;

		// an exact selector matches the value as written by JavaScript
		auto sExact = bNumber ? "=" + NumberText(dValue) : KString{};

		for (const auto& Br : Node.Branches)
		{
			if (Br.sSelector == "other")
			{
				pOther = &Br;
			}
			else if (!sExact.empty() && Br.sSelector == sExact)
			{
				pChosen = &Br;
				break;
			}
		}

		if (!pChosen)
		{
			auto sCategory = bNumber
				? KPluralRules::Select(m_sLanguage, dValue - static_cast<double>(Node.iOffset),
				                       Node.Type == Node::Type::SelectOrdinal ? KPluralRules::Type::Ordinal : KPluralRules::Type::Cardinal)
				: KStringView("other");

			for (const auto& Br : Node.Branches)
			{
				if (Br.sSelector == sCategory)
				{
					pChosen = &Br;
					break;
				}
			}
		}

		pPound = &sPound;
	}

	if (!pChosen)
	{
		pChosen = pOther;
	}

	KString sOut;

	if (pChosen)
	{
		FormatNodes(pChosen->Message, jArgs, pPound, sOut);
	}

	return sOut;

} // FormatBranch

//-----------------------------------------------------------------------------
void KMessageFormat::FormatNodes(const Nodes& Nodes, const KJSON& jArgs, const KString* pPound, KString& sOut) const
//-----------------------------------------------------------------------------
{
	for (const auto& N : Nodes)
	{
		switch (N.Type)
		{
			case Node::Type::Text:
				sOut += N.sText;
				break;

			case Node::Type::Pound:
				if (pPound)
				{
					sOut += *pPound;
				}
				else
				{
					sOut += '#';
				}
				break;

			case Node::Type::Argument:
			case Node::Type::Plural:
			case Node::Type::SelectOrdinal:
			case Node::Type::Select:
			{
				auto* pValue = Lookup(jArgs, N.sText);

				if (!pValue)
				{
					// the argument is missing: show what was meant
					kDebug(2, "no argument '{}' for message '{}'", N.sText, m_sMessage);
					sOut += '{';
					sOut += N.sText;
					sOut += '}';
					break;
				}

				if (N.Type == Node::Type::Argument)
				{
					sOut += ValueText(*pValue);
				}
				else
				{
					sOut += FormatBranch(N, jArgs, ValueText(*pValue), pPound);
				}
				break;
			}
		}
	}

} // FormatNodes

//-----------------------------------------------------------------------------
KString KMessageFormat::Format(const KJSON& jArgs) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	FormatNodes(m_Nodes, jArgs, nullptr, sOut);
	return sOut;

} // Format

DEKAF2_NAMESPACE_END
