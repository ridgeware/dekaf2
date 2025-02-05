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

#include "khistory.h"
#include "kfilesystem.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KHistory::LoadHistory(KString sPathname)
//-----------------------------------------------------------------------------
{
	if (sPathname.empty())
	{
		sPathname = kFormat("{}/.config/{}/terminal-history.txt", kGetHome(), kBasename(kGetOwnPathname()));
	}

	// make sure the path exists
	kCreateDir(KString(kDirname(sPathname)));

	m_sHistoryfile = sPathname;

	KInFile File(sPathname);

	if (File.is_open())
	{
		m_History.clear();

		for (auto sHist : File)
		{
			m_History.push_back(sHist);
		}

		ResetIter();

		return true;
	}
	else
	{
		ResetIter();
		kDebug(2, "cannot open history for reading: {}", sPathname);
		return false;
	}

} // SetFile

//-----------------------------------------------------------------------------
void KHistory::SetSize(uint32_t iHistorySize, bool bToDisk, KString sPathname)
//-----------------------------------------------------------------------------
{
	if (bToDisk)
	{
		LoadHistory(std::move(sPathname));
	}

	if (iHistorySize > 0 && iHistorySize < iMinHistory)
	{
		iHistorySize = iMinHistory;
	}

	m_iMaxHistory = iHistorySize;

	CheckSize();

} // SetSize

//-----------------------------------------------------------------------------
bool KHistory::Add(KStringView sLine)
//-----------------------------------------------------------------------------
{
	// neither empty nor duplicate lines are added
	if (m_iMaxHistory > 0 && !sLine.empty() && (m_History.empty() || m_History.back() != sLine))
	{
		CheckSize();

		m_History.push_back(sLine);

		if (!m_sHistoryfile.empty())
		{
			kAppendFile(m_sHistoryfile, sLine + '\n');
		}

		ResetIter();

		return true;
	}

	return false;

} // Add

//-----------------------------------------------------------------------------
void KHistory::Stash(KStringView sLine)
//-----------------------------------------------------------------------------
{
	m_sStash     = sLine;
	m_bHaveStash = true;

	// when stashing we jump to the end of the history
	ResetIter();
}

//-----------------------------------------------------------------------------
KStringView KHistory::PopStashed()
//-----------------------------------------------------------------------------
{
	if (m_bHaveStash)
	{
		m_bHaveStash = false;
		return m_sStash;
	}

	return {};
}

//-----------------------------------------------------------------------------
std::size_t KHistory::Depth() const
//-----------------------------------------------------------------------------
{
	return m_it - m_History.begin();
}

//-----------------------------------------------------------------------------
bool KHistory::HaveOlder() const
//-----------------------------------------------------------------------------
{
	return m_it != m_History.begin();
}

//-----------------------------------------------------------------------------
bool KHistory::HaveNewer() const
//-----------------------------------------------------------------------------
{
	return m_it != m_History.end();
}

//-----------------------------------------------------------------------------
KStringView KHistory::GetOlder()
//-----------------------------------------------------------------------------
{
	return (HaveOlder()) ? (--m_it)->ToView() : KStringViewZ{};
}

//-----------------------------------------------------------------------------
KStringView KHistory::GetNewer()
//-----------------------------------------------------------------------------
{
	if (HaveNewer())
	{
		++m_it;
		if (m_it == m_History.end()) return PopStashed();
		return m_it->ToView();
	}
	return KStringView{};
}

//-----------------------------------------------------------------------------
KStringView KHistory::Find(KStringView sSearch, bool bStartsWith)
//-----------------------------------------------------------------------------
{
	auto it = m_it;
	
	while (it != m_History.begin())
	{
		++it;
		if (bStartsWith)
		{
			if (it->starts_with(sSearch))
			{
				return *it;
			}
		}
		else if (it->contains(sSearch))
		{
			return *it;
		}
	}

	return {};

} // Find

//-----------------------------------------------------------------------------
void KHistory::ResetIter ()
//-----------------------------------------------------------------------------
{
	m_it = m_History.end();
}

//-----------------------------------------------------------------------------
void KHistory::CheckSize()
//-----------------------------------------------------------------------------
{
	if (m_History.size() > m_iMaxHistory)
	{
		if (m_iMaxHistory == 0)
		{
			// just remove all history
			m_History.clear();

			if (!m_sHistoryfile.empty())
			{
				kRemoveFile(m_sHistoryfile);
			}
		}
		else
		{
			// erase 100 more to not repeat it for every new line
			m_History.erase(m_History.begin(), (m_History.begin() + (m_History.size() - (m_iMaxHistory - iDeleteForResize))));

			if (!m_sHistoryfile.empty())
			{
				// flush shortened list to history file
				KOutFile File(m_sHistoryfile, std::ios_base::trunc);

				if (File.is_open())
				{
					for (auto sHist : m_History)
					{
						File.WriteLine(sHist);
					}
				}
				else
				{
					kDebug(1, "cannot open history for writing: {}", m_sHistoryfile);
				}
			}
		}

		ResetIter();
	}

} // CheckSize

DEKAF2_NAMESPACE_END
