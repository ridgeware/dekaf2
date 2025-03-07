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

#include "kstringview.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kjson.h"
#include "koutstringstream.h"
#include "kassociative.h"
#include "kctype.h"
#include "kcsv.h"
#include "krow.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// print a table from various types of data, into various output formats
class DEKAF2_PUBLIC KFormTable
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = KString::size_type;

	enum Style
	{
		ASCII    = 1 << 0, ///< Box style with ASCII lines (+-----+-----+) (default)
		Bold     = 1 << 1, ///< Box style with bold lines
		Thin     = 1 << 2, ///< Box style with thin lines
		Double   = 1 << 3, ///< Box style with two bold lines
		Rounded  = 1 << 4, ///< Box style with thin lines and rounded corners
		Spaced   = 1 << 6, ///< space as separator
		Vertical = 1 << 5, ///< vertical ASCII lines
		HTML     = 1 << 7, ///< html markup
		JSON     = 1 << 8, ///< json output
		CSV      = 1 << 9  ///< csv output
	};

	enum Alignment
	{
		None   = 0,
		Auto   = 1 << 0, ///< left aligned for strings, right aligned for numbers
		Left   = 1 << 1, ///< left aligned
		Center = 1 << 2, ///< center aligned
		Right  = 1 << 3, ///< right aligned
		Wrap   = 1 << 4  ///< wrap into next line if needed (not yet supported), else cut
	};

	struct BoxChars
	{
		KCodePoint TopLeft;
		KCodePoint TopMiddle;
		KCodePoint TopRight;
		KCodePoint MiddleLeft;
		KCodePoint MiddleMiddle;
		KCodePoint MiddleRight;
		KCodePoint BottomLeft;
		KCodePoint BottomMiddle;
		KCodePoint BottomRight;
		KCodePoint Horizontal;
		KCodePoint Vertical;
	};

	struct ColDef
	{
		ColDef() = default;
		ColDef(size_type iWidth, Alignment iAlign = Alignment::Auto) : m_iWidth(iWidth), m_iAlign(iAlign) {}
		ColDef(KString sColName, size_type iWidth = 0, Alignment iAlign = Alignment::Auto) : m_sColName(std::move(sColName)), m_iWidth(iWidth), m_iAlign(iAlign) {}
		ColDef(KString sColName, KString sDispName, size_type iWidth = 0, Alignment iAlign = Alignment::Auto) : m_sColName(std::move(sColName)), m_sDispName(std::move(sDispName)), m_iWidth(iWidth), m_iAlign(iAlign) {}

		const KString& GetDispName() const { return m_sDispName.empty() ? m_sColName : m_sDispName; }

		KString   m_sColName;
		KString   m_sDispName;
		size_type m_iWidth { 0 };
		Alignment m_iAlign { Alignment::Auto };
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

	/// switch dry mode on to "write" rows or columns for the purpose of measuring their
	/// max width - this will update the existing or yet unexisting column definitions
	void DryMode(bool bYesNo) { m_bGetExtents = bYesNo; }

	/// returns the current output style
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

	struct ColumnRenderer
	{
		ColumnRenderer () {};
		ColumnRenderer (Alignment Align) : m_Align(Align) {}
		ColumnRenderer (size_type iSpan) : m_iSpan(iSpan) {}
		ColumnRenderer (Alignment Align, size_type iSpan) : m_Align(Align), m_iSpan(iSpan) {}

		Alignment m_Align { Alignment::None };
		size_type m_iSpan { 0 };
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

	/// print a row from a json object or multiple rows from an array of objects or arrays - no mixed forms are allowed,
	/// and a maximum of two dimensions
	bool PrintJSON(const KJSON& json);

	/// print a horizontal separator
	void PrintSeparator();

	/// print a string OUTSIDE of all internal formatting logic - only use this for very special formattings, and keep in mind that
	/// it will not automatically adapt to all output styles
	void PrintRaw(KStringView sRawOutput) { Print(sRawOutput); }

	/// returns column count
	size_type ColCount() const { return m_ColDefs.size(); }

	/// returns ref on ColDef for a certain column - expands row if smaller than iColumn
	ColDef& GetColDef(size_type iColumn);

	/// returns ref on all ColDefs
	ColDefs& GetColDefs() { return m_ColDefs; }

	/// returns count of output rows, not counting headers and separators
	size_type GetPrintedRows() const { return m_iPrintedRows; }

	/// returns true if the selected table style requires a first run on all output data to determine
	/// column extents
	bool WantDryMode() const;

	/// return a style value for a string with a style name, like ASCII, thin, HTML ..
	/// This is a static method and does not change the style of a KFormStyle instance
	static Style StringToStyle(KStringView sStyle);


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
	bool      m_bColDefsUserSet  { false };

}; // KFormTable

DEKAF2_ENUM_IS_FLAG(KFormTable::Alignment)

DEKAF2_NAMESPACE_END

