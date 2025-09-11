/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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
//
*/

#include "krow.h"
#include "klog.h"
#include "kcsv.h"
#include "ksystem.h"
#include "kexception.h"

DEKAF2_NAMESPACE_BEGIN

// MySQL requires NUL, the quotes and backslash to be escaped. Do not escape
// by doubling the value, as the result then depends on the context (if inside
// single quotes, double double quotes will be seen as two, and vice verse)
constexpr KStringView ESCAPE_MYSQL { "'\"\\`\0"_ksv };
// TODO check the rules for MSSQL, particularly for \0 and \Z
constexpr KStringView ESCAPE_MSSQL { "\'"_ksv   };

int16_t detail::KCommonSQLBase::m_iDebugLevel { 2 };


//-----------------------------------------------------------------------------
void KSQLString::ThrowWarning(KStringView sContent)
//-----------------------------------------------------------------------------
{
	KException ex( kFormat("KSQLString: dynamic string buffers are not accepted: {}", sContent.LeftUTF8(50)) );
	kException(ex);
	throw ex;
}

//-----------------------------------------------------------------------------
void KSQLString::AssignFromView(KStringView sContent)
//-----------------------------------------------------------------------------
{
	if (!kIsInsideDataSegment(sContent.data()))
	{
		ThrowWarning(sContent);
	}

	m_sContent = sContent;

} // AssignFromView

//-----------------------------------------------------------------------------
KSQLString& KSQLString::operator+=(const string_type::value_type* sOther)
//-----------------------------------------------------------------------------
{
	if (!kIsInsideDataSegment(sOther))
	{
		ThrowWarning(sOther);
	}

	m_sContent += sOther;

	return *this;

} // ctor

//-----------------------------------------------------------------------------
std::vector<KSQLString> KSQLString::Split(const char chDelimit, bool bTrimWhiteSpace) const
//-----------------------------------------------------------------------------
{
	if (ESCAPE_MYSQL.contains(chDelimit))
	{
		KException ex( kFormat("KSQLString: invalid split character {}", chDelimit) );
		kException(ex);
		throw ex;
	}

	std::vector<KSQLString> Strings;

	char chQuote             { 0     };
	bool bEscaped            { false };
	KString::size_type iPos  { 0     };
	KString::size_type iLast { 0     };
	KString::size_type iSize { m_sContent.size() };

	auto SplitAndPush = [&]()
	{
		KSQLString sStr;
		sStr.m_sContent = m_sContent.substr(iLast, iPos - iLast);

		if (bTrimWhiteSpace)
		{
			sStr.m_sContent.Trim();
		}

		Strings.push_back(std::move(sStr));
		iLast = iPos + 1;
	};

	for (; iPos < iSize; ++iPos)
	{
		if (bEscaped)
		{
			bEscaped = false;
			continue;
		}

		auto ch = m_sContent[iPos];

		if (ch == '\\')
		{
			bEscaped = true;
			continue;
		}

		if (chQuote != 0)
		{
			if (ch == chQuote)
			{
				chQuote = 0;
			}
		}
		else if (ch == '\'' || ch == '"')
		{
			chQuote = ch;
		}
		else if (ch == chDelimit)
		{
			// split here
			SplitAndPush();
		}
	}

	if (iLast < iPos)
	{
		SplitAndPush();
	}

	return Strings;

} // Split


//-----------------------------------------------------------------------------
bool operator==(const KCOL& left, const KCOL& right)
//-----------------------------------------------------------------------------
{
	return left.GetFlags() == right.GetFlags()
		&& left.GetMaxLen() == right.GetMaxLen()
		&& left.sValue == right.sValue;
}

//-----------------------------------------------------------------------------
void KCOL::clear()
//-----------------------------------------------------------------------------
{
	sValue.clear();
	m_iMaxLen = 0;
	m_Flags   = KCOL::Flags::NOFLAG;
}

