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

#include "kformtable.h"
#include "kcsv.h"
#include "kcrashexit.h"
#include "khtmlentities.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KFormTable::KFormTable(KOutStream& Out, std::vector<ColDef> ColDefs)
//-----------------------------------------------------------------------------
: m_Out(&Out)
, m_ColDefs(std::move(ColDefs))
, m_bColDefsUserSet(!m_ColDefs.empty())
{
}

//-----------------------------------------------------------------------------
KFormTable::KFormTable(KStringRef& sOut, std::vector<ColDef> ColDefs)
//-----------------------------------------------------------------------------
: m_OutStringStream(std::make_unique<KOutStringStream>(sOut))
, m_Out(m_OutStringStream.get())
, m_ColDefs(std::move(ColDefs))
, m_bColDefsUserSet(!m_ColDefs.empty())
{
}

//-----------------------------------------------------------------------------
KFormTable::KFormTable(KJSON& jOut)
//-----------------------------------------------------------------------------
: m_JsonOut(&jOut)
, m_Style(Style::JSON)
{
}

//-----------------------------------------------------------------------------
KFormTable::~KFormTable()
//-----------------------------------------------------------------------------
{
	Close();
	
} // dtor

//-----------------------------------------------------------------------------
void KFormTable::Open(KOutStream&  Out)
//-----------------------------------------------------------------------------
{
	Close();

	m_Out = &Out;

} // Open

//-----------------------------------------------------------------------------
void KFormTable::Open(KStringRef& sOut)
//-----------------------------------------------------------------------------
{
	Close();

	m_OutStringStream = std::make_unique<KOutStringStream>(sOut);
	m_Out = m_OutStringStream.get();

} // Open

//-----------------------------------------------------------------------------
void KFormTable::Close()
//-----------------------------------------------------------------------------
{
	if (m_iColumn > 0)
	{
		PrintNextRow();
	}

	PrintBottom();

	m_Out = &kGetNullOutStream();
	m_OutStringStream.reset();

} // Close

//-----------------------------------------------------------------------------
void KFormTable::AddColDef(ColDef ColDef)
//-----------------------------------------------------------------------------
{
	m_ColDefs.push_back(std::move(ColDef));

	auto& Col   = m_ColDefs.back();
	auto& sName = Col.GetDispName();

	if (Col.m_iWidth == 0 && !sName.empty())
	{
		FitWidth(m_ColDefs.size() - 1, sName);
	}

	m_bColDefsUserSet = true;

} // AddColDef

//-----------------------------------------------------------------------------
void KFormTable::AddColDefs(const std::vector<ColDef>& ColDefs)
//-----------------------------------------------------------------------------
{
	for (auto& Col : ColDefs)
	{
		AddColDef(Col);
	}

	m_bHadTopPrinted = false;

} // AddColDefs

//-----------------------------------------------------------------------------
void KFormTable::Print(KCodePoint cp)
//-----------------------------------------------------------------------------
{
	if (cp.IsASCII()) m_Out->Write(char(cp));
	else m_Out->Write(Unicode::ToUTF<KString>(cp.value()));

} // Print

//-----------------------------------------------------------------------------
void KFormTable::PrintSeparator()
//-----------------------------------------------------------------------------
{
	if (!m_bGetExtents && ColCount() > 0)
	{
		switch (m_Style)
		{
			case Style::ASCII:
			case Style::Bold:
			case Style::Thin:
			case Style::Double:
			case Style::Rounded:
			{
				bool bFirst { true };
				
				for (const auto& Col : m_ColDefs)
				{
					if (bFirst)
					{
						Print(m_BoxChars.MiddleLeft);
						bFirst = false;
					}
					else Print(m_BoxChars.MiddleMiddle);
					
					for (size_type i = 0; i < Col.m_iWidth + 2; ++i) Print(m_BoxChars.Horizontal);
				}
				Print(m_BoxChars.MiddleRight);
			}
			DEKAF2_FALLTHROUGH;
			case Style::Spaced:
				Print('\n');
				break;
				
			case Style::HTML:
			case Style::JSON:
			case Style::CSV:
			case Style::Vertical:
				break;
		}
	}

} // PrintSeparator

