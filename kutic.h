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
#include "kregex.h"
#include <array>

namespace dekaf2 {

class DEKAF2_PUBLIC KTICElement;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds one T, I, or C element with the current stack of a DOM traversal
class KParsedTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default constructor that uses '/' as the stack delimiter character
	KParsedTICElement() = default;
	/// constructor with a selectable stack delimiter character
	KParsedTICElement(char chStackDelimiter)
	: m_chStackDelimiter(chStackDelimiter)
	{
	}
	/// constructor to allow to explicitly set the stack
	KParsedTICElement(KString sStack,
					  char    chStackDelimiter = '/');

	/// Add stack value for each traversed HTML node
	void Add(KStringView sStackElement);
	/// Reduces stack by one level when traversing down a tree
	bool Reduce();
	/// Clears the stack
	void clear();
	/// Returns the stack as flat string
	const KString& Stack() const { return m_sStack; }
	/// Returns true if the search matches the stack.
	/// Empty search always matches.
	bool Matches(const KTICElement& Element) const;
	/// Returns stack depth
	std::size_t CountDepth() const;

//------
protected:
//------

	char      m_chStackDelimiter { '/' };
	KString   m_sStack           { m_chStackDelimiter };

}; // KParsedTICElement

inline
bool operator==(const KParsedTICElement& left, const KParsedTICElement& right)
{
	return left.Stack() == right.Stack();
}

inline
bool operator!=(const KParsedTICElement& left, const KParsedTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds one T, I, or C search element
class DEKAF2_PUBLIC KTICElement
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KTICElement() = default;

	/// Construct and set the search element from any string view like type (as a regular expression pattern)
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	KTICElement(T sRegex)
	: m_Regex(sRegex)
	{
	}

	/// Returns true if the search matches the stack.
	/// Empty search always matches.
	bool Matches(const KParsedTICElement& Element) const
	{
		return m_Regex.Matches(Element.Stack());
	}

	/// Returns the regular expression pattern of this element
	KStringView Regex() const
	{
		return m_Regex.Pattern();
	}

	/// Returns true if the regular expression is empty
	bool empty() const
	{
		return m_Regex.empty();
	}

//------
private:
//------

	KRegex m_Regex{""};

}; // KTICElement