//-----------------------------------------------------------------------------
KString KCOL::FlagsToString (Flags iFlags)
//-----------------------------------------------------------------------------
{
	KString sPretty;

	if (iFlags == NOFLAG)
	{
		sPretty = "[STRING]";
	}
	else
	{
		if (iFlags & PKEY)            {  sPretty += "[PKEY]";            }
		if (iFlags & NONCOLUMN)       {  sPretty += "[NONCOLUMN]";       }
		if (iFlags & EXPRESSION)      {  sPretty += "[EXPRESSION]";      }
		if (iFlags & INSERTONLY)      {  sPretty += "[INSERTONLY]";      }
		if (iFlags & NUMERIC)         {  sPretty += "[NUMERIC]";         }
		if (iFlags & MONEY)           {  sPretty += "[MONEY]";           }
		if (iFlags & NULL_IS_NOT_NIL) {  sPretty += "[NULL_IS_NOT_NIL]"; }
		if (iFlags & BOOLEAN)         {  sPretty += "[BOOLEAN]";         }
		if (iFlags & JSON)            {  sPretty += "[JSON]";            }
		if (iFlags & INT64NUMERIC)    {  sPretty += "[INT64NUMERIC]";    }
		if (iFlags & INCREMENT)       {  sPretty += "[INCREMENT]";       }
	}

	return (sPretty);

} // FlagsToString

//-----------------------------------------------------------------------------
void KROW::LogRowLayout(int iLogLevel) const
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(iLogLevel))
	{
		std::size_t iCol { 0 };

		for (const auto& it : *this)
		{
			kDebugLog (iLogLevel,
					   kFormat("  col[{:>02}]: {:<25} {}",
							   ++iCol,
							   it.first,
							   it.second.FlagsToString()
							)
					   );
		}
	}
#endif

} // LogRowLayout

//-----------------------------------------------------------------------------
bool KROW::NeedsEscape (KStringView sCol, KStringView sCharsToEscape)
//-----------------------------------------------------------------------------
{
	return sCol.find_first_of(sCharsToEscape) != KStringView::npos;

} // NeedsEscape

//-----------------------------------------------------------------------------
KSQLString KROW::EscapeChars (KStringView sCol, KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as its own escape char (i.e. it gets doubled up).
	KSQLString sEscaped;
	auto& sRef = sEscaped.ref();
	sRef.reserve(sCol.size());

	for (KStringView::size_type iStart; (iStart = sCol.find_first_of(sCharsToEscape)) != KStringView::npos; )
	{
		sRef += sCol.substr(0, iStart);
		auto ch = sCol[iStart];
		if (!ch) ch = '0'; // NUL needs special treatment
		sRef += (iEscapeChar) ? iEscapeChar : ch;
		sRef += ch;
		// prepare for next round
		sCol.remove_prefix(++iStart);
	}
	// add the remainder of the input string
	sRef += sCol;

	return sEscaped;

} // EscapeChars

//-----------------------------------------------------------------------------
KStringView KROW::EscapedCharacters (DBT iDBType)
//-----------------------------------------------------------------------------
{
	switch (iDBType)
	{
		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
		case DBT::SYBASE:
			return ESCAPE_MSSQL;
		case DBT::MYSQL:
		default:
			return ESCAPE_MYSQL;
	}

} // EscapedCharacters

//-----------------------------------------------------------------------------
KSQLString KROW::EscapeChars (KStringView sCol, DBT iDBType)
//-----------------------------------------------------------------------------
{
	switch (iDBType)
	{
		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
		case DBT::SYBASE:
			return EscapeChars (sCol, ESCAPE_MSSQL);
		case DBT::MYSQL:
		default:
			return EscapeChars (sCol, ESCAPE_MYSQL, '\\');
	}

} // EscapeChars

//-----------------------------------------------------------------------------
KSQLString KROW::EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as its own escape char (i.e. it gets doubled up).

	KSQLString sEscaped;

	// check if we shall clip the string
	auto iMaxLen = Col.second.GetMaxLen();

	if (iMaxLen)
	{
		sEscaped = EscapeChars(Col.second.sValue.Left(iMaxLen), sCharsToEscape, iEscapeChar);

		if (sEscaped.size() > iMaxLen)
		{
			kDebug (1, "clipping {}='{:.10}...' with length {} to {} chars", Col.first, sEscaped, sEscaped.size(), iMaxLen);

			auto iClipAt { iMaxLen };

			// watch out for a trailing escape or escape char, depending on the algorithm:
			if (iEscapeChar)
			{
				// now walk back to the beginning of a sequence of backslashes
				while (iClipAt > 0 && sEscaped.str()[iClipAt-1] == iEscapeChar)
				{
					--iClipAt;
				}
				// now walk forth in pairs of backslashes as long as there is room
				while (iClipAt+2 <= iMaxLen)
				{
					iClipAt += 2;
				}
			}
			else
			{
				// as long as we are not completely sure about the MS escapes we just drop all of
				// them trailing a cutoff string
				while (iClipAt > 0 && sCharsToEscape.find(sEscaped.str()[iClipAt-1]) != KStringView::npos)
				{
					--iClipAt;
				}
			}

			sEscaped.resize(iClipAt);
		}
	}
	else
	{
		sEscaped = EscapeChars(Col.second.sValue, sCharsToEscape, iEscapeChar);
	}

	return sEscaped;

} // EscapeChars

