/*
 //-----------------------------------------------------------------------------//
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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
#include <array>

namespace dekaf2 {

class KUTICSearchElement;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Class that holds a single UTIC element
class KUTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUTICElement() = default;

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
	bool Matches(const KUTICSearchElement& Element) const;

//------
protected:
//------

	/// protected constructor to allow a child class to explicitly set the stack
	KUTICElement(KStringView sList)
	: m_sList(sList)
	{
	}

	KString m_sList { "/" };

}; // KUTICElement

inline
bool operator==(const KUTICElement& left, const KUTICElement& right)
{
	return left.Element() == right.Element();
}

inline
bool operator!=(const KUTICElement& left, const KUTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// single search element, derived from KUTICElement, but without stack logic
class KUTICSearchElement : private KUTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Construct and set the search element
	KUTICSearchElement(KStringView sSearch = KStringView{})
	: KUTICElement(sSearch)
	{
	}

	KUTICSearchElement(const char* sSearch)
	: KUTICSearchElement(KStringView(sSearch))
	{
	}

	KUTICSearchElement(const KString& sSearch)
	: KUTICSearchElement(KStringView(sSearch))
	{
	}

	using base = KUTICElement;
	using base::clear;
	using base::Element;

//------
private:
//------

}; // KUTICSearchElement

inline
bool KUTICElement::Matches(const KUTICSearchElement& Element) const
{
	return m_sList.Contains(Element.Element());
}

inline
bool operator==(const KUTICElement& left, const KUTICSearchElement& right)
{
	return left.Matches(right);
}

inline
bool operator==(const KUTICSearchElement& left, const KUTICElement& right)
{
	return operator==(right, left);
}

inline
bool operator!=(const KUTICElement& left, const KUTICSearchElement& right)
{
	return !operator==(left, right);
}

inline
bool operator!=(const KUTICSearchElement& left, const KUTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// class that holds URL, tag, ID, and class to compare with in KUTIC
class KUTICSearcher
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUTICSearcher() = default;

	/// Construct a KUTICSearcher object that can be matched against any KUTIC
	KUTICSearcher(bool bPositiveMatch,
				  KStringView sURL,
				  KUTICSearchElement Tags,
				  KUTICSearchElement IDs,
				  KUTICSearchElement Classes)
	: m_sURL(sURL)
	, m_Tags(std::move(Tags))
	, m_IDs(std::move(IDs))
	, m_Classes(std::move(Classes))
	, m_bPositiveMatch(bPositiveMatch)
	{
	}

	KStringView URL() const { return m_sURL; }
	const KUTICSearchElement& Tags()    const { return m_Tags;    }
	const KUTICSearchElement& IDs()     const { return m_IDs;     }
	const KUTICSearchElement& Classes() const { return m_Classes; }
	bool PositiveMatch() const { return m_bPositiveMatch; }

//------
private:
//------

	KString            m_sURL;
	KUTICSearchElement m_Tags;
	KUTICSearchElement m_IDs;
	KUTICSearchElement m_Classes;
	bool               m_bPositiveMatch;

}; // KUTICSearcher

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KUTIC class which holds URL, tag stack, ID stack and class stack
/// for each HTML content block while parsing
class KUTIC
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

	KUTIC() = default;
	KUTIC(KStringView sURL)
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
	const KUTICElement& Tags()    const { return m_TIC[Tag];   }
	/// Returns the ID stack as flat string
	const KUTICElement& IDs()     const { return m_TIC[ID];    }
	/// Returns the Class stack as flat string
	const KUTICElement& Classes() const { return m_TIC[Class]; }

	/// Returns true if all three search strings match the HTML node.
	/// Empty search always matches.
	bool Matches(const KUTICSearcher& Searcher) const;

//------
private:
//------

	std::array<KUTICElement, 3> m_TIC;
	std::size_t m_iDepth { 0 };
	KString m_sURL;

}; // KUTIC

bool operator==(const KUTIC& left, const KUTICSearcher& right);
bool operator==(const KUTICSearcher& left, const KUTIC& right);
bool operator!=(const KUTIC& left, const KUTICSearcher& right);
bool operator!=(const KUTICSearcher& left, const KUTIC& right);

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
	/// @param Searcher a KUTICSearcher object, which is basically constructed from four strings for U T I C stacks
	void AddUTIC(KUTICSearcher Searcher)
	{
		m_UTICs.push_back(std::move(Searcher));
	}

	/// Check all stored UTIC definitions for a match
	/// @param bDefaultMatches determines the result for a stack that did not match any definition
	bool MatchesUTICS(bool bDefaultMatches = true) const;

	/// Check all UTIC definitions in Searchers for a match
	/// @param Searchers vector of KUTICSearcher objects with the search definitions
	/// @param bDefaultMatches determines the result for a stack that did not match any definition
	bool MatchesUTICS(const std::vector<KUTICSearcher>& Searchers, bool bDefaultMatches = true) const;

//------
protected:
//------

	virtual void Object(KHTMLObject& Object) override;

//------
private:
//------

	// the UTIC search masks to either include or exclude
	std::vector<KUTICSearcher> m_UTICs;

	// the UTIC stacks as we parse through the HTML
	KUTIC m_UTIC;

}; // KHTMLUTICParser


} // end of namespace dekaf2

