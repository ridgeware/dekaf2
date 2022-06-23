/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

#include "kutic.h"
#include "kprops.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KParsedTICElement::KParsedTICElement(KString sStack,
									 char    chStackDelimiter)
//-----------------------------------------------------------------------------
: m_chStackDelimiter (chStackDelimiter )
, m_sStack           (std::move(sStack))
{
	kAssert(!m_sStack.empty(), "stack must hold at least one stack delimiter character");
	kAssert(m_sStack.front() == m_chStackDelimiter, "stack must start with delimiter character");

} // KParsedTICElement ctor

//-----------------------------------------------------------------------------
std::size_t KParsedTICElement::CountDepth() const
//-----------------------------------------------------------------------------
{
	kAssert(!m_sStack.empty(), "stack must hold at least one stack delimiter character");
	kAssert(m_sStack.front() == m_chStackDelimiter, "stack must start with delimiter character");

	return std::count(m_sStack.begin() + 1, m_sStack.end(), m_chStackDelimiter);

} // CountDepth

//-----------------------------------------------------------------------------
void KParsedTICElement::clear()
//-----------------------------------------------------------------------------
{
	m_sStack = m_chStackDelimiter;

} // clear

//-----------------------------------------------------------------------------
void KParsedTICElement::Add(KStringView sStackElement)
//-----------------------------------------------------------------------------
{
	kAssert(!sStackElement.contains(m_chStackDelimiter), kFormat("stack element may not contain a '/': {}", sStackElement));

	m_sStack += sStackElement;
	m_sStack += m_chStackDelimiter;

} // Add

//-----------------------------------------------------------------------------
bool KParsedTICElement::Reduce()
//-----------------------------------------------------------------------------
{
	kAssert(!m_sStack.empty(), "stack is empty, but may never be");

	// we always have at least one char in the list
	KString::size_type iPos = m_sStack.size() - 1;

	if (DEKAF2_UNLIKELY(iPos == 0))
	{
		// prevent a crash nonetheless..
		return false;
	}

	iPos = m_sStack.rfind(m_chStackDelimiter, --iPos);
	// the result will never be npos as we always have a starting slash

	m_sStack.erase(iPos + 1);

	return true;

} // Reduce

//-----------------------------------------------------------------------------
KParsedUTIC::KParsedUTIC(KString           sURL,
						 KParsedTICElement Tags,
						 KParsedTICElement IDs,
						 KParsedTICElement Classes,
						 DepthType         iDepth)
//-----------------------------------------------------------------------------
: m_Tags(std::move(Tags))
, m_IDs(std::move(IDs))
, m_Classes(std::move(Classes))
, m_sURL(sURL)
, m_iDepth(iDepth)
{
	if (m_iDepth == 0)
	{
		m_iDepth = m_Tags.CountDepth();
	}

	kAssert(m_iDepth == m_Tags   .CountDepth(), kFormat("{} real depth differs from set depth", "Tags"   ));
	kAssert(m_iDepth == m_IDs    .CountDepth(), kFormat("{} real depth differs from set depth", "IDs"    ));
	kAssert(m_iDepth == m_Classes.CountDepth(), kFormat("{} real depth differs from set depth", "Classes"));

} // KParsedUTIC ctor

//-----------------------------------------------------------------------------
void KParsedUTIC::Add(KStringView sTag, KStringView sID, KStringView sClass)
//-----------------------------------------------------------------------------
{
	m_Tags   .Add(sTag);
	m_IDs    .Add(sID);
	m_Classes.Add(sClass);

	++m_iDepth;

} // Add

//-----------------------------------------------------------------------------
bool KParsedUTIC::Reduce()
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!m_iDepth))
	{
		return false;
	}

	--m_iDepth;

	return m_Tags   .Reduce()  &&
	       m_IDs    .Reduce()  &&
	       m_Classes.Reduce();

} // Reduce

