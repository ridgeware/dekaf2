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
void KParsedUTICElement::Add(KStringView sElement)
//-----------------------------------------------------------------------------
{
	m_sList += sElement;
	m_sList += '/';

} // Add

//-----------------------------------------------------------------------------
bool KParsedUTICElement::Reduce()
//-----------------------------------------------------------------------------
{
	// we always have at least one char in the list
	KString::size_type iPos = m_sList.size() - 1;

	if (DEKAF2_UNLIKELY(iPos == 0))
	{
		return false;
	}

	iPos = m_sList.rfind('/', --iPos);
	// the result will never be npos as we always have a starting slash

	m_sList.erase(iPos + 1);

	return true;

} // Reduce

//-----------------------------------------------------------------------------
void KParsedUTIC::Add(KStringView sTag, KStringView sID, KStringView sClass)
//-----------------------------------------------------------------------------
{
	m_TIC[Tag].Add(sTag);
	m_TIC[ID].Add(sID);
	m_TIC[Class].Add(sClass);

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

	return m_TIC[Tag].Reduce() &&
	       m_TIC[ID].Reduce()  &&
	       m_TIC[Class].Reduce();

} // Reduce

//-----------------------------------------------------------------------------
void KParsedUTIC::clear()
//-----------------------------------------------------------------------------
{
	m_TIC[Tag].clear();
	m_TIC[ID].clear();
	m_TIC[Class].clear();

} // clear

//-----------------------------------------------------------------------------
bool KParsedUTIC::Matches(const KUTIC& Searcher) const
//-----------------------------------------------------------------------------
{
	return *this == Searcher;

} // Matches

//-----------------------------------------------------------------------------
bool KUTIC::AppendFromFile(std::shared_ptr<std::vector<KUTIC>>& UTICs, KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	if (!UTICs)
	{
		auto UTICs = std::make_shared<std::vector<KUTIC>>();
	}

	KInFile File(sFileName);

	if (File.is_open())
	{
		for (KStringView sLine : File)
		{
			sLine.TrimLeft();

			if (!sLine.empty())
			{
				if (sLine.front() != '#')
				{
					auto UTIC = sLine.Split();

					if (UTIC.size() == 4)
					{
						UTICs->push_back({ true, UTIC[0], UTIC[1], UTIC[2], UTIC[3] });
					}
				}
			}
		}
	}
	else
	{
		kDebug(1, "cannot open: {}", sFileName);
		return false;
	}

	return true;

} // LoadFromFile

//-----------------------------------------------------------------------------
bool KUTIC::AppendFromJSON(std::shared_ptr<std::vector<KUTIC>>& UTICs, bool bInclude, const KJSON& json)
//-----------------------------------------------------------------------------
{
	if (!UTICs)
	{
		auto UTICs = std::make_shared<std::vector<KUTIC>>();
	}

	if (json.is_array())
	{
		for (auto& it : json)
		{
			UTICs->push_back(
			{
				bInclude,
				kjson::GetStringRef(it, "U"),
				kjson::GetStringRef(it, "T"),
				kjson::GetStringRef(it, "I"),
				kjson::GetStringRef(it, "C"),
			});
		}
	}
	else
	{
		kDebug(1, "json is not an array of objects");
		return false;
	}

	return true;

} // LoadFromJSON

//-----------------------------------------------------------------------------
std::shared_ptr<std::vector<KUTIC>> KUTIC::LoadFromFile(KStringViewZ sFileName)
//-----------------------------------------------------------------------------
{
	auto UTICs = std::make_shared<std::vector<KUTIC>>();
	AppendFromFile(UTICs, sFileName);
	return UTICs;

} // LoadFromFile

//-----------------------------------------------------------------------------
std::shared_ptr<std::vector<KUTIC>> KUTIC::LoadFromJSON(bool bInclude, const KJSON& json)
//-----------------------------------------------------------------------------
{
	auto UTICs = std::make_shared<std::vector<KUTIC>>();
	AppendFromJSON(UTICs, true, json);
	return UTICs;

} // LoadFromJSON


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
		case KHTMLObject::TAG:
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

bool operator==(const KParsedUTIC& left, const KUTIC& right)
{
	return left.URL().contains(right.URL())   &&
	       left.Tags()      == right.Tags()   &&
	       left.IDs()       == right.IDs()    &&
	       left.Classes()   == right.Classes();
}

bool operator==(const KUTIC& left, const KParsedUTIC& right)
{
	return operator==(right, left);
}

bool operator!=(const KParsedUTIC& left, const KUTIC& right)
{
	return !operator==(left, right);
}

bool operator!=(const KUTIC& left, const KParsedUTIC& right)
{
	return !operator==(left, right);
}


} // end of namespace dekaf2

