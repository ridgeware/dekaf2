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
/// persistent configuration store backed by a KJSON2 document

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson2.h>
#include <cstddef>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Persistent configuration store backed by a KJSON2 document.
///
/// The configuration is loaded from / saved to a JSON file. By default, the
/// file is located at @c ~/.config/{program_name}/config.json on Unix-like
/// systems (or its Windows equivalent), but any path may be specified.
///
/// Read/write access is provided by delegating @c operator[] to
/// KJSON2::Select() and @c operator() to KJSON2::ConstSelect(). Both
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
/// Direct access to the underlying KJSON2 document is available via Get().
///
/// ### Example
/// @code
/// KConfig cfg;                                   // loads default file if present
/// cfg["display"]["format"] = "ascii";            // read/write, plain key
/// cfg["/display/width"]    = 120;                // read/write, JSON Pointer
/// auto sFmt = cfg("/display/format").String();   // read-only, JSON Pointer
/// cfg.Save();                                    // persist to disk
/// @endcode
class DEKAF2_PUBLIC KConfig
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using StringT     = KJSON2::StringT;
	using StringViewT = KJSON2::StringViewT;
	using size_type   = KJSON2::size_type;

	/// Default constructor. Sets Path() to the per-program default
	/// (@c ~/.config/{program_name}/config.json) and attempts to load it.
	/// A missing file is not an error; check Loaded() for the outcome.
	KConfig();

	/// Sets the configuration file path to @p sPath and attempts to load it.
	/// A missing file is not an error; check Loaded() for the outcome.
	explicit KConfig(KStringViewZ sPath);

	/// Returns a reference to the underlying JSON document.
	DEKAF2_NODISCARD
	KJSON2&       Get()       noexcept { return m_JSON; }

	/// Returns a const reference to the underlying JSON document.
	DEKAF2_NODISCARD
	const KJSON2& Get() const noexcept { return m_JSON; }

	/// Read/write access via KJSON2::Select() - inserts missing keys / expands arrays.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	KJSON2&       operator[] (StringViewT sKey)       noexcept { return m_JSON.Select(sKey);      }

	/// Read-only access via KJSON2::ConstSelect() - returns an empty element on miss.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	const KJSON2& operator[] (StringViewT sKey) const noexcept { return m_JSON.ConstSelect(sKey); }

	/// Forwards to KJSON2::operator[] for array index access (inserts/expands as needed).
	DEKAF2_NODISCARD
	KJSON2&       operator[] (size_type iIndex)       noexcept { return m_JSON[iIndex];           }

	/// Forwards to KJSON2::operator[] (const) for array index access.
	DEKAF2_NODISCARD
	const KJSON2& operator[] (size_type iIndex) const noexcept { return m_JSON[iIndex];           }

	/// Read-only access via KJSON2::ConstSelect() - never inserts, returns empty on miss.
	/// Accepts plain keys, JSON Pointers ("/a/b"), and dotted notation (".a.b").
	DEKAF2_NODISCARD
	const KJSON2& operator() (StringViewT sKey) const noexcept { return m_JSON.ConstSelect(sKey); }

	/// Forwards to KJSON2::operator() for array index - read-only access.
	DEKAF2_NODISCARD
	const KJSON2& operator() (size_type iIndex) const noexcept { return m_JSON(iIndex);           }

	/// Loads the configuration from @p sPath. If @p sPath is empty, Path() is used.
	/// On success the path is remembered for subsequent Save() calls.
	/// @return true if the file was found and parsed successfully, false otherwise.
	/// A missing file is not an error condition for callers (no config yet) but
	/// returns false; check Loaded() to differentiate states if needed.
	bool Load (KStringViewZ sPath = KStringViewZ{});

	/// Saves the configuration to @p sPath. If @p sPath is empty, Path() is used.
	/// Creates the parent directory if needed. The file is written
	/// pretty-printed for hand-editability.
	/// @return true on success, false on error.
	bool Save (KStringViewZ sPath = KStringViewZ{});

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

	KJSON2  m_JSON;
	KString m_sPath;
	bool    m_bLoaded { false };

}; // KConfig

DEKAF2_NAMESPACE_END
