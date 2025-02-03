/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

/// @file khistory.h
/// history management

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// class to manage line history in memory and on disk
class KHistory
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	KHistory() = default;

	using Storage = std::vector<KString>;

	/// set a history file - if pathname is empty a program name derived default filename will be used in ~/.config/dekaf2/
	bool           SetFile (KString sPathname = KString{});
	/// set the size for the history, both in memory and on disk
	/// @param iHistorySize the size for the history, minimum 100, default 1000
	void           SetSize (uint32_t iHistorySize);
	/// add latest line to history
	bool           Add     (KStringView sLine);
	/// do we have a stashed line?
	bool           HaveStashed() const { return m_bHaveStash; }
	/// stash the line that was currently edited
	void           Stash   (KStringView sLine);
	/// get the stashed line back
	KStringView    PopStashed();

	/// get current depth in history
	std::size_t    Depth() const;
	/// check if there is a older line in history
	bool           HaveOlder() const;
	/// check if there is a newer line in history
	bool           HaveNewer() const;
	/// get an older line in history
	KStringView    GetOlder();
	/// get a newer line in history
	KStringView    GetNewer();

	/// is history empty?
	bool           empty() const { return m_History.empty(); }
	/// what size does the history have?
	std::size_t    size () const { return m_History.size();  }

	Storage::const_iterator begin () const { return m_History.begin (); }
	Storage::const_iterator end   () const { return m_History.end   (); }

//-------
private:
//-------

	void CheckSize ();
	void ResetIter ();

	KString  m_sHistoryfile;
	KString  m_sStash;
	Storage  m_History;
	Storage::const_iterator m_it { m_History.begin() };
	uint32_t m_iMaxHistory       {  1000 };
	bool     m_bHaveStash        { false };
//	bool     m_bLastWasOlder     { false };

}; // KHistory

DEKAF2_NAMESPACE_END
