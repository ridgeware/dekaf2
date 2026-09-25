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
	ClearError();

	if (m_sPath.empty() || !kFileExists(m_sPath))
	{
		// no config yet - not an error
		return false;
	}

	// The whole file as UTF-8, whatever encoding its byte order mark announces.
	// A config file is small, and the string parser is faster than the stream
	// parser and strict: it refuses content after the JSON value, which the
	// stream parser leaves unread.
	KString sText;

	if (!kReadText(m_sPath, sText))
	{
		m_JSON = KJSON{};
		return SetError(kFormat("cannot read {}", m_sPath));
	}

	KString sError;

	if (!kjson::Parse(m_JSON, sText, sError))
	{
		m_JSON = KJSON{};
		return SetError(kFormat("{}: {}", m_sPath, sError));
	}

	m_bLoaded = true;

	return true;

} // Load

//-----------------------------------------------------------------------------
bool KConfig::Save (KStringViewZ sPath/*=""*/, int iMode/*=DEKAF2_MODE_CREATE_FILE*/)
//-----------------------------------------------------------------------------
{
	if (!sPath.empty())
	{
		m_sPath = sPath;
	}

	ClearError();

	if (m_sPath.empty())
	{
		return SetError("no path to save the configuration to");
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
				return SetError(kFormat("cannot create the directory {}", sDir));
			}
		}
	}

	// An untouched KConfig holds a null JSON. For a hand-editable config
	// file, an empty object "{}" is friendlier than a literal "null".
	if (m_JSON.is_null())
	{
		m_JSON = KJSON::object();
	}

	// Pretty-printed for hand-editability. kWriteFile() applies a mode other
	// than the default when it creates the file, so a file with credentials is
	// never readable by others, not even for a moment.
	if (!kWriteFile(m_sPath, kFormat("{}\n", m_JSON.Serialize(/*bPretty=*/true)), iMode))
	{
		return SetError(kFormat("cannot write {}", m_sPath));
	}

	return true;

} // Save

DEKAF2_NAMESPACE_END