//-----------------------------------------------------------------------------
KSQLString KROW::EscapeChars (const KROW::value_type& Col, DBT iDBType)
//-----------------------------------------------------------------------------
{
	switch (iDBType)
	{
		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
		case DBT::SYBASE:
			return EscapeChars (Col, ESCAPE_MSSQL);
		default:
			return EscapeChars (Col, ESCAPE_MYSQL, '\\');
	}

} // EscapeChars

//-----------------------------------------------------------------------------
std::size_t KROW::CreateColumns(KStringView sColumns)
//-----------------------------------------------------------------------------
{
	// expects a comma-delimed list of columns (without table aliases)

	std::size_t iCreated { 0 };

	for (const auto sColumn : sColumns.Split())
	{
		if (!Exists(sColumn))
		{
			if (KCOLS::Add(sColumn) != KCOLS::end())
			{
				++iCreated;
			}
		}
	}

	return iCreated;

} // CreateColumns

//-----------------------------------------------------------------------------
bool KROW::SetValue (KStringView sColName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	// do not replace this with a simple KCOLS::Add() !
	auto it = KCOLS::find (sColName);

	if (it == KCOLS::end())
	{
		return (KCOLS::Add (sColName, KCOL(sValue)) != KCOLS::end());
	}
	else
	{
		it->second.sValue = sValue;
		return (true);
	}
}

//-----------------------------------------------------------------------------
bool KROW::SetValue (KStringView sColName, int64_t iValue)
//-----------------------------------------------------------------------------
{
	// do not replace this with a simple KCOLS::Add() !
	auto it = KCOLS::find (sColName);

	if (it == KCOLS::end())
	{
		return (KCOLS::Add (sColName, KCOL(KString::to_string(iValue), KCOL::NUMERIC)) != KCOLS::end());
	}
	else
	{
		it->second.sValue = KString::to_string(iValue);
		return (true);
	}
}

//-----------------------------------------------------------------------------
bool KROW::SetValue (KStringView sColName, double nValue)
//-----------------------------------------------------------------------------
{
	// do not replace this with a simple KCOLS::Add() !
	auto it = KCOLS::find (sColName);

	if (it == KCOLS::end())
	{
		return (KCOLS::Add (sColName, KCOL(KString::to_string(nValue), KCOL::MONEY)) != KCOLS::end());
	}
	else
	{
		it->second.sValue = KString::to_string(nValue);
		return (true);
	}
}

//-----------------------------------------------------------------------------
bool KROW::SetFlags (KStringView sColName, KCOL::Flags Flags)
//-----------------------------------------------------------------------------
{
	auto it = KCOLS::find (sColName);
	
	if (it == KCOLS::end())
	{
		return (false);
	}
	else
	{
		it->second.SetFlags(Flags);
		return (true);
	}
}

