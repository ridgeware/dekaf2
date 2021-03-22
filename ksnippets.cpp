/*
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

#include "ksnippets.h"
#include "kstringstream.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KSnippets::KSnippets(KInStream& InStream, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	Add(InStream, sLeadIn, sLeadOut);

} // KSnippets

//-----------------------------------------------------------------------------
KSnippets::KSnippets(KString sInput, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	Add(sInput, sLeadIn, sLeadOut);

} // KSnippets

//-----------------------------------------------------------------------------
bool KSnippets::Add(KInStream& InStream, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (!kReadAll(InStream, sBuffer, false))
	{
		kDebug(1, "cannot read from input stream");
		return false;
	}

	m_Buffer.push_front(std::move(sBuffer));
	IsolateSnippets(m_Buffer.front(), sLeadIn, sLeadOut);

	return true;

} // Load

//-----------------------------------------------------------------------------
bool KSnippets::Add(KString sInput, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	m_Buffer.push_front(std::move(sInput));
	IsolateSnippets(m_Buffer.front(), sLeadIn, sLeadOut);

	return true;

} // Load

//-----------------------------------------------------------------------------
bool KSnippets::AddFile(KStringViewZ sInputFile, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	KInFile InFile(sInputFile);

	if (!Add(InFile, sLeadIn, sLeadOut))
	{
		kDebug(1, "cannot read from input file: {}", sInputFile);
		return false;
	}

	return true;

} // LoadFile

//-----------------------------------------------------------------------------
void KSnippets::SetOutput(KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	m_OutStream = &OutStream;

} // SetOutput

//-----------------------------------------------------------------------------
void KSnippets::SetOutput(KString& sOutput)
//-----------------------------------------------------------------------------
{
	m_OSS = std::make_unique<KOutStringStream>(sOutput);
	m_OutStream = m_OSS.get();

} // SetOutput

//-----------------------------------------------------------------------------
bool KSnippets::Add(KSnippets&& other)
//-----------------------------------------------------------------------------
{
	for (auto& it : other.m_Buffer)
	{
		m_Buffer.push_front(std::move(it));
	}

	for (auto& it : other.m_Snippets)
	{
		auto pair = m_Snippets.insert(it);

		if (!pair.second)
		{
			pair.first->second = it.second;
		}
	}

	other = KSnippets();

	return true;

} // Add(KSnippets&&)

//-----------------------------------------------------------------------------
void KSnippets::IsolateSnippets(KStringView sBuffer, KStringView sLeadIn, KStringView sLeadOut)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		auto iStart = sBuffer.find(sLeadIn);

		if (iStart == KStringView::npos)
		{
			break;
		}

		sBuffer.remove_prefix(iStart + sLeadIn.size());

		auto sName = sBuffer.Left(sBuffer.find_first_of(" \r\n\b\t"));

		if (sName.empty())
		{
			kDebug(1, "name of opening snippet '{}' is empty here: {}", sLeadIn, sBuffer.LeftUTF8(30));
			break;
		}
		
		sBuffer.remove_prefix(sName.size() + 1);

		KStringView::size_type iEnd { static_cast<KStringView::size_type>(-1) };
		KStringView sSnippet;

		for (;;)
		{
			iEnd = sBuffer.find(sLeadOut, ++iEnd);

			if (iEnd == KStringView::npos)
			{
				kDebug(1, "no closing snippet for open '{}'", sLeadIn);
				return;
			}

			sSnippet = sBuffer.ToView(0, iEnd);

			// now search back until lead in, and check that there is a white space
			auto iBegin = sSnippet.find_last_of(" \r\n\b\t");

			if (iBegin == KStringView::npos)
			{
				// all valid, we reached start of snippet
				break;
			}

			auto iLastLeadIn = sSnippet.rfind(sLeadIn);

			if (iLastLeadIn == KStringView::npos)
			{
				// all valid, we reached start of snippet
				break;
			}

			if (iBegin > iLastLeadIn)
			{
				// valid, this is the end of a lead in
				break;
			}

			// this was an internal replacer without space
		}

		sBuffer.remove_prefix(iEnd + sLeadOut.size());

		kDebug(2, "adding snippet with name: '{}'", sName);

		auto pair = m_Snippets.insert( {sName, sSnippet} );

		if (!pair.second)
		{
			pair.first->second = sSnippet;
		}
	}

} // IsolateSnippets

//-----------------------------------------------------------------------------
bool KSnippets::Compose(KStringView sSnippet, const KReplacer& Replacer)
//-----------------------------------------------------------------------------
{
	if (!m_OutStream)
	{
		return false;
	}

	auto it = m_Snippets.find(sSnippet);

	if (it == m_Snippets.end())
	{
		return false;
	}

	auto sRange = it->second;

	if (Replacer.empty() && !Replacer.GetRemoveAllVariables())
	{
		m_OutStream->Write(sRange);
	}
	else
	{
		m_OutStream->Write(Replacer.Replace(sRange));
	}
	
	return m_OutStream->Good();

} // Compose

//-----------------------------------------------------------------------------
void KSnippets::clear()
//-----------------------------------------------------------------------------
{
	m_Buffer.clear();
	m_Snippets.clear();
	m_OSS.reset();
	m_OutStream = nullptr;

} // clear

// if std::unordered_map is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::unordered_map<int, int>>::value || std::is_nothrow_move_constructible<KSnippets>::value,
			  "KSnippets is intended to be nothrow move constructible, but is not!");

} // of namespace dekaf2

