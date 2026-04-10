/*
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

/// @file kformtable.h
/// print a table from various types of data, into various output formats

#include <dekaf2/kstringview.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kjson.h>
#include <dekaf2/koutstringstream.h>
#include <dekaf2/kassociative.h>
#include <dekaf2/kctype.h>
#include <dekaf2/kcsv.h>
#include <dekaf2/krow.h>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_format
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Formats and prints tabular data into various output styles.
///
/// KFormTable takes data from different sources (strings, numbers, JSON, KROW) and
/// renders it as a formatted table into a stream, string, or JSON object. Supported
/// output styles include ASCII box drawing, Unicode box styles (bold, thin, double,
/// rounded), space-separated, vertical key-value pairs, HTML, JSON, CSV, and Markdown.
///
/// Column definitions can be set explicitly via the constructor or AddColDef(), or they
/// are created automatically from the data. For styles that require fixed column widths
/// (box styles, Spaced, Markdown), a two-pass approach with DryMode() can be used to
/// first measure and then print.
///
/// @par Basic usage with explicit columns:
/// @code
/// KString sOut;
/// KFormTable Table(sOut, { 10, { 10, KFormTable::Center }, { 5, KFormTable::Right } });
/// Table.SetStyle(KFormTable::Style::ASCII);
/// Table.PrintRow({ "Name", "City", "Age" });
/// Table.PrintSeparator();
/// Table.PrintRow({ "Alice", "Berlin", "30" });
/// Table.PrintRow({ "Bob", "Munich", "25" });
/// Table.Close();
/// @endcode
///
/// @par Two-pass usage with DryMode (auto-sized columns):
/// @code
/// KString sOut;
/// KFormTable Table(sOut);
/// Table.DryMode(true);
/// for (auto& row : data) Table.PrintRow(row);  // measure
/// Table.DryMode(false);
/// for (auto& row : data) Table.PrintRow(row);  // print
/// Table.Close();
/// @endcode
///
/// @par JSON input:
/// @code
/// KString sOut;
/// KFormTable Table(sOut);
/// Table.SetStyle(KFormTable::Style::Markdown);
/// Table.PrintJSON(jsonArray);  // auto-detects columns from keys
/// Table.Close();
/// @endcode
class DEKAF2_PUBLIC KFormTable
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = KString::size_type;

	/// Output style for the table. The box styles (ASCII through Rounded) draw bordered
	/// tables with different Unicode or ASCII line characters. The other styles produce
	/// structured output for different use cases.
	enum Style
	{
		ASCII    = 1 << 0,  ///< ASCII box drawing with +, -, | characters (default)
		Bold     = 1 << 1,  ///< Unicode box drawing with bold/heavy lines (┃━┏┓)
		Thin     = 1 << 2,  ///< Unicode box drawing with thin/light lines (│─┌┐)
		Double   = 1 << 3,  ///< Unicode box drawing with double lines (║═╔╗)
		Rounded  = 1 << 4,  ///< Unicode box drawing with thin lines and rounded corners (╭╮╰╯)
		Vertical = 1 << 5,  ///< key : value format, one column per line, rows separated by blank lines
		Spaced   = 1 << 6,  ///< columns separated by spaces, no borders
		HTML     = 1 << 7,  ///< HTML @<table@> markup with @<tr@>, @<td@>, @<th@> elements
		JSON     = 1 << 8,  ///< JSON array output (array of arrays or array of objects)
		CSV      = 1 << 9,  ///< RFC 4180 CSV output with proper quoting
		Markdown = 1 << 10  ///< Markdown table with | separators and alignment markers in header separator
	};

	/// Column alignment. Can be combined with Wrap using bitwise OR. Auto is the default
	/// and selects left alignment for strings and right alignment for numbers.
	enum Alignment
	{
		None   = 0,       ///< no explicit alignment set (treated as Auto)
		Auto   = 1 << 0,  ///< left aligned for strings, right aligned for numbers
		Left   = 1 << 1,  ///< left aligned
		Center = 1 << 2,  ///< center aligned
		Right  = 1 << 3,  ///< right aligned
		Wrap   = 1 << 4   ///< wrap into next line(s) if content exceeds column width, instead of truncating
	};

	/// Characters used to draw box-style table borders. Used with SetBoxStyle() to
	/// define custom box drawing characters for the ASCII style.
	///
	/// The nine corner/junction characters define the frame:
	/// @code
	/// TopLeft──────TopMiddle──────TopRight
	///    │            │              │
	/// MiddleLeft──MiddleMiddle──MiddleRight
	///    │            │              │
	/// BottomLeft──BottomMiddle──BottomRight
	/// @endcode
	struct BoxChars
	{
		KCodePoint TopLeft;       ///< top-left corner (e.g. '+' or '┌')
		KCodePoint TopMiddle;     ///< top junction between columns (e.g. '+' or '┬')
		KCodePoint TopRight;      ///< top-right corner (e.g. '+' or '┐')
		KCodePoint MiddleLeft;    ///< left junction at separator rows (e.g. '+' or '├')
		KCodePoint MiddleMiddle;  ///< center junction at separator rows (e.g. '+' or '┼')
		KCodePoint MiddleRight;   ///< right junction at separator rows (e.g. '+' or '┤')
		KCodePoint BottomLeft;    ///< bottom-left corner (e.g. '+' or '└')
		KCodePoint BottomMiddle;  ///< bottom junction between columns (e.g. '+' or '┴')
		KCodePoint BottomRight;   ///< bottom-right corner (e.g. '+' or '┘')
		KCodePoint Horizontal;    ///< horizontal line character (e.g. '-' or '─')
		KCodePoint Vertical;      ///< vertical line character (e.g. '|' or '│')
	};

	/// Defines one column: its name, display name, width, and alignment.
	/// Can be constructed implicitly from a single integer (width only), making
	/// initializer lists like @code { 10, { 20, KFormTable::Right }, 5 } @endcode possible.
	struct ColDef
	{
		ColDef() = default;
		/// construct with width and optional alignment
		ColDef(size_type iWidth, Alignment iAlign = Alignment::Auto) : m_iWidth(iWidth), m_iAlign(iAlign) {}
		/// construct with column name, optional width, and optional alignment
		ColDef(KString sColName, size_type iWidth = 0, Alignment iAlign = Alignment::Auto) : m_sColName(std::move(sColName)), m_iWidth(iWidth), m_iAlign(iAlign) {}
		/// construct with column name, display name, optional width, and optional alignment
		ColDef(KString sColName, KString sDispName, size_type iWidth = 0, Alignment iAlign = Alignment::Auto) : m_sColName(std::move(sColName)), m_sDispName(std::move(sDispName)), m_iWidth(iWidth), m_iAlign(iAlign) {}

		/// returns the display name if set, otherwise the column name
		const KString& GetDispName() const { return m_sDispName.empty() ? m_sColName : m_sDispName; }

		KString   m_sColName;          ///< internal column name (used as JSON key)
		KString   m_sDispName;         ///< display name shown in headers (if empty, m_sColName is used)
		size_type m_iWidth { 0 };      ///< column width in Unicode codepoints (0 = auto-sized)
		Alignment m_iAlign { Alignment::Auto }; ///< column alignment
	};

	using ColDefs = std::vector<ColDef>;

	KFormTable() = default;
	/// construct a KFormTable class that prints into a stream
	KFormTable(KOutStream&  Out, ColDefs ColDefs = {});
	/// construct a KFormTable class that prints into a string
	KFormTable(KStringRef& sOut, ColDefs ColDefs = {});
	/// construct a KFormTable class that prints into JSON
	KFormTable(KJSON& jOut);
	/// destructor, makes sure table is finalized properly
	~KFormTable();

	/// Switch dry mode on or off. In dry mode, PrintRow(), PrintColumn(), and PrintJSON()
	/// measure text widths without producing output. This updates column definitions with
	/// the maximum width seen for each column. Call DryMode(false) afterwards to switch to
	/// actual output. WantDryMode() returns true if the current style benefits from a dry pass.
	/// @param bYesNo true to enable measuring, false to enable output
	void DryMode(bool bYesNo) { m_bGetExtents = bYesNo; }

	/// returns the current output style
	DEKAF2_NODISCARD
	Style GetStyle() const { return m_Style; }
	/// sets the output style
	void SetStyle(Style Style);
	/// sets ASCII style with a user defined box style
	void SetBoxStyle(const BoxChars& BoxChars);

	/// set the maximum column width for any column
	void SetMaxColWidth(size_type iMaxWidth);
	/// set the maximum column count
	void SetMaxColCount(size_type iMaxColCount);

	/// open a new output stream - normally done by constructor
	void Open(KOutStream&  Out);
	/// open a new output string - normally done by constructor
	void Open(KStringRef& sOut);
	/// close the output - normally done by destructor
	void Close();

	/// add one more column definition, will be appended to the right of the existing column definitions
	void AddColDef(ColDef ColDef);

	/// add one more column definition, will be appended to the right of the existing column definitions
	/// given arguments must be valid constructor arguments for ColDef
	template<typename...Args>
	void AddColDef(Args&&...args)
	{
		AddColDef(ColDef(std::forward<Args>(args)...));
	}
	/// add a vector of column definitions, will be appended to the right of the existing column definitions
	void AddColDefs(const ColDefs& ColDefs);

	/// set a new name for a column header
	void SetColName(size_type iColumn, KString sColName);
	/// set a new name for a column header, replacing sOldColName by sNewColName
	void SetColNameAs(KStringView sOldColName, KStringView sNewColName);

	/// Per-cell rendering options that can override the column defaults.
	/// Allows setting a different alignment or spanning multiple columns for individual cells.
	struct ColumnRenderer
	{
		ColumnRenderer () {};
		/// construct with alignment override
		ColumnRenderer (Alignment Align) : m_Align(Align) {}
		/// construct with column span
		ColumnRenderer (size_type iSpan) : m_iSpan(iSpan) {}
		/// construct with alignment override and column span
		ColumnRenderer (Alignment Align, size_type iSpan) : m_Align(Align), m_iSpan(iSpan) {}

		Alignment m_Align { Alignment::None }; ///< alignment override (None = use column default)
		size_type m_iSpan { 0 };               ///< number of columns to span (0 or 1 = no spanning)
	};

	/// print any type convertible into a string view as single column into the output stream
	void PrintColumn(KStringView sText, ColumnRenderer Render = ColumnRenderer())
	{
		PrintColumnInt(false, sText, Render);
	}

	/// print a floating point value as single column into the output stream
	template <typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
	void PrintColumn(T iInt, uint16_t iPrecision = uint16_t(-1), ColumnRenderer Render = ColumnRenderer(), char chThousandsSeparator = ',')
	{
		PrintColumnInt(true, kFormNumber(iInt, chThousandsSeparator, 3, iPrecision), Render);
	}

	/// print any type that is an integer number as single column into the output stream
	template <typename T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_floating_point<T>::value, int>::type = 0>
	void PrintColumn(T iInt, ColumnRenderer Render = ColumnRenderer(), char chThousandsSeparator = ',')
	{
		PrintColumnInt(true, kFormNumber(iInt, chThousandsSeparator), Render);
	}

	/// print any duration type as single column into the output stream
	template <typename T, typename std::enable_if<detail::is_duration<T>::value, int>::type = 0>
	void PrintColumn(T duration, ColumnRenderer Render = ColumnRenderer(), char chThousandsSeparator = ',')
	{
		PrintColumnInt(true, kFormNumber(duration, chThousandsSeparator), Render);
	}

	/// print any iterable type with elements that are convertible into a string view or a number (also floating point) or a duration into the output stream
	template<class Columns = std::vector<KStringView>>
	void PrintColumns(const Columns& Row)
	{
		for (const auto& Column : Row)
		{
			PrintColumn(Column);
		}
	}

	/// when printing single columns, closes one row
	void PrintNextRow();

	/// print any iterable type with elements that are convertible into a string view or a number (also floating point) or a duration into the output stream,
	/// and terminate the current row
	template<class Columns = std::vector<KStringView>,
	         typename std::enable_if<std::is_same<Columns, KROW>::value == false, int>::type = 0>
	void PrintRow(const Columns& Row)
	{
		PrintColumns(Row);
		PrintNextRow();
	}

	/// print a KROW and terminate the current row
	void PrintRow(const KROW& Row);

	/// Print rows from a JSON value. Accepts:
	/// - an array of arrays (each inner array is one row)
	/// - an array of objects (keys become column headers, values become cells)
	/// - a single object (printed as one row)
	///
	/// For object input, columns are created automatically from the keys encountered.
	/// Column names can be renamed with SetColNameAs() before calling PrintJSON().
	/// If no column definitions exist yet and the style benefits from it, a dry-mode
	/// pass is performed automatically to measure column widths.
	/// @param json the JSON data to print
	/// @return true on success, false if the JSON structure is not supported
	bool PrintJSON(const KJSON& json);

	/// Print a horizontal separator line between rows. For box styles this draws a
	/// full-width line using the configured box characters. For Markdown this prints
	/// the alignment-aware header separator (e.g. |:---:|---:|). For HTML, JSON, CSV,
	/// and Vertical styles this is a no-op.
	void PrintSeparator();

	/// print a string OUTSIDE of all internal formatting logic - only use this for very special formattings, and keep in mind that
	/// it will not automatically adapt to all output styles
	void PrintRaw(KStringView sRawOutput) { Print(sRawOutput); }

	/// returns column count
	DEKAF2_NODISCARD
	size_type ColCount() const { return m_ColDefs.size(); }

	/// returns ref on ColDef for a certain column - expands row if smaller than iColumn
	DEKAF2_NODISCARD
	ColDef& GetColDef(size_type iColumn);

	/// returns ref on all ColDefs
	DEKAF2_NODISCARD
	ColDefs& GetColDefs() { return m_ColDefs; }

	/// returns count of output rows, not counting headers and separators
	DEKAF2_NODISCARD
	size_type GetPrintedRows() const { return m_iPrintedRows; }

	/// returns true if the selected table style requires a first run on all output data to determine
	/// column extents
	DEKAF2_NODISCARD
	bool WantDryMode() const;

	/// Convert a style name string into a Style enum value. Recognized names (case-insensitive):
	/// "ascii", "query", "table", "bold", "thin", "double", "rounded", "spaced",
	/// "vertical", "html", "json", "csv", "markdown", "md".
	/// An optional leading '-' is stripped. Returns ASCII on unrecognized input.
	DEKAF2_NODISCARD
	static Style StringToStyle(KStringView sStyle);

	/// Returns a comma-separated list of supported output style names
	DEKAF2_NODISCARD
	static KStringViewZ GetSupportedStyles();

	/// Returns true if the given string is a recognized style name
	DEKAF2_NODISCARD
	static bool IsKnownStyle(KStringView sStyle);

	/// Describes one supported style name, its enum value, a short description, and whether it is an alias
	struct StyleDef
	{
		KStringViewZ  sName;
		Style         eStyle;
		KStringViewZ  sDescription;
		bool          bIsAlias;
	};

	/// Returns a pointer pair (begin/end) over all known style definitions (including aliases)
	class StyleDefs
	{
		friend class KFormTable;

	public:

		const StyleDef* begin() const { return m_begin; }
		const StyleDef* end()   const { return m_end;   }

	private:

		StyleDefs(const StyleDef* begin, const StyleDef* end) : m_begin(begin), m_end(end) {}
		const StyleDef* m_begin;
		const StyleDef* m_end;

	}; // StyleDefs

	/// Returns an iterable range over all known style definitions
	DEKAF2_NODISCARD
	static StyleDefs GetStyles();

//----------
private:
//----------

	static constexpr Style Box = Style(Style::ASCII | Style::Bold | Style::Thin | Style::Double | Style::Rounded);

	void PrintTop();
	void PrintBottom();

	void PrintColumnInt(bool bIsNumber, KStringView sText, ColumnRenderer Render = ColumnRenderer());

	void Print(KCodePoint cp);
	void Print(KStringView str) { m_Out->Write(str); }

	size_type FitWidth(size_type iColumn, KStringView sText);
	void CheckHaveColHeaders();

	KUnorderedMap<KString, size_type> m_ColNames;
	KUnorderedMap<KString, KString>   m_ColNamesAs;

	std::unique_ptr<KOutStringStream> m_OutStringStream;
	KOutStream*                       m_Out { &kGetNullOutStream() };
	std::unique_ptr<KJSON>            m_JsonTemp;
	KJSON*                            m_JsonOut { nullptr };
	KJSON                             m_JsonRow;
	std::unique_ptr<KCSV>             m_CSV;

	ColDefs   m_ColDefs;

	size_type m_iColumn          { 0 };
	size_type m_iMaxColWidth     { size_type(-1) };
	size_type m_iMaxColCount     { size_type(-1) };
	size_type m_iMaxColNameWidth { 0 };
	size_type m_iPrintedRows     { 0 };

	BoxChars  m_BoxChars;
	Style     m_Style            { Style::ASCII };
	bool      m_bHadTopPrinted   { false };
	bool      m_bGetExtents      { false };
	bool      m_bHaveColHeaders  { false };
	bool      m_bInTableHeader   { false };
	bool      m_bInWrapContinuation { false };
	bool      m_bColDefsUserSet  { false };

	std::vector<KString> m_WrapOverflow;

}; // KFormTable

DEKAF2_ENUM_IS_FLAG(KFormTable::Alignment)


/// @}

DEKAF2_NAMESPACE_END