//-----------------------------------------------------------------------------
void KROW::PrintValuesForInsert(KSQLString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	sSQL.ref() += "(";

	bool bComma = false;

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (KCOL::NONCOLUMN))
		{
			continue;
		}

		KString sHack = it.second.sValue.ToLowerASCII();
		sHack.Replace(" ","");
		bool    bHack = ((sHack == "now()") || (sHack == "now(6)") || (sHack == "{{now}}"));

		if (it.second.sValue.empty() && !it.second.IsFlag (KCOL::NULL_IS_NOT_NIL))
		{
			// Note: this is the default handling for NIL values: to place them in SQL as SQL null
			sSQL.ref() += kFormat ("{}\n\tnull", (bComma) ? "," : "");
		}
		else if (bHack || it.second.HasFlag (KCOL::NUMERIC | KCOL::MONEY | KCOL::BOOLEAN | KCOL::EXPRESSION))
		{
			sSQL.ref() += kFormat ("{}\n\t{}", (bComma) ? "," : "", it.second.sValue); // raw value, no quotes and no processing
		}
		else
		{
			// catch-all logic for all string values
			// Note: if the value is actually NIL ('') and NULL_IS_NOT_NIL is set, then the value will
			// be placed into SQL as '' instead of SQL null.
			// WARNING: SQLServer < v15 NEEDS the prefixed N in front of UTF8 string columns, or it
			// will not properly decode them!
			sSQL.ref() += kFormat ("{}\n\t{}'{}'",
							 (bComma) ? "," : "",
							 iDBType == DBT::SQLSERVER ? "N" : "",
							 EscapeChars (it, iDBType));
		}
		bComma = true;
	}

	sSQL.ref() += "\n)";

} // PrintValuesForInsert

//-----------------------------------------------------------------------------
KSQLString KROW::FormInsert (DBT iDBType, bool bIdentityInsert/*=false*/, bool bIgnore/*=false*/) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	m_sLastError.clear(); // reset
	KSQLString sSQL;

	kDebug (3, "before: {}", sSQL);
	
	if (empty())
	{
		m_sLastError = "KROW::FormInsert(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormInsert(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	sSQL.ref() += kFormat("insert{} into {} (", bIgnore ? " ignore" : "", GetTablename());

	kDebug (3, GetTablename());

	LogRowLayout();

	bool bComma = false;

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (KCOL::NONCOLUMN))
		{
			continue;
		}
		
		sSQL.ref() += kFormat ("{}\n\t{}", (bComma) ? "," : "", it.first);
		bComma = true;
	}

	sSQL += "\n) values ";

	PrintValuesForInsert(sSQL, iDBType);

	if (bIdentityInsert && (iDBType == DBT::SQLSERVER || iDBType == DBT::SQLSERVER15))
	{
		sSQL.ref() = kFormat("SET IDENTITY_INSERT {} ON \n"
					"{} \n"
					"SET IDENTITY_INSERT {} OFF"
					, GetTablename(), sSQL, GetTablename());
	}
	
	kDebug (3, "after: {}", sSQL);
	
	return sSQL;

} // FormInsert

//-----------------------------------------------------------------------------
bool KROW::AppendInsert (KSQLString& sSQL, DBT iDBType, bool bIdentityInsert/*=false*/, bool bIgnore/*=true*/) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	if (sSQL.empty())
	{
		// this is the first record, create the value syntax insert..
		sSQL = FormInsert (iDBType, bIdentityInsert, bIgnore);
		return !sSQL.empty();
	}

	m_sLastError.clear(); // reset

	kDebug (3, "before: {}", sSQL);

	if (empty())
	{
		m_sLastError = "KROW::AppendInsert(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	LogRowLayout();

	sSQL += ",";

	PrintValuesForInsert(sSQL, iDBType);

	if (bIdentityInsert && (iDBType == DBT::SQLSERVER || iDBType == DBT::SQLSERVER15))
	{
		m_sLastError = "KROW::AppendInsert(): cannot append row to existing DDL statement with IDENTITY_INSERT provision";
		kDebugLog (1, m_sLastError);
		return false;
	}

	kDebug (3, "after: {}", sSQL);

	return (true);

} // AppendInsert