//-----------------------------------------------------------------------------
void KParsedUTIC::clear()
//-----------------------------------------------------------------------------
{
	m_Tags   .clear();
	m_IDs    .clear();
	m_Classes.clear();

} // clear

//-----------------------------------------------------------------------------
bool KParsedUTIC::Matches(const KUTIC& Searcher) const
//-----------------------------------------------------------------------------
{
	return URL().contains(Searcher.URL())   &&
	       Tags()      == Searcher.Tags()   &&
	       IDs()       == Searcher.IDs()    &&
	       Classes()   == Searcher.Classes();

} // Matches

//-----------------------------------------------------------------------------
bool operator==(const KParsedUTIC& left, const KParsedUTIC& right)
//-----------------------------------------------------------------------------
{
	return left.Depth()   == right.Depth()   &&
	       left.Tags()    == right.Tags()    &&
	       left.IDs()     == right.IDs()     &&
	       left.Classes() == right.Classes() &&
	       left.URL()     == right.URL();

} // operator==

//-----------------------------------------------------------------------------
KUTIC::KUTIC(const KJSON& jUTIC, bool bPositiveMatch)
//-----------------------------------------------------------------------------
: m_Tags           (GetUTICValue(jUTIC, "T"))
, m_IDs            (GetUTICValue(jUTIC, "I"))
, m_Classes        (GetUTICValue(jUTIC, "C"))
, m_sURL           (GetUTICValue(jUTIC, "U"))
, m_sCSSSelector   (GetUTICValue(jUTIC, "X"))
, m_bPositiveMatch (bPositiveMatch)
{
	if (jUTIC.is_object())
	{
		m_jPayload = KJSON::object();
		// now collect all properties that are not any of UTICX
		for (const auto& it : jUTIC.items())
		{
			if (!it.key().In("U,T,I,C,X"))
			{
				m_jPayload.push_back({it.key(), it.value()});
			}
		}
	}

} // KUTIC ctor

//-----------------------------------------------------------------------------
bool KUTIC::Append(std::vector<KUTIC>& UTICs, KInStream& InStream, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	for (KStringView sLine : InStream)
	{
		sLine.TrimLeft();

		if (!sLine.empty())
		{
			if (sLine.front() != '#')
			{
				auto vUTIC = sLine.Split();

				KUTIC UTIC;

				if (vUTIC.size() == 3)
				{
					UTIC = KUTIC(vUTIC[0], vUTIC[1], vUTIC[2], "", bPositiveMatch);
				}
				else if (vUTIC.size() == 4)
				{
					UTIC = KUTIC(vUTIC[0], vUTIC[1], vUTIC[2], vUTIC[3], bPositiveMatch);
				}
				else if (vUTIC.size() == 5)
				{
					UTIC = KUTIC(vUTIC[0], vUTIC[1], vUTIC[2], vUTIC[3], vUTIC[4], bPositiveMatch);
				}

				if (!UTIC.empty())
				{
					UTICs.push_back(std::move(UTIC));
				}
			}
		}
	}

	return true;

} // Append

//-----------------------------------------------------------------------------
bool KUTIC::Append(std::vector<KUTIC>& UTICs, KStringViewZ sFileName, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	KInFile File(sFileName);

	if (!File.is_open())
	{
		kDebug(1, "cannot open: {}", sFileName);
		return false;
	}

	return Append(UTICs, File, bPositiveMatch);

} // Append

//-----------------------------------------------------------------------------
bool KUTIC::Append(std::vector<KUTIC>& UTICs, const KJSON& json, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	if (json.is_array())
	{
		for (auto& it : json)
		{
			KUTIC UTIC(it, bPositiveMatch);

			if (!UTIC.empty())
			{
				UTICs.push_back(std::move(UTIC));
			}
		}
	}
	else
	{
		kDebug(1, "json is not an array of objects");
		return false;
	}

	return true;

} // Append