//-----------------------------------------------------------------------------
void KFormTable::PrintTop()
//-----------------------------------------------------------------------------
{
	if (!m_bGetExtents)
	{
		m_bHadTopPrinted = true;

		switch (m_Style)
		{
			case Style::HTML:
				Print("<table>\n");
				break;

			case Style::ASCII:
			case Style::Bold:
			case Style::Thin:
			case Style::Double:
			case Style::Rounded:
			{
				if (m_BoxChars.TopLeft == 0)
				{
					// make sure we have a box style set
					SetStyle(Style::ASCII);
				}

				bool bFirst { true };

				for (const auto& Col : m_ColDefs)
				{
					if (bFirst)
					{
						Print(m_BoxChars.TopLeft);
						bFirst = false;
					}
					else Print(m_BoxChars.TopMiddle);

					for (size_type i = 0; i < Col.m_iWidth + 2; ++i) Print(m_BoxChars.Horizontal);
				}

				Print(m_BoxChars.TopRight);
				Print('\n');
			}
			break;

			case Style::Spaced:
			case Style::JSON:
			case Style::CSV:
			case Style::Vertical:
				break;
		}

		CheckHaveColHeaders();

		if (m_bHaveColHeaders && m_Style != Style::JSON)
		{
			m_bInTableHeader = true;

			PrintNextRow();

			for (auto& Col : m_ColDefs)
			{
				PrintColumnInt(false, Col.GetDispName());
			}

			PrintNextRow();
			PrintSeparator();

			m_bInTableHeader = false;
		}
	}

} // PrintTop

//-----------------------------------------------------------------------------
void KFormTable::PrintBottom()
//-----------------------------------------------------------------------------
{
	if (!m_bGetExtents && m_bHadTopPrinted)
	{
		switch (m_Style)
		{
			case Style::HTML:
				Print("</table>\n");
				break;

			case Style::ASCII:
			case Style::Bold:
			case Style::Thin:
			case Style::Double:
			case Style::Rounded:
			{
				bool bFirst { true };

				for (const auto& Col : m_ColDefs)
				{
					if (bFirst)
					{
						Print(m_BoxChars.BottomLeft);
						bFirst = false;
					}
					else Print(m_BoxChars.BottomMiddle);

					for (size_type i = 0; i < Col.m_iWidth + 2; ++i) Print(m_BoxChars.Horizontal);
				}

				Print(m_BoxChars.BottomRight);
				Print('\n');
			}
			break;

			case Style::JSON:
				// check if we have to dump the json into an output stream
				if (m_JsonOut && m_JsonTemp)
				{
					if (m_Out) m_Out->Write(m_JsonOut->dump());
					*m_JsonOut = KJSON::array();
				}
				break;

			case Style::Spaced:
			case Style::CSV:
			case Style::Vertical:
				break;
		}

		m_bHadTopPrinted = false;
	}

} // PrintBottom

//-----------------------------------------------------------------------------
KFormTable::size_type KFormTable::FitWidth(size_type iColumn, KStringView sText)
//-----------------------------------------------------------------------------
{
	if (iColumn >= m_iMaxColCount)
	{
		return 0;
	}

	while (iColumn >= ColCount())
	{
		m_ColDefs.push_back({});
	}

	auto iWidth = sText.SizeUTF8();

	if (iWidth > m_iMaxColWidth)
	{
		iWidth = m_iMaxColWidth;
	}

	if (m_ColDefs[iColumn].m_iWidth < iWidth)
	{
		m_ColDefs[iColumn].m_iWidth = iWidth;
	}

	return iWidth;

} // FitWidth