//-----------------------------------------------------------------------------
KSQLString KROW::FormUpdate (DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	m_sLastError.clear(); // reset
	KSQLString sSQL;
	
	if (empty())
	{
		m_sLastError = "KROW::FormUpdate(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormUpdate(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	KROW Keys;

	sSQL.ref() += kFormat ("update {} set\n", GetTablename());

	kDebug (3, m_sTablename);

	LogRowLayout();

	bool  bComma = false;
	for (const auto& it : *this)
	{
		if (it.second.IsFlag (KCOL::NONCOLUMN))
		{
			continue;
		}
		else if (it.second.IsFlag (KCOL::PKEY))
		{
			Keys.Add (it.first, it.second);
		}
		else if (it.second.HasFlag (KCOL::EXPRESSION | KCOL::BOOLEAN))
		{
			sSQL.ref() += kFormat ("\t{}{}={}\n", (bComma) ? "," : "", it.first, it.second.sValue);
			bComma = true;
		}
		else
		{
			if (it.second.sValue.empty())
			{
				if (it.second.HasFlag (KCOL::INCREMENT))
				{
					continue; // omit column from update
				}
				else
				{
					sSQL.ref() += kFormat ("\t{}{}=null\n", (bComma) ? "," : "", it.first);
				}
			}
			else
			{
				sSQL.ref() += kFormat ("\t{}{}=", (bComma) ? "," : "", it.first);

				if (it.second.HasFlag (KCOL::INCREMENT))
				{
					sSQL.ref() += kFormat ("{}+", it.first);
				}

				if (it.second.HasFlag (KCOL::NUMERIC | KCOL::MONEY | KCOL::EXPRESSION | KCOL::BOOLEAN))
				{
					sSQL.ref() += kFormat ("{}\n", EscapeChars (it, iDBType));
				}
				else
				{
					// WARNING: SQLServer < v15 NEEDS the prefixed N in front of UTF8 string columns, or it
					// will not properly decode them!
					sSQL.ref() += kFormat ("{}'{}'\n",
										   iDBType == DBT::SQLSERVER ? "N" : "",
										   EscapeChars (it, iDBType));
				}
			}
			bComma = true;
		}
	}

	kDebug (GetDebugLevel()+1, "update will rely on {} keys", Keys.size());

	if (Keys.empty())
	{
		m_sLastError.Format("KROW::FormUpdate({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, m_sLastError);
		sSQL.clear();
		return sSQL;
	}

	bool bFirstKey { true };
	for (const auto& it : Keys)
	{
		KStringView sPrefix;
		if (bFirstKey)
		{
			bFirstKey = false;
			sPrefix = " where ";
		}
		else
		{
			sPrefix = "   and ";
		}
		
		sSQL.ref() += kFormat("{}{}=", sPrefix, it.first);

		if (it.second.HasFlag(KCOL::NUMERIC | KCOL::MONEY | KCOL::EXPRESSION | KCOL::BOOLEAN))
		{
			sSQL.ref() += kFormat("{}\n", EscapeChars (it, iDBType));
		}
		else
		{
			// WARNING: SQLServer < v15 NEEDS the prefixed N in front of UTF8 string columns, or it
			// will not properly decode them!
			sSQL.ref() += kFormat("{}'{}'\n",
								  iDBType == DBT::SQLSERVER ? "N" : "",
								  EscapeChars (it, iDBType));
		}
	}

	return sSQL;

} // FormUpdate

//-----------------------------------------------------------------------------
KSQLString KROW::FormSelect (DBT iDBType, bool bSelectAllColumns) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	m_sLastError.clear(); // reset
	KSQLString sSQL;

	if (empty())
	{
		bSelectAllColumns = true;
	}

	if (GetTablename().empty())
	{
		m_sLastError = "KROW::FormSelect(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	kDebug (3, GetTablename());

	if (!bSelectAllColumns)
	{
		sSQL = "select \n";

		LogRowLayout();

		std::size_t iColumns { 0 };

		for (const auto& it : *this)
		{
			if (!it.second.HasFlag (KCOL::NONCOLUMN | KCOL::PKEY))
			{
				sSQL.ref() += kFormat("\t{} {}\n", (iColumns++) ? "," : "", it.first);
			}
		}

		if (!iColumns)
		{
			bSelectAllColumns = true;
		}
	}

	// do not replace this with an else - bSelectAllColumns can change in the previous branch
	if (bSelectAllColumns)
	{
		sSQL = "select * \n";
	}


	sSQL.ref() += kFormat("  from {}\n", GetTablename());

	std::size_t iKeys { 0 };

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (KCOL::PKEY) && !it.second.sValue.empty())
		{
			KStringView sPrefix = !iKeys++ ? " where " : "   and ";

			if (it.second.HasFlag(KCOL::NUMERIC | KCOL::MONEY | KCOL::EXPRESSION | KCOL::BOOLEAN))
			{
				sSQL.ref() += kFormat("{}{}={}\n", sPrefix, it.first, EscapeChars (it, iDBType));
			}
			else
			{
				// WARNING: SQLServer < v15 NEEDS the prefixed N in front of UTF8 string columns, or it
				// will not properly decode them!
				sSQL.ref() += kFormat("{}{}={}'{}'\n",
								sPrefix,
								it.first,
								iDBType == DBT::SQLSERVER ? "N" : "",
								EscapeChars (it, iDBType));
			}
		}
	}

	kDebug (GetDebugLevel()+1, "KROW::FormSelect: select will rely on {} keys", iKeys);

	return sSQL;

} // FormSelect