inline
bool operator==(const KTICElement& left, const KParsedTICElement& right)
{
	return left.Matches(right);
}
inline
bool operator==(const KParsedTICElement& left, const KTICElement& right)
{
	return operator==(right, left);
}
inline
bool operator!=(const KParsedTICElement& left, const KTICElement& right)
{
	return !operator==(left, right);
}
inline
bool operator!=(const KTICElement& left, const KParsedTICElement& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// class that holds URL, tag, ID, and class to compare with in KParsedUTIC
class DEKAF2_PUBLIC KUTIC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUTIC() = default;

	/// Construct a KUTIC object that can be matched against any KParsedUTIC
	KUTIC(KString     sURL,
		  KTICElement Tags,
		  KTICElement IDs,
		  KTICElement Classes,
		  KString     sCSSSelector,
		  bool bPositiveMatch = true)
	: m_Tags           (std::move(Tags)        )
	, m_IDs            (std::move(IDs)         )
	, m_Classes        (std::move(Classes)     )
	, m_sURL           (std::move(sURL)        )
	, m_sCSSSelector   (std::move(sCSSSelector))
	, m_bPositiveMatch (bPositiveMatch         )
	{
	}

	KUTIC(KTICElement Tags,
		  KTICElement IDs,
		  KTICElement Classes,
		  KString     sCSSSelector = KString{},
		  bool        bPositiveMatch = true)
	: m_Tags           (std::move(Tags)        )
	, m_IDs            (std::move(IDs)         )
	, m_Classes        (std::move(Classes)     )
	, m_sCSSSelector   (std::move(sCSSSelector))
	, m_bPositiveMatch (bPositiveMatch         )
	{
	}

	/// construct from a json object with U T I C X properties
	KUTIC(const KJSON& jUTIC, bool bPositiveMatch = true);

	/// returns the URL substring pattern to match
	const KString&     URL()           const { return m_sURL;           }
	/// returns the Tag regex pattern to match
	const KTICElement& Tags()          const { return m_Tags;           }
	/// returns the ID regex pattern to match
	const KTICElement& IDs()           const { return m_IDs;            }
	/// returns the Class regex pattern to match
	const KTICElement& Classes()       const { return m_Classes;        }
	/// returns the CSS selector to match
	const KString&     CSSSelector()   const { return m_sCSSSelector;   }
	/// returns payload json data for this KUTIC matcher
	const KJSON&       Payload()       const { return m_jPayload;       }
	/// is this a positive or negative match rule
	bool               PositiveMatch() const { return m_bPositiveMatch; }
	/// returns true if all rules are empty
	bool               empty()         const;

	/// load UTICs from a stream, comma separated U, T, I, C [, X] line by line, append to existing list
	static bool Append(std::vector<KUTIC>& UTICs, KInStream& InStream, bool bPositiveMatch = true);
	/// load UTICs from a file, comma separated U, T, I, C [, X] line by line, append to existing list
	static bool Append(std::vector<KUTIC>& UTICs, KStringViewZ sFileName, bool bPositiveMatch = true);
	/// give a json array with objects containing U T I C [X] properties, append to existing list
	static bool Append(std::vector<KUTIC>& UTICs, const KJSON& json, bool bPositiveMatch = true);

	/// load UTICs from a stream, comma separated U, T, I, C [, X] line by line
	static std::vector<KUTIC> Load(KInStream& InStream, bool bPositiveMatch = true);
	/// load UTICs from a file, comma separated U, T, I, C [, X] line by line
	static std::vector<KUTIC> Load(KStringViewZ sFileName, bool bPositiveMatch = true);
	/// give a json array with objects containing U T I C [X] properties
	static std::vector<KUTIC> Load(const KJSON& json, bool bPositiveMatch = true);

	/// returns a regex string for UTICs and replaces the occurence of the name alone with the empty regex
	static const KStringView GetUTICValue(const KJSON& json, KStringView sName);

//------
private:
//------

	KTICElement m_Tags;
	KTICElement m_IDs;
	KTICElement m_Classes;
	KString     m_sURL;
	KString     m_sCSSSelector;
	KJSON       m_jPayload;
	bool        m_bPositiveMatch { true };

}; // KUTIC

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KParsedUTIC class which holds URL, tag stack, ID stack and class stack
/// for each HTML content block while parsing
class DEKAF2_PUBLIC KParsedUTIC
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using DepthType = uint16_t;
	static constexpr auto MAX_DEPTH = std::numeric_limits<DepthType>::max();

	KParsedUTIC() = default;
	KParsedUTIC(KStringView sURL)
	: m_sURL(sURL)
	{
	}
	/// constructor to allow to explicitly set the stacks (normally done through Add() / Reduce())
	KParsedUTIC(KString           sURL,
				KParsedTICElement Tags,
				KParsedTICElement IDs,
				KParsedTICElement Classes,
				DepthType         iDepth = 0);

	/// Add tag, id and class values for each traversed HTML node
	void Add(KStringView sTag, KStringView sID, KStringView sClass);
	/// Reduces stacks by one level when traversing down a tree
	bool Reduce();
	/// Returns current stack depth
	std::size_t Depth()                 const { return m_iDepth;  }
	/// Clears the UTIC stacks
	void clear();

	///  Returns the URL as flat string
	const KString&            URL()     const { return m_sURL;    }
	/// Returns the tag stack as flat string
	const KParsedTICElement& Tags()     const { return m_Tags;    }
	/// Returns the ID stack as flat string
	const KParsedTICElement& IDs()      const { return m_IDs;     }
	/// Returns the Class stack as flat string
	const KParsedTICElement& Classes()  const { return m_Classes; }

	/// Returns true if all three regex patterns and the URL match the HTML node.
	/// Empty search always matches. This implementation does not match the
	/// CSS selector of the searcher. For that functionality, subclass KParsedUTIC
	/// and implement a different Matches()
	bool Matches(const KUTIC& Searcher) const;

//------
private:
//------

	KParsedTICElement m_Tags;
	KParsedTICElement m_IDs;
	KParsedTICElement m_Classes;
	KString           m_sURL;
	DepthType         m_iDepth { 0 };

}; // KParsedUTIC

bool operator==(const KParsedUTIC& left, const KParsedUTIC& right);
inline
bool operator!=(const KParsedUTIC& left, const KParsedUTIC& right)
{
	return !operator==(left, right);
}

inline
bool operator==(const KParsedUTIC& left, const KUTIC& right)
{
	return left.Matches(right);
}
inline
bool operator==(const KUTIC& left, const KParsedUTIC& right)
{
	return operator==(right, left);
}
inline
bool operator!=(const KParsedUTIC& left, const KUTIC& right)
{
	return !operator==(left, right);
}
inline
bool operator!=(const KUTIC& left, const KParsedUTIC& right)
{
	return !operator==(left, right);
}

bool operator==(const KParsedUTIC& left, const std::vector<KUTIC>& right);
inline
bool operator==(const std::vector<KUTIC>& left, const KParsedUTIC& right)
{
	return operator==(right, left);
}
inline
bool operator!=(const KParsedUTIC& left, const std::vector<KUTIC>& right)
{
	return !operator==(left, right);
}
inline
bool operator!=(const std::vector<KUTIC>& left, const KParsedUTIC& right)
{
	return !operator==(left, right);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A parser that builds the UTIC schema while parsing HTML for content blocks
/// UTIC = URL, tag stack, ID stack, class stack to address or identify specific content blocks
class DEKAF2_PUBLIC KHTMLUTICParser : public KHTMLContentBlocks
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