//-----------------------------------------------------------------------------
void KFormTable::PrintColumnInt(bool bIsNumber, KStringView sText, ColumnRenderer Renderer)
//-----------------------------------------------------------------------------
{
	if (m_iColumn >= m_iMaxColCount)
	{
		return;
	}

	if (m_bGetExtents)
	{
		FitWidth(m_iColumn, sText);
		++m_iColumn;
		return;
	}

	if (!m_bHadTopPrinted)
	{
		PrintTop();
	}

	size_type iWidth { 0 };
	size_type iSpan  { 1 };
	Alignment iAlign { Alignment::Auto };

	// only get column width and align information for Box, Spaced or HTML
	// (it is important to keep iWidth at 0 if we do not want to trim the output)
	if ((m_Style & (Box | Style::Spaced | Style::HTML)) != 0)
	{
		// get the defaults for the current column format
		if (m_iColumn < ColCount())
		{
			// don't trim HTML tables, except the user has set the
			// width explicitly
			if (m_Style != Style::HTML || m_bColDefsUserSet)
			{
				iWidth = m_ColDefs[m_iColumn].m_iWidth;
			}
			iAlign = m_ColDefs[m_iColumn].m_iAlign;
		}
	}

	// alignment may be modified
	if (Renderer.m_Align != Alignment::None)
	{
		iAlign = Renderer.m_Align;
	}

	// span may count more than one column
	if (Renderer.m_iSpan > 1)
	{
		iWidth = 0;
		iSpan  = Renderer.m_iSpan;

		// calc column width from the width of all spanned columns
		for (auto iCol = m_iColumn; iCol < m_iColumn + iSpan; ++iCol)
		{
			if (iCol >= ColCount())
			{
				break;
			}

			iWidth += m_ColDefs[iCol].m_iWidth;
		}
	}

	// calculate the content
	// have to use the casting to suppress UBSAN warning
	// remove the Alignment::Wrap bit
	iAlign     = static_cast<Alignment>((static_cast<std::underlying_type<Alignment>::type>(iAlign) & ~Alignment::Wrap));
	auto iSize = sText.SizeUTF8();
	size_type iFill {     0 };
	bool bOverflow  { false };

	if (iWidth)
	{
		if (iSize > iWidth)
		{
			// if bWrap ...
			// cut to maximum length
			sText     = sText.LeftUTF8(iWidth);
			bOverflow = true;
		}
		else if (iSize < iWidth)
		{
			iFill = iWidth - iSize;
		}
	}

	if (iAlign == Alignment::None)
	{
		iAlign = Alignment::Auto;
	}

	if (iAlign == Alignment::Auto)
	{
		iAlign = bIsNumber ? Alignment::Right : Alignment::Left;
	}

	// output the data

	switch (m_Style)
	{
		case Style::ASCII:
		case Style::Bold:
		case Style::Thin:
		case Style::Double:
		case Style::Rounded:
			if (!m_iColumn)
			{
				Print(m_BoxChars.Vertical);
			}
			Print(' ');
			break;

		case Style::Spaced:
			if (m_iColumn)
			{
				Print(' ');
			}
			break;

		case Style::HTML:
			if (!m_iColumn)
			{
				Print("\t<tr>\n");
			}
			Print(m_bInTableHeader ? "\t\t<th" : "\t\t<td");

			if (iSpan > 1)
			{
				Print(kFormat(" span={}", iSpan));
			}

			// don't print align=left - we take it as the default
			if ((iAlign & Alignment::Left) != Alignment::Left)
			{
				Print(" align=");
			}
			break;

		case Style::JSON:
		case Style::CSV:
		case Style::Vertical:
			break;
	}

	switch (m_Style)
	{
		case Style::JSON:
			kAssert(m_JsonOut, "no JSON pointer");
			if (m_bHaveColHeaders)
			{
				if (!m_iColumn)
				{
					m_JsonRow = KJSON();
				}

				if (m_iColumn < ColCount())
				{
					m_JsonRow[m_ColDefs[m_iColumn].GetDispName()] = sText;
				}
				else
				{
					m_JsonRow[kFormat("column{}", m_iColumn)] = sText;
				}
			}
			else
			{
				if (!m_iColumn)
				{
					m_JsonRow = KJSON::array();
				}

				// simply add array elements
				m_JsonRow.push_back(sText);
			}
			break;

		case Style::CSV:
			kAssert(m_CSV.get(), "no CSV pointer");
			m_CSV->WriteColumn(*m_Out, sText);
			break;

		case Style::ASCII:
		case Style::Bold:
		case Style::Thin:
		case Style::Double:
		case Style::Rounded:
		case Style::Spaced:
		case Style::HTML:
		{
			// with alignment
			if ((iAlign & Alignment::Left) == Alignment::Left)
			{
				if (m_Style == Style::HTML)
				{
					Print('>');
					Print(KHTMLEntity::EncodeMandatory(sText));
				}
				else
				{
					Print(sText);
					for (size_type i = 0; i < iFill; ++i) Print(' ');
				}
			}
			else if ((iAlign & Alignment::Center) == Alignment::Center)
			{
				auto iHalf = iFill / 2;

				if (m_Style == Style::HTML)
				{
					Print("center>");
					Print(KHTMLEntity::EncodeMandatory(sText));
				}
				else
				{
					for (size_type i = 0; i < iHalf; ++i)
					{
						Print(' ');
					}
					Print(sText);
				}

				if (m_Style != Style::HTML)
				{
					if (iFill && (iFill & 1) != 0)
					{
						// for odd fill sizes add one more space at the end
						++iHalf;
					}

					for (size_type i = 0; i < iHalf; ++i)
					{
						Print(' ');
					}
				}
			}
			else if ((iAlign & Alignment::Right) == Alignment::Right)
			{
				if (m_Style == Style::HTML)
				{
					Print("right>");
					Print(KHTMLEntity::EncodeMandatory(sText));
				}
				else
				{
					for (size_type i = 0; i < iFill; ++i)
					{
						Print(' ');
					}
					Print(sText);
				}
			}
		}
		break;

		case Style::Vertical:
			if (m_bInTableHeader)
			{
				if (iSize > m_iMaxColNameWidth)
				{
					m_iMaxColNameWidth = iSize;
				}
			}
			else
			{
				iFill = m_iMaxColNameWidth + 1;
				// print colname and value
				if (m_iColumn < ColCount())
				{
					auto& sName = m_ColDefs[m_iColumn].GetDispName();
					iFill -= sName.SizeUTF8();
					Print(sName);
				}

				while (iFill--)
				{
					Print(' ');
				}
				
				Print(": ");
				Print(sText);
			}
			break;
	}

	switch (m_Style)
	{
		case Style::ASCII:
		case Style::Bold:
		case Style::Thin:
		case Style::Double:
		case Style::Rounded:
			Print(bOverflow ? '>' : ' ');
			Print(m_BoxChars.Vertical);
			break;

		case Style::Spaced:
			Print(bOverflow ? '>' : ' ');
			break;

		case Style::HTML:
			Print(m_bInTableHeader ? "</th>\n" : "</td>\n");
			break;

		case Style::JSON:
		case Style::CSV:
			break;

		case Style::Vertical:
			if (!m_bInTableHeader) Print('\n');
			break;
	}

	if (!m_iColumn && !m_bInTableHeader)
	{
		++m_iPrintedRows;
	}

	m_iColumn += iSpan;

} // PrintColumnInt