//-----------------------------------------------------------------------------
KSQLString KROW::FormDelete (DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	m_sLastError.clear(); // reset
	KSQLString sSQL;

	kDebug (3, "before: {}", sSQL);

	if (empty())
	{
		m_sLastError = "KROW::FormDelete(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormDelete(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return sSQL;
	}

	LogRowLayout();

	Index kk = 0;

	sSQL.ref() += kFormat("delete from {}\n", GetTablename());

	kDebug (3, GetTablename());

	for (const auto& it : *this)
	{
		if (!it.second.IsFlag (KCOL::PKEY))
		{
			continue;
		}

		if (it.second.sValue.empty())
		{
			sSQL.ref() += kFormat(" {} {} is null\n",(!kk) ? "where" : "  and", it.first);
		}
		else if (it.second.HasFlag(KCOL::NUMERIC | KCOL::MONEY | KCOL::EXPRESSION | KCOL::BOOLEAN))
		{
			sSQL.ref() += kFormat(" {} {}={}\n",     (!kk) ? "where" : "  and", it.first, EscapeChars (it, iDBType));
		}
		else
		{
			// WARNING: SQLServer < v15 NEEDS the prefixed N in front of UTF8 string columns, or it
			// will not properly decode them!
			sSQL.ref() += kFormat(" {} {}={}'{}'\n",
							(!kk) ? "where" : "  and",
							it.first,
							iDBType == DBT::SQLSERVER ? "N" : "",
							EscapeChars (it, iDBType));
		}

		++kk;
	}

	if (!kk)
	{
		m_sLastError.Format("KROW::FormDelete({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, m_sLastError);
		sSQL.clear();
		return sSQL;
	}
	
	kDebug (3, "after: {}", sSQL);

	return sSQL;

} // FormDelete

//-----------------------------------------------------------------------------
bool KROW::AddCol (KStringView sColName, const LJSON& Value, KCOL::Flags Flags, KCOL::Len iMaxLen)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	// make sure all type flags are removed
	Flags &= KCOL::MODE_FLAGS;

	switch (Value.type())
	{
		case LJSON::value_t::object:
		case LJSON::value_t::array:
			return AddCol(sColName, Value.dump(-1), Flags | KCOL::JSON, iMaxLen);

		case LJSON::value_t::string:
		case LJSON::value_t::binary:
			return AddCol(sColName, Value.get<KJSON::string_t>(), Flags | KCOL::NOFLAG, iMaxLen);

		case LJSON::value_t::number_integer:
			return AddCol(sColName, Value.get<KJSON::number_integer_t>(), Flags | KCOL::NUMERIC, iMaxLen);

		case LJSON::value_t::number_unsigned:
			return AddCol(sColName, Value.get<KJSON::number_unsigned_t>(), Flags | KCOL::NUMERIC, iMaxLen);

		case LJSON::value_t::number_float:
			return AddCol(sColName, Value.get<KJSON::number_float_t>(), Flags | KCOL::MONEY, iMaxLen);

		case LJSON::value_t::boolean:
			return AddCol(sColName, Value.get<KJSON::boolean_t>(), Flags | KCOL::BOOLEAN, iMaxLen);

		case LJSON::value_t::null:
			if (Flags & KCOL::NULL_IS_NOT_NIL)
			{
				return AddCol(sColName, "null", KCOL::EXPRESSION);
			}
			else
			{
				return AddCol(sColName, "", Flags | KCOL::JSON, iMaxLen);
			}

		case LJSON::value_t::discarded:
			kDebug(2, "could not identify JSON type for {}", sColName);
			return false;
	}

	return false;

} // AddCol

