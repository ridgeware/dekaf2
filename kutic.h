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

#pragma once

#include "khtmlcontentblocks.h"
#include "kjson.h"
#include <array>

namespace dekaf2 {

class KUTICElement;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class that holds a single UTIC element
class KParsedUTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KParsedUTICElement() = default;

	/// Add stack value for each traversed HTML node
	void Add(KStringView sElement);
	/// Reduces stack by one level when traversing down a tree
	bool Reduce();
	/// Clears the stack
	void clear() { m_sList = '/'; }
	/// Returns the stack as flat string
	KStringView Element() const { return m_sList; }
	/// Returns true if the search matches the stack.
	/// Empty search always matches.
	bool Matches(const KUTICElement& Element) const;

//------
protected:
//------

	/// protected constructor to allow a child class to explicitly set the stack
	KParsedUTICElement(KStringView sList)
	: m_sList(sList)
	{
	}

	KString m_sList { "/" };

}; // KParsedUTICElement

inline
bool operator==(const KParsedUTICElement& left, const KParsedUTICElement& right)
{
	return left.Element() == right.Element();
}

inline
bool operator!=(const KParsedUTICElement& left, const KParsedUTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// single search element, derived from KParsedUTICElement, but without stack logic
class KUTICElement : private KParsedUTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Construct and set the search element
	KUTICElement(KStringView sSearch = KStringView{})
	: KParsedUTICElement(sSearch)
	{
	}

	KUTICElement(const char* sSearch)
	: KUTICElement(KStringView(sSearch))
	{
	}

	KUTICElement(const KString& sSearch)
	: KUTICElement(KStringView(sSearch))
	{
	}

	using base = KParsedUTICElement;
	using base::clear;
	using base::Element;

//------
private:
//------

}; // KUTICElement

inline
bool KParsedUTICElement::Matches(const KUTICElement& Element) const
{
	return m_sList.Contains(Element.Element());
}

inline
bool operator==(const KParsedUTICElement& left, const KUTICElement& right)
{
	return left.Matches(right);
}

inline
bool operator==(const KUTICElement& left, const KParsedUTICElement& right)
{
	return operator==(right, left);
}

inline
bool operator!=(const KParsedUTICElement& left, const KUTICElement& right)
{
	return !operator==(left, right);
}

inline
bool operator!=(const KUTICElement& left, const KParsedUTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// class that holds URL, tag, ID, and class to compare with in KParsedUTIC
class KUTIC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUTIC() = default;

	/// Construct a KUTIC object that can be matched against any KParsedUTIC
	KUTIC(bool bPositiveMatch,
		  KStringView sURL,
		  KUTICElement Tags,
		  KUTICElement IDs,
		  KUTICElement Classes)
	: m_sURL(sURL)
	, m_Tags(std::move(Tags))
	, m_IDs(std::move(IDs))
	, m_Classes(std::move(Classes))
	, m_bPositiveMatch(bPositiveMatch)
	{
	}

	KStringView URL() const { return m_sURL; }
	const KUTICElement& Tags()    const { return m_Tags;    }
	const KUTICElement& IDs()     const { return m_IDs;     }
	const KUTICElement& Classes() const { return m_Classes; }
	bool PositiveMatch() const { return m_bPositiveMatch; }

	/// load UTICs from a file, comma separated U, T, I, C line by line, append to existing list
	static bool AppendFromFile(std::shared_ptr<std::vector<KUTIC>>& UTICs, KStringViewZ sFileName);
	/// give a json array with objects containing U T I C properties, append to existing list
	static bool AppendFromJSON(std::shared_ptr<std::vector<KUTIC>>& UTICs, bool bInclude, const KJSON& json);

	/// load UTICs from a file, comma separated U, T, I, C line by line
	static std::shared_ptr<std::vector<KUTIC>> LoadFromFile(KStringViewZ sFileName);
	/// give a json array with objects containing U T I C properties
	static std::shared_ptr<std::vector<KUTIC>> LoadFromJSON(bool bInclude, const KJSON& json);

//------
private:
//------

	KString      m_sURL;
	KUTICElement m_Tags;
	KUTICElement m_IDs;
	KUTICElement m_Classes;
	bool         m_bPositiveMatch;

}; // KUTIC

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KParsedUTIC class which holds URL, tag stack, ID stack and class stack
/// for each HTML content block while parsing
class KParsedUTIC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Index
	{
		Tag   = 0,
		ID    = 1,
		Class = 2
	};

	KParsedUTIC() = default;
	KParsedUTIC(KStringView sURL)
	: m_sURL(sURL)
	{
	}

	/// Add tag, id and class values for each traversed HTML node
	void Add(KStringView sTag, KStringView sID, KStringView sClass);
	/// Reduces stacks by one level when traversing down a tree
	bool Reduce();
	/// Returns current stack depth
	std::size_t Depth() const { return m_iDepth; }
	/// Clears the UTIC stacks
	void clear();

	///  Returns the URL as flat string
	KStringView URL()             const { return m_sURL;       }
	/// Returns the tag stack as flat string
	const KParsedUTICElement& Tags()    const { return m_TIC[Tag];   }
	/// Returns the ID stack as flat string
	const KParsedUTICElement& IDs()     const { return m_TIC[ID];    }
	/// Returns the Class stack as flat string
	const KParsedUTICElement& Classes() const { return m_TIC[Class]; }

	/// Returns true if all three search strings match the HTML node.
	/// Empty search always matches.
	bool Matches(const KUTIC& Searcher) const;

//------
private:
//------

	std::array<KParsedUTICElement, 3> m_TIC;
	std::size_t m_iDepth { 0 };
	KString m_sURL;

}; // KParsedUTIC

bool operator==(const KParsedUTIC& left, const KUTIC& right);
bool operator==(const KUTIC& left, const KParsedUTIC& right);
bool operator!=(const KParsedUTIC& left, const KUTIC& right);
bool operator!=(const KUTIC& left, const KParsedUTIC& right);

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A parser that builds the UTIC schema while parsing HTML for content blocks
/// UTIC = URL, tag stack, ID stack, class stack to address or identify specific content blocks
class KHTMLUTICParser : public KHTMLContentBlocks
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Add UTIC search definitions that will be checked when calling MatchesUTICS().
	/// @param Searcher a KUTIC object, which is basically constructed from four strings for U T I C stacks
	void AddUTIC(KUTIC Searcher);

	/// Set a set of UTIC search definitions that will be checked when calling MatchesUTICS().
	/// Overrides any UTIC added with AddUTIC()
	void SetUTICs(std::shared_ptr<std::vector<KUTIC>>& SearchSet) { m_SharedUTICs = SearchSet; }

	/// Check all stored UTIC definitions for a match
	/// @param bDefaultMatches determines the result for a stack that did not match any definition
	bool MatchesUTICs(bool bDefaultMatches = true) const;

	/// Check all UTIC definitions in Searchers for a match
	/// @param Searchers vector of KUTIC objects with the search definitions
	/// @param bDefaultMatches determines the result for a stack that did not match any definition
	bool MatchesUTICs(const std::vector<KUTIC>& Searchers, bool bDefaultMatches = true) const;

//------
protected:
//------

	virtual void Object(KHTMLObject& Object) override;

//------
private:
//------

	// the UTIC search masks to either include or exclude
	std::shared_ptr<std::vector<KUTIC>> m_SharedUTICs;

	// the UTIC stacks as we parse through the HTML
	KParsedUTIC m_UTIC;

}; // KHTMLUTICParser


} // end of namespace dekaf2