//-----------------------------------------------------------------------------
void KFormTable::PrintRow(const KROW& Row)
//-----------------------------------------------------------------------------
{
	if (Row.empty()) return;

	// special mode: single cols in Vertical style get printed
	// without name and separating lines
	if (Row.size() == 1 && m_Style == Style::Vertical)
	{
		if (!m_bGetExtents)
		{
			Print(Row.begin()->second);
			Print('\n');
			++m_iPrintedRows;
		}
		return;
	}

	if (!m_bGetExtents && m_ColDefs.empty())
	{
		m_bGetExtents = true;
		PrintRow(Row);
		m_bGetExtents = false;
	}

	if (m_bGetExtents && m_ColDefs.empty())
	{
		// print col headers once
		for (auto& Col : Row)
		{
			bool bIsNum = (Col.second.GetFlags() & (KCOL::Flags::NUMERIC | KCOL::Flags::MONEY | KCOL::Flags::INT64NUMERIC)) != KCOL::Flags::NOFLAG;
			PrintColumnInt(bIsNum, Col.first);
			m_ColDefs[m_iColumn - 1].m_sColName = Col.first; // only if < maxcolumns
		}
		
		PrintNextRow();
	}

	// do not print non-header rows for length checking if it's not Box/Spaced/HTML layout
	if (!m_bGetExtents || (m_Style & (Box | Style::Spaced | Style::HTML )) != 0)
	{
		for (auto& Col : Row)
		{
			bool bIsNum = (Col.second.GetFlags() & (KCOL::Flags::NUMERIC | KCOL::Flags::MONEY | KCOL::Flags::INT64NUMERIC)) != KCOL::Flags::NOFLAG;
			PrintColumnInt(bIsNum, Col.second.sValue);
		}

		PrintNextRow();
	}

} // PrintRow(const KROW&)