//-----------------------------------------------------------------------------
LJSON KROW::to_json (CONVERSION Flags/*=CONVERSION::NO_CONVERSION*/) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	LJSON json = LJSON::object();

	for (auto& col : *this)
	{
		kDebug (4, "{:35}: 0x{:08x} = {}", col.first, std::to_underlying(col.second.GetFlags()), col.second.FlagsToString());

		KString sKey = col.first;

		if (Flags & CONVERSION::KEYS_TO_LOWER)
		{
			sKey.MakeLower();
		}
		else if (Flags & CONVERSION::KEYS_TO_UPPER)
		{
			sKey.MakeUpper();
		}

		if (col.second.HasFlag(KCOL::NONCOLUMN))
		{
			continue;
		}
		else if (col.second.HasFlag(KCOL::INT64NUMERIC))
		{
			// large integers > 53 bits have no representation in JavaScript and need to
			// be stored as string values..
			json[sKey] = col.second.sValue;

			// JK: we need to solved this "large int" problem in KJSON/LJSON
			// Almost all the integer fields in the database that we care about (or compute)
			// are being serialized as strings in JSON.
			// Client-side JavaScript (i.e. UI code) is treating them as strings.
			// Operations like "+" and "+=" end up doing string concatenation instead of math.
			// Can we fix Lohmann's datatypes or some such?

			// JS: No, this is not a problem in nlohmann::json but in JavaScript itself.
			// JSON can support 64 bit integers. If we were to support only other C++
			// or Java or Python clients, then we could support 64 bit data.
			// BUT:
			// The MAX_SAFE_INTEGER constant of JavaScript has a value of 9007199254740991.
			// The reasoning behind that number is that JavaScript uses double-precision
			// floating-point format numbers as specified in IEEE 754 and can only safely
			// represent numbers between -(2^53 - 1) and 2^53 - 1.
			//
			// Please see this SO topic (and skip the first, bad, reply):
			// https://stackoverflow.com/questions/209869/what-is-the-accepted-way-to-send-64-bit-values-over-json
			//
			// This is why we have to convert 64 bit integers into strings.
		}
		else if (col.second.HasFlag(KCOL::BOOLEAN) || sKey.StartsWith("is_"))
		{
			json[sKey] = col.second.sValue.Bool();
		}
		else if (col.second.HasFlag(KCOL::MONEY))
		{
			json[sKey] = col.second.sValue.Double();
		}
		else if (col.second.HasFlag(KCOL::NUMERIC))
		{
			// TODO get a strategy as to how to and if to adapt to locales with other chars than . as the
			// decimal separator
			if (col.second.sValue.contains('.'))
			{
				json[sKey] = col.second.sValue.Double();
			}
			// note: we need to split out signed and unsigned to avoid overflows on some platforms
			// when the string represents a really large number (like an FNV hash):
			// signed integer overflow: 1063188930240168165 * 10 cannot be represented in type 'long int'
			else if (col.second.sValue.front() == '-')
			{
				json[sKey] = col.second.sValue.Int64();
			}
			else
			{
				json[sKey] = col.second.sValue.UInt64();
			}
		}
#if 0
		else if (/*(col.second.iFlags & KCOL::NULL_IS_NOT_NIL) &&*/ col.second.sValue.empty())
		{
			json[sKey] = KJSON();
		}
#endif
		else if (col.second.HasFlag(KCOL::JSON))
		{
			// this is a json serialization
#ifndef DEKAF2_WRAPPED_KJSON
			bool bOld = KLog::getInstance().ShowStackOnJsonError(false);
#endif
			DEKAF2_TRY
			{
				LJSON object;
				kjson::Parse(object, col.second.sValue);
#ifndef DEKAF2_WRAPPED_KJSON
				KLog::getInstance().ShowStackOnJsonError(bOld);
#endif
				json[sKey] = std::move(object);
			}
			DEKAF2_CATCH(const LJSON::exception& exc)
			{
#ifdef DEKAF2_IS_MSC
				exc.what();
#endif
				// not a valid json object / array, store it as a string
				kjson::SetStringFromUTF8orLatin1(json[sKey], col.second.sValue);
			}
#ifndef DEKAF2_WRAPPED_KJSON
			KLog::getInstance().ShowStackOnJsonError(bOld);
#endif
		}
		else
		{
			// strings
			if (!col.second.sValue.empty()
				&& ((col.second.sValue.front() == '{' && col.second.sValue.back() == '}')
					|| (col.second.sValue.front() == '[' && col.second.sValue.back() == ']')))
			{
				// we assume this is a json serialization
#ifndef DEKAF2_WRAPPED_KJSON
				bool bOld = KLog::getInstance().ShowStackOnJsonError(false);
#endif
				DEKAF2_TRY
				{
					LJSON object;
					kjson::Parse(object, col.second.sValue);
#ifndef DEKAF2_WRAPPED_KJSON
					KLog::getInstance().ShowStackOnJsonError(bOld);
#endif
					json[sKey] = std::move(object);
					// continue the loop right here, do NOT fall through to the plain string case!
					continue;
				}
				DEKAF2_CATCH(const LJSON::exception& exc)
				{
#ifdef DEKAF2_IS_MSC
					exc.what();
#endif
					// not a valid json object / array, store it as a string
					// fall through to checked string assignment
				}
#ifndef DEKAF2_WRAPPED_KJSON
				KLog::getInstance().ShowStackOnJsonError(bOld);
#endif
			}
			// treat this as an unstructured string, but check for proper UTF8
			kjson::SetStringFromUTF8orLatin1(json[sKey], col.second.sValue);
		}
	}

	return json;

} // to_json