//-----------------------------------------------------------------------------
std::vector<KUTIC> KUTIC::Load(KInStream& InStream, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	std::vector<KUTIC> UTICs;
	Append(UTICs, InStream);
	return UTICs;

} // Load

//-----------------------------------------------------------------------------
std::vector<KUTIC> KUTIC::Load(KStringViewZ sFileName, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	std::vector<KUTIC> UTICs;
	Append(UTICs, sFileName);
	return UTICs;

} // Load

//-----------------------------------------------------------------------------
std::vector<KUTIC> KUTIC::Load(const KJSON& json, bool bPositiveMatch)
//-----------------------------------------------------------------------------
{
	std::vector<KUTIC> UTICs;
	Append(UTICs, json, bPositiveMatch);
	return UTICs;

} // Load

//-----------------------------------------------------------------------------
const KStringView KUTIC::GetUTICValue(const KJSON& json, KStringView sName)
//-----------------------------------------------------------------------------
{
	KStringView sValue = kjson::GetStringRef(json, sName);
	return (sValue == sName) ? KStringView{} : sValue;

} // GetUTICValue

//-----------------------------------------------------------------------------
bool KUTIC::empty() const
//-----------------------------------------------------------------------------
{
	// do not test for m_sCSSSelector here - as we do not evaluate it, it _is_
	// empty. If that changes in a derived class, overwrite empty() as well
	return m_Tags.empty()    &&
	       m_IDs.empty()     &&
	       m_Classes.empty() &&
	       m_sURL.empty();

} // empty

//-----------------------------------------------------------------------------
bool operator==(const KParsedUTIC& left, const std::vector<KUTIC>& right)
//-----------------------------------------------------------------------------
{
	for (const auto& SearchedUTIC : right)
	{
		if (left.Matches(SearchedUTIC))
		{
			return SearchedUTIC.PositiveMatch();
		}
	}

	return false;
	
} // operator==

//-----------------------------------------------------------------------------
void KHTMLUTICParser::Object(KHTMLObject& Object)
//-----------------------------------------------------------------------------
{
	// call the parent's implementation first - should it flush content
	// we want the UTIC with the current setting, not with the one
	// after the tag
	KHTMLContentBlocks::Object(Object);

	switch (Object.Type())
	{
		case KHTMLTag::TYPE:
		{
			auto& Tag = reinterpret_cast<KHTMLTag&>(Object);

			if (!Tag.IsInline())
			{
				if (Tag.IsOpening())
				{
					m_UTIC.Add(Tag.Name, Tag.Attributes.Get("id"), Tag.Attributes.Get("class"));
				}
				else if (Tag.IsClosing())
				{
					m_UTIC.Reduce();
				}
			}
			break;
		}

		default:
			break;
	}

} // Object

//-----------------------------------------------------------------------------
bool KHTMLUTICParser::MatchesUTICs(const std::vector<KUTIC>& Searchers, bool bDefaultMatches) const
//-----------------------------------------------------------------------------
{
	for (const auto& SearchedUTIC : Searchers)
	{
		if (m_UTIC == SearchedUTIC)
		{
			return SearchedUTIC.PositiveMatch();
		}
	}

	return bDefaultMatches;

} // MatchesUTICS

//-----------------------------------------------------------------------------
bool KHTMLUTICParser::MatchesUTICs(bool bDefaultMatches) const
//-----------------------------------------------------------------------------
{
	if (!m_SharedUTICs)
	{
		return bDefaultMatches;
	}
	return MatchesUTICs(*m_SharedUTICs, bDefaultMatches);

} // MatchesUTICS

//-----------------------------------------------------------------------------
void KHTMLUTICParser::AddUTIC(KUTIC Searcher)
//-----------------------------------------------------------------------------
{
	if (!m_SharedUTICs)
	{
		m_SharedUTICs = std::make_shared<std::vector<KUTIC>>();
	}
	m_SharedUTICs->push_back(std::move(Searcher));

} // AddUTIC

} // end of namespace dekaf2