//-----------------------------------------------------------------------------
bool KFormTable::PrintJSON(const KJSON& json)
//-----------------------------------------------------------------------------
{
	if (!m_bGetExtents && m_ColDefs.empty())
	{
		m_bGetExtents = true;
		PrintJSON(json);
		m_bGetExtents = false;
	}

	if (json.is_array())
	{
		for (auto& it : json)
		{
			if (it.is_object())
			{
				PrintJSON(it);
			}
			else if (it.is_array())
			{
				for (auto& Col : it)
				{
#if DEKAF2_WRAPPED_KJSON
					PrintColumnInt(Col.is_number(), Col.is_string() ? Col.String() : Col.CopyString());
#else
					PrintColumnInt(Col.is_number(), Col.is_string() ? Col.get_ref<const KString&>() : kjson::Print(Col));
#endif
				}
				PrintNextRow();
			}
			else
			{
				kDebug(1, "only objects or arrays of objects/arrays can be printed");
				PrintNextRow();
				return false;
			}
		}
		return true;
	}
	else if (!json.is_object())
	{
		kDebug(1, "only objects or arrays of objects/arrays can be printed");
		PrintNextRow();
		return false;
	}

	// is an object - this means we have to do a bit more sorting of names and values

	if (m_bGetExtents)
	{
		for (auto& it : json.items())
		{
			bool bIsNumber = it.value().is_number();

			auto nit = m_ColNames.find(it.key());

			if (nit == m_ColNames.end())
			{
				if (m_ColDefs.size() >= m_iMaxColCount)
				{
					// drop this column (and all following), but continue
					// iterating the object to measure extents for accepted
					// columns
					continue;
				}
				// check if we shall replace the column name
				auto as = m_ColNamesAs.find(it.key());
				KStringView sDispName = (as == m_ColNamesAs.end()) ? KStringView{} : KStringView(as->second);
				// append a new column
				m_ColDefs.push_back( { it.key(), sDispName, 0, bIsNumber ? Alignment::Right : Alignment::Left } );
				// and point to it
				auto p = m_ColNames.insert( { it.key(), m_ColDefs.size()-1 } );
				// point nit at
				nit = p.first;
			}

			if (it.value().is_string())
			{
#if DEKAF2_WRAPPED_KJSON
				FitWidth(nit->second, it.value().String());
#else
				FitWidth(nit->second, it.value().get_ref<const KString&>());
#endif
			}
			else
			{
#if DEKAF2_WRAPPED_KJSON
				FitWidth(nit->second, it.value().CopyString());
#else
				FitWidth(nit->second, kjson::Print(it.value()));
#endif
			}
		}
		PrintNextRow();
		// now print all column names to make sure they fit
		for (auto& Col : m_ColDefs)
		{
			PrintColumnInt(false, Col.m_sDispName.empty() ? Col.m_sColName : Col.m_sDispName);
		}
	}
	else
	{
		// iterate over the column names and try to find values in the current
		// object
		for (auto& Col : m_ColDefs)
		{
			auto it = json.find(Col.m_sColName);

			if (it != json.end())
			{
				bool bIsNumber = it.value().is_number();
				bool bIsString = it.value().is_string();
#if DEKAF2_WRAPPED_KJSON
				PrintColumnInt(bIsNumber, bIsString ? it.value().String() : it.value().CopyString());
#else
				PrintColumnInt(bIsNumber, bIsString ? it.value().get_ref<const KString&>() : kjson::Print(it.value()));
#endif
			}
			else
			{
				if (m_Style != Style::JSON)
				{
					// only print empty column for missing key if output style is not JSON
					// (would result in keys with empty values otherwise)
					PrintColumnInt(false, "");
				}
			}
		}
	}

	PrintNextRow();
	return true;

} // PrintJSON

//-----------------------------------------------------------------------------
void KFormTable::PrintNextRow()
//-----------------------------------------------------------------------------
{
	if (!m_bGetExtents && m_iColumn > 0)
	{
		if ((m_Style & (Style::JSON | Style::Vertical)) == 0)
		{
			for (size_type iCol = m_iColumn; iCol < ColCount(); ++iCol)
			{
				// print empty trailing cols
				PrintColumn({});
			}
		}

		switch (m_Style)
		{
			case Style::HTML:
				Print("\t</tr>\n");
				break;

			case Style::ASCII:
			case Style::Bold:
			case Style::Thin:
			case Style::Double:
			case Style::Rounded:
			case Style::Spaced:
				Print('\n');
				break;

			case Style::Vertical:
				if (!m_bInTableHeader) Print('\n');
				break;

			case Style::JSON:
				if (!m_JsonRow.empty())
				{
					if (m_JsonOut)
					{
						if (m_JsonOut->empty()) *m_JsonOut = KJSON::array();
						m_JsonOut->push_back(std::move(m_JsonRow));
					}
				}
				break;

			case Style::CSV:
				if (m_CSV) m_CSV->WriteEndOfRecord(*m_Out);
				break;
		}
	}

	m_iColumn = 0;

} // PrintNextRow