//-----------------------------------------------------------------------------
KString KROW::to_csv (bool bHeaders/*=false*/, CONVERSION Flags/*=CONVERSION::NO_CONVERSION*/) const
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	// shall we print modified header columns?
	if (DEKAF2_UNLIKELY(bHeaders && (Flags & (KEYS_TO_LOWER | KEYS_TO_UPPER))))
	{
		// yes -> use a vector of KStrings
		std::vector<KString> Columns;
		Columns.reserve(this->size());

		for (const auto& col : *this)
		{
			kDebug (4, "{:35}: 0x{:08x} = {}", col.first, std::to_underlying(col.second.GetFlags()), col.second.FlagsToString());

			if (col.second.IsFlag(KCOL::NONCOLUMN))
			{
				continue;
			}

			Columns.push_back(col.first);

			if (Flags & CONVERSION::KEYS_TO_LOWER)
			{
				Columns.back().MakeLower();
			}
			else if (Flags & CONVERSION::KEYS_TO_UPPER)
			{
				Columns.back().MakeUpper();
			}
		}

		return KCSV().Write(Columns);
	}
	else
	{
		// use a vector of KStringViews, no column modifications needed
		std::vector<KStringView> Columns;
		Columns.reserve(this->size());

		for (const auto& col : *this)
		{
			kDebug (4, "{:35}: 0x{:08x} = {}", col.first, std::to_underlying(col.second.GetFlags()), col.second.FlagsToString());

			if (col.second.IsFlag(KCOL::NONCOLUMN))
			{
				continue;
			}

			if (DEKAF2_UNLIKELY(bHeaders))
			{
				Columns.push_back(col.first);
			}
			else
			{
				if (DEKAF2_UNLIKELY(col.second.IsFlag(KCOL::BOOLEAN)))
				{
					Columns.push_back(col.second.sValue.Bool() ? "1" : "0");
				}
				else
				{
					Columns.push_back(col.second.sValue);
				}
			}
		}

		return KCSV().Write(Columns);
	}

} // to_csv

//-----------------------------------------------------------------------------
KROW& KROW::operator+=(const KROW& another)
//-----------------------------------------------------------------------------
{
	for (auto& col : another)
	{
		AddCol (col.first, col.second.sValue, col.second.GetFlags(), col.second.GetMaxLen());
	}
	return *this;

} // operator+=(KRON)

//-----------------------------------------------------------------------------
KROW& KROW::operator+=(const LJSON& json)
//-----------------------------------------------------------------------------
{
	for (auto& it : json.items())
	{
		AddCol (it.key(), it.value());
	}
	return *this;

} // operator+=(KJSON)

//-----------------------------------------------------------------------------
bool KROW::Exists (KStringView sColName) const
//-----------------------------------------------------------------------------
{
	return find(sColName) != end();

} // Exists

DEKAF2_NAMESPACE_END
