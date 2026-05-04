/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
// |/|   Software without restriction, including without limitation        |\|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |\|
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
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |\|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include <dekaf2/data/json/kconfig.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/format/kformat.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KString KConfig::DefaultPath ()
//-----------------------------------------------------------------------------
{
	// kGetConfigPath(false) returns the path without creating the directory
	return kFormat("{}{}config.json", kGetConfigPath(false), kDirSep);

} // DefaultPath

//-----------------------------------------------------------------------------
KConfig::KConfig ()
//-----------------------------------------------------------------------------
: m_sPath(DefaultPath())
{
	// dekaf2 convention: the ctor does the obvious I/O. A missing file is
	// not an error here - Loaded() reports the outcome.
	Load();

} // ctor

//-----------------------------------------------------------------------------
KConfig::KConfig (KStringViewZ sPath)
//-----------------------------------------------------------------------------
: m_sPath(sPath)
{
	Load();

} // ctor with path

//-----------------------------------------------------------------------------
bool KConfig::Load (KStringViewZ sPath/*=""*/)
//-----------------------------------------------------------------------------
{
	if (!sPath.empty())
	{
		m_sPath = sPath;
	}

	m_bLoaded = false;

	if (m_sPath.empty() || !kFileExists(m_sPath))
	{
		return false;
	}

	KInFile fin(m_sPath);
	if (!fin.is_open())
	{
		return false;
	}

	// Parse without throwing - we return false on parse errors
	if (!m_JSON.Parse(fin, /*bThrow=*/false))
	{
		return false;
	}

	m_bLoaded = true;

	return true;

} // Load

//-----------------------------------------------------------------------------
bool KConfig::Save (KStringViewZ sPath/*=""*/)
//-----------------------------------------------------------------------------
{
	if (!sPath.empty())
	{
		m_sPath = sPath;
	}

	if (m_sPath.empty())
	{
		return false;
	}

	// If the target path is inside the standard config directory, let
	// kGetConfigPath(true) create ~/.config/{prog}/ with the canonical
	// restrictive permissions (DEKAF2_MODE_CREATE_CONFIG_DIR = 0700). This
	// avoids duplicating that policy here.
	//
	// For any other user-chosen path (e.g. ~/something/my.cfg) we do NOT
	// want to reduce permissions of intermediate directories to 0700 -
	// that would be surprising. Fall through to the normal directory
	// creation mode, which honors the user's umask.
	const auto& sConfigRoot = kGetConfigPath(/*bCreateDirectory=*/false);
	
	if (!sConfigRoot.empty() && m_sPath.starts_with(sConfigRoot))
	{
		(void) kGetConfigPath(/*bCreateDirectory=*/true);
	}

	// kDirname returns a KStringView into m_sPath, not null-terminated, so
	// copy to a KString to satisfy KStringViewZ-taking filesystem functions.
	auto sDirView = kDirname(m_sPath);

	if (!sDirView.empty() && sDirView != ".")
	{
		KString sDir(sDirView);

		if (!kDirExists(sDir))
		{
			if (!kCreateDir(sDir, DEKAF2_MODE_CREATE_DIR, /*bCreateIntermediates=*/true))
			{
				return false;
			}
		}
	}

	KOutFile fout(m_sPath);

	if (!fout.is_open())
	{
		return false;
	}

	// An untouched KConfig holds a null JSON. For a hand-editable config
	// file, an empty object "{}" is friendlier than a literal "null".
	if (m_JSON.is_null())
	{
		m_JSON = KJSON2::object();
	}

	// Pretty-printed for hand-editability
	m_JSON.Serialize(fout, /*bPretty=*/true);

	return fout.Good();

} // Save

DEKAF2_NAMESPACE_END