//-----------------------------------------------------------------------------
KFormTable::ColDef& KFormTable::GetColDef(size_type iColumn)
//-----------------------------------------------------------------------------
{
	while (iColumn >= ColCount())
	{
		m_ColDefs.push_back({});
	}

	return m_ColDefs[iColumn];

} // GetColDef

//-----------------------------------------------------------------------------
void KFormTable::SetMaxColWidth(size_type iMaxWidth)
//-----------------------------------------------------------------------------
{
	m_iMaxColWidth = iMaxWidth;

	// check existing columns
	for (auto& Col : m_ColDefs)
	{
		if (Col.m_iWidth > m_iMaxColWidth)
		{
			Col.m_iWidth = m_iMaxColWidth;
		}
	}

} // SetMaxColWidth

//-----------------------------------------------------------------------------
void KFormTable::SetMaxColCount(size_type iMaxColCount)
//-----------------------------------------------------------------------------
{
	m_iMaxColCount = iMaxColCount;

	if (m_ColDefs.size() > m_iMaxColCount)
	{
		m_ColDefs.erase(m_ColDefs.begin() + m_iMaxColCount, m_ColDefs.end());
	}

	auto it = m_ColNames.begin();
	auto ie = m_ColNames.end();

	for (;it != ie;)
	{
		if (it->second >= m_iMaxColCount)
		{
			it = m_ColNames.erase(it);
		}
		else
		{
			++it;
		}
	}

} // SetMaxColCount

//-----------------------------------------------------------------------------
void KFormTable::SetColName(size_type iColumn, KString sColName)
//-----------------------------------------------------------------------------
{
	FitWidth(iColumn, sColName);

	m_ColDefs[iColumn].m_sDispName = std::move(sColName);

} // SetColName

//-----------------------------------------------------------------------------
void KFormTable::SetColNameAs(KStringView sOldColName, KStringView sNewColName)
//-----------------------------------------------------------------------------
{
	auto p = m_ColNamesAs.insert( { sOldColName, sNewColName } );

	if (!p.second)
	{
		p.first->second = sNewColName;
	}

	for (auto& Col : m_ColDefs)
	{
		if (Col.m_sColName == sOldColName)
		{
			Col.m_sDispName = sNewColName;
			break;
		}
	}

} // SetColNameAs

