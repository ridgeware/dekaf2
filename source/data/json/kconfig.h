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
// |/|   Software without restriction, including without limitation        |/|
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
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#pragma once

/// @file kconfig.h
/// persistent configuration store backed by a KJSON document

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <cstddef>

#ifndef DEKAF2_WRAPPED_KJSON
	#error "KConfig needs the wrapped KJSON type - build with DEKAF2_WRAPPED_KJSON"
#endif

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Persistent configuration store backed by a KJSON document.
///
/// The configuration is loaded from / saved to a JSON file. By default, the
/// file is located at @c ~/.config/{program_name}/config.json on Unix-like
/// systems (or its Windows equivalent), but any path may be specified.
///
/// Read/write access is provided by delegating @c operator[] to
/// KJSON::Select() and @c operator() to KJSON::ConstSelect(). Both
/// accept three key syntaxes:
/// - **plain key**: e.g. @c "format" - direct child of the current element
/// - **JSON Pointer (RFC 6901)**: e.g. @c "/display/format" - leading '/'
///   navigates a path
/// - **dotted notation**: e.g. @c ".display.format" - leading '.' navigates
///   a path
///
/// @c operator[] inserts/expands on miss (read/write semantics);
/// @c operator() never inserts and returns an empty element on miss
/// (read-only semantics).
///
/// Direct access to the underlying KJSON document is available via Get().
///
/// The file is read as text: UTF-8 with or without byte order mark, and
/// UTF-16 or UTF-32 with a byte order mark, which is converted to UTF-8.
/// The parser is strict, anything but white space after the JSON value is an
/// error. Save() always writes UTF-8 without byte order mark.
///
/// A missing file is not an error. A file that cannot be read or parsed, and a
/// failed Save(), set the error of the KErrorBase: Error() tells what went
/// wrong, HasError() whether something did.
///
/// ### Example
/// @code
/// KConfig cfg;                                   // loads default file if present
/// cfg["display"]["format"] = "ascii";            // read/write, plain key
/// cfg["/display/width"]    = 120;                // read/write, JSON Pointer
/// auto sFmt = cfg("/display/format").String();   // read-only, JSON Pointer
/// cfg.Save();                                    // persist to disk
/// @endcode
class DEKAF2_PUBLIC KConfig : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using StringT     = KJSON::StringT;
	using StringViewT = KJSON::StringViewT;
	using size_type   = KJSON::size_type;

	/// Default constructor. Sets Path() to the per-program default
	/// (@c ~/.config/{program_name}/config.json) and attempts to load it.
	/// A missing file is not an error; check Loaded() for the outcome.
	KConfig();

	/// Sets the configuration file path to @p sPath and attempts to load it.
	/// A missing file is not an error; check Loaded() for the outcome.
	explicit KConfig(KStringViewZ sPath);

	/// Returns a reference to the underlying JSON document.
	DEKAF2_NODISCARD
	KJSON&       Get()       noexcept { return m_JSON; }

	/// Returns a const reference to the underlying JSON document.
	DEKAF2_NODISCARD
	const KJSON& Get() const noexcept { return m_JSON; }

	/// Read/write access via KJSON::Select() - inserts missing keys / expands arrays.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	KJSON&       operator[] (StringViewT sKey)       noexcept { return m_JSON.Select(sKey);      }

	/// Read-only access via KJSON::ConstSelect() - returns an empty element on miss.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	const KJSON& operator[] (StringViewT sKey) const noexcept { return m_JSON.ConstSelect(sKey); }

	/// Forwards to KJSON::operator[] for array index access (inserts/expands as needed).
	DEKAF2_NODISCARD
	KJSON&       operator[] (size_type iIndex)       noexcept { return m_JSON[iIndex];           }

	/// Forwards to KJSON::operator[] (const) for array index access.
	DEKAF2_NODISCARD
	const KJSON& operator[] (size_type iIndex) const noexcept { return m_JSON[iIndex];           }

	/// Read-only access via KJSON::ConstSelect() - never inserts, returns empty on miss.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	const KJSON& operator() (StringViewT sKey) const noexcept { return m_JSON.ConstSelect(sKey); }

	/// Forwards to KJSON::operator() for array index - read-only access.
	DEKAF2_NODISCARD
	const KJSON& operator() (size_type iIndex) const noexcept { return m_JSON(iIndex);           }

	/// Loads the configuration from @p sPath. If @p sPath is empty, Path() is used.
	/// The path is remembered for subsequent Save() calls.
	/// @return true if the file was found and parsed successfully, false otherwise.
	/// A missing file returns false without setting an error (no config yet); a
	/// file that cannot be read or parsed sets the error and empties the document.
	bool Load (KStringViewZ sPath = KStringViewZ{});

	/// Saves the configuration to @p sPath. If @p sPath is empty, Path() is used.
	/// Creates the parent directory if needed. The file is written
	/// pretty-printed for hand-editability.
	/// @param iMode the permissions of the file, e.g. 0600 for a file that holds
	/// credentials. On POSIX systems the file is created with them, and an
	/// existing file is narrowed to them before anything is written.
	/// @return true on success, false on error (then the error is set).
	bool Save (KStringViewZ sPath = KStringViewZ{}, int iMode = DEKAF2_MODE_CREATE_FILE);

	/// Returns the path that will be used by Load("") / Save("").
	DEKAF2_NODISCARD
	const KString& Path () const noexcept { return m_sPath; }

	/// Sets the configuration file path without loading or saving.
	void           SetPath (KString sPath) { m_sPath = std::move(sPath); }

	/// Returns true if the last Load() call successfully read a file.
	DEKAF2_NODISCARD
	bool Loaded () const noexcept { return m_bLoaded; }

	/// Returns the default configuration file path
	/// (@c ~/.config/{program_name}/config.json on Unix-like systems).
	/// Does not create any directories.
	DEKAF2_NODISCARD
	static KString DefaultPath ();

//----------
private:
//----------

	KJSON   m_JSON;
	KString m_sPath;
	bool    m_bLoaded { false };

}; // KConfig

DEKAF2_NAMESPACE_END
