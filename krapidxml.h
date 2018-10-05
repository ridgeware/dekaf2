/*
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

#include "kstringview.h"
#include "kstring.h"
#include "kreader.h"
#include "kwriter.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class RapidXMLDocument
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using StringView   = KStringView;
	using String       = KString;
	using XMLPosition  = void*;

	//-----------------------------------------------------------------------------
	RapidXMLDocument();
	//-----------------------------------------------------------------------------

	RapidXMLDocument(const RapidXMLDocument&) = delete;
	RapidXMLDocument(RapidXMLDocument&&) = default;
	RapidXMLDocument& operator=(const RapidXMLDocument&) = delete;
	RapidXMLDocument& operator=(RapidXMLDocument&&) = default;

	//-----------------------------------------------------------------------------
	void AddNode(StringView sName, StringView sValue = StringView{}, bool bDescendInto = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void AddAttribute(StringView sName, StringView sValue = StringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void AddValue(StringView sValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool GetNode(StringView sName, StringView& sValue, bool bDescendInto = false) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	StringView GetNode(StringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool GetSibling(StringView sName, StringView& sValue, bool bPrevious = false) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool GetAttribute(StringView sName, StringView& sValue) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	StringView GetAttribute(StringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void GetValue(StringView& sValue) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	StringView GetValue() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	StringView GetName() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	XMLPosition GetPosition() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetPosition(XMLPosition position) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void StartNode() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool DescendNode(StringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool AscendNode() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool NextSibling(StringView sName, bool bPrevious = false) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void AddXMLDeclaration();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Serialize(KOutStream& OutStream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Serialize(String& string) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	String Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& InStream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Parse(StringView string);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	void Parse();
	//-----------------------------------------------------------------------------

	// helper types to allow for a unique_ptr<void>, which lets us hide all
	// rapidxml headers from the interface
	using deleter_t = std::function<void(void *)>;
	using unique_void_ptr = std::unique_ptr<void, deleter_t>;

	unique_void_ptr D;
	mutable void* P;
	std::vector<char> XMLData;

}; // XMLDocument

} // end of namespace dekaf2