//-----------------------------------------------------------------------------
void KFormTable::SetStyle(Style Style)
//-----------------------------------------------------------------------------
{
	m_Style = Style;

	switch (m_Style)
	{
		case Style::ASCII:
			m_BoxChars.TopLeft      = '+';
			m_BoxChars.TopMiddle    = '+';
			m_BoxChars.TopRight     = '+';
			m_BoxChars.MiddleLeft   = '+';
			m_BoxChars.MiddleMiddle = '+';
			m_BoxChars.MiddleRight  = '+';
			m_BoxChars.BottomLeft   = '+';
			m_BoxChars.BottomMiddle = '+';
			m_BoxChars.BottomRight  = '+';
			m_BoxChars.Horizontal   = '-';
			m_BoxChars.Vertical     = '|';
			break;

		case Style::Bold:
			m_BoxChars.TopLeft      = 0x250F;
			m_BoxChars.TopMiddle    = 0x2533;
			m_BoxChars.TopRight     = 0x2513;
			m_BoxChars.MiddleLeft   = 0x2523;
			m_BoxChars.MiddleMiddle = 0x254B;
			m_BoxChars.MiddleRight  = 0x252B;
			m_BoxChars.BottomLeft   = 0x2517;
			m_BoxChars.BottomMiddle = 0x253B;
			m_BoxChars.BottomRight  = 0x251B;
			m_BoxChars.Horizontal   = 0x2501;
			m_BoxChars.Vertical     = 0x2503;
			break;

		case Style::Thin:
			m_BoxChars.TopLeft      = 0x250C;
			m_BoxChars.TopMiddle    = 0x252C;
			m_BoxChars.TopRight     = 0x2510;
			m_BoxChars.MiddleLeft   = 0x251C;
			m_BoxChars.MiddleMiddle = 0x253C;
			m_BoxChars.MiddleRight  = 0x2524;
			m_BoxChars.BottomLeft   = 0x2514;
			m_BoxChars.BottomMiddle = 0x2534;
			m_BoxChars.BottomRight  = 0x2518;
			m_BoxChars.Horizontal   = 0x2500;
			m_BoxChars.Vertical     = 0x2502;
			break;

		case Style::Double:
			m_BoxChars.TopLeft      = 0x2554;
			m_BoxChars.TopMiddle    = 0x2566;
			m_BoxChars.TopRight     = 0x2557;
			m_BoxChars.MiddleLeft   = 0x2560;
			m_BoxChars.MiddleMiddle = 0x256C;
			m_BoxChars.MiddleRight  = 0x2563;
			m_BoxChars.BottomLeft   = 0x255A;
			m_BoxChars.BottomMiddle = 0x2569;
			m_BoxChars.BottomRight  = 0x255D;
			m_BoxChars.Horizontal   = 0x2550;
			m_BoxChars.Vertical     = 0x2551;
			break;

		case Style::Rounded:
			m_BoxChars.TopLeft      = 0x256D;
			m_BoxChars.TopMiddle    = 0x252C;
			m_BoxChars.TopRight     = 0x256E;
			m_BoxChars.MiddleLeft   = 0x251C;
			m_BoxChars.MiddleMiddle = 0x253C;
			m_BoxChars.MiddleRight  = 0x2524;
			m_BoxChars.BottomLeft   = 0x2570;
			m_BoxChars.BottomMiddle = 0x2534;
			m_BoxChars.BottomRight  = 0x256F;
			m_BoxChars.Horizontal   = 0x2500;
			m_BoxChars.Vertical     = 0x2502;
			break;

		case Style::CSV:
			if (!m_CSV)
			{
				m_CSV = std::make_unique<KCSV>();
			}
			break;

		case Style::JSON:
			if (!m_JsonOut)
			{
				if (!m_JsonTemp)
				{
					m_JsonTemp = std::make_unique<KJSON>();
				}
				m_JsonOut = m_JsonTemp.get();
			}
			break;

		case Style::HTML:
		case Style::Spaced:
		case Style::Vertical:
			break;
	}

} // SetStyle

//-----------------------------------------------------------------------------
void KFormTable::SetBoxStyle(const BoxChars& BoxChars)
//-----------------------------------------------------------------------------
{
	m_BoxChars = BoxChars;

	m_Style = Style::ASCII;

} // SetBoxStyle

//-----------------------------------------------------------------------------
void KFormTable::CheckHaveColHeaders()
//-----------------------------------------------------------------------------
{
	if (!m_bHaveColHeaders)
	{
		for (const auto& Col : m_ColDefs)
		{
			if (!Col.GetDispName().empty())
			{
				m_bHaveColHeaders = true;
				break;
			}
		}
	}

} // CheckHaveColHeaders

//-----------------------------------------------------------------------------
bool KFormTable::WantDryMode() const
//-----------------------------------------------------------------------------
{
	return ((m_Style & (Box | Style::Spaced)) != 0) && m_ColDefs.empty();
}

//-----------------------------------------------------------------------------
KFormTable::Style KFormTable::StringToStyle(KStringView sStyle)
//-----------------------------------------------------------------------------
{
	Style Style = Style::ASCII;

	sStyle.remove_prefix('-');

	switch (sStyle.CaseHash())
	{
		case "ascii"_casehash:
		case "query"_casehash:
		case "table"_casehash:
			Style = Style::ASCII;
			break;
		case "vertical"_casehash:
			Style = Style::Vertical;
			break;
		case "json"_casehash:
			Style = Style::JSON;
			break;
		case "csv"_casehash:
			Style = Style::CSV;
			break;
		case "html"_casehash:
			Style = Style::HTML;
			break;
		case "bold"_casehash:
			Style = Style::Bold;
			break;
		case "rounded"_casehash:
			Style = Style::Rounded;
			break;
		case "thin"_casehash:
			Style = Style::Thin;
			break;
		case "double"_casehash:
			Style = Style::Double;
			break;
		case "spaced"_casehash:
			Style = Style::Spaced;
			break;
		default:
			kDebug(1, "invalid style: {}", sStyle);
			break;
	}

	return Style;

} // StringToStyle

DEKAF2_NAMESPACE_END
