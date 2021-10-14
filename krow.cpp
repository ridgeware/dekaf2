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

namespace dekaf2 {


// MySQL requires NUL, the quotes and backslash to be escaped. Do not escape
// by doubling the value, as the result then depends on the context (if inside
// single quotes, double double quotes will be seen as two, and vice verse)
constexpr KStringView ESCAPE_MYSQL { "\'\"\\`\0"_ksv };
// TODO check the rules for MSSQL, particularly for \0 and \Z
constexpr KStringView ESCAPE_MSSQL { "\'"_ksv   };

int16_t detail::KCommonSQLBase::m_iDebugLevel { 2 };


//-----------------------------------------------------------------------------
bool operator==(const KCOL& left, const KCOL& right)
//-----------------------------------------------------------------------------
{
	return left.GetFlags() == right.GetFlags()
		&& left.GetMaxLen() == right.GetMaxLen()
		&& left.sValue == right.sValue;
}

//-----------------------------------------------------------------------------
KString KROW::ColumnInfoForLogOutput (const KCOLS::value_type& it, Index iCol) const
//-----------------------------------------------------------------------------
{
	return kFormat("  col[{:>02}]: {:<25} {}",
		    iCol,
		    it.first,
			FlagsToString(it.second.GetFlags()));

} // ColumnInfoForLogOutput

//-----------------------------------------------------------------------------
KString KROW::FlagsToString (uint64_t iFlags)
//-----------------------------------------------------------------------------
{
	KString sPretty;

	if (iFlags & PKEY)            {  sPretty += "[PKEY]";            }
	if (iFlags & NONCOLUMN)       {  sPretty += "[NONCOLUMN]";       }
	if (iFlags & EXPRESSION)      {  sPretty += "[EXPRESSION]";      }
	if (iFlags & INSERTONLY)      {  sPretty += "[INSERTONLY]";      }
	if (iFlags & NUMERIC)         {  sPretty += "[NUMERIC]";         }
	if (iFlags & NULL_IS_NOT_NIL) {  sPretty += "[NULL_IS_NOT_NIL]"; }
	if (iFlags & BOOLEAN)         {  sPretty += "[BOOLEAN]";         }
	if (iFlags & JSON)            {  sPretty += "[JSON]";            }
	if (iFlags & INT64NUMERIC)    {  sPretty += "[INT64NUMERIC]";    }

	return (sPretty);

} // FlagsToString

//-----------------------------------------------------------------------------
void KROW::LogRowLayout(int iLogLevel) const
//-----------------------------------------------------------------------------
{
	if (kWouldLog(iLogLevel))
	{
		int16_t iii = 0;
		for (const auto& it : *this)
		{
			kDebugLog (iLogLevel, ColumnInfoForLogOutput(it, iii++));
		}
	}

} // LogRowLayout

//-----------------------------------------------------------------------------
bool KROW::NeedsEscape (KStringView sCol, KStringView sCharsToEscape)
//-----------------------------------------------------------------------------
{
	return sCol.find_first_of(sCharsToEscape) != KStringView::npos;

} // NeedsEscape

//-----------------------------------------------------------------------------
KString KROW::EscapeChars (KStringView sCol, KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as it's own escape char (i.e. it gets doubled up).
	KString sEscaped;
	sEscaped.reserve(sCol.size());

	for (KStringView::size_type iStart; (iStart = sCol.find_first_of(sCharsToEscape)) != KStringView::npos; )
	{
		sEscaped += sCol.substr(0, iStart);
		auto ch = sCol[iStart];
		if (!ch) ch = '0'; // NUL needs special treatment
		sEscaped += (iEscapeChar) ? iEscapeChar : ch;
		sEscaped += ch;
		// prepare for next round
		sCol.remove_prefix(++iStart);
	}
	// add the remainder of the input string
	sEscaped += sCol;

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
KString KROW::EscapeChars (KStringView sCol, DBT iDBType)
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
KString KROW::EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as it's own escape char (i.e. it gets doubled up).

	KString sEscaped;

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
				// now walk back to the begin of a sequence of backslashes
				while (iClipAt > 0 && sEscaped[iClipAt-1] == iEscapeChar)
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
				while (iClipAt > 0 && sCharsToEscape.find(sEscaped[iClipAt-1]) != KStringView::npos)
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
KString KROW::EscapeChars (const KROW::value_type& Col, DBT iDBType)
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
	auto it = KCOLS::find (sColName);
	if (it == KCOLS::end())
	{
		return (KCOLS::Add (sColName, KCOL(KString::to_string(iValue), NUMERIC)) != KCOLS::end());
	}
	else
	{
		it->second.sValue = KString::to_string(iValue);
		return (true);
	}
}

//-----------------------------------------------------------------------------
bool KROW::SetFlags (KStringView sColName, KCOL::Flags iFlags)
//-----------------------------------------------------------------------------
{
	auto it = KCOLS::find (sColName);
	if (it == KCOLS::end())
	{
		return (false);
	}
	else
	{
		it->second.SetFlags(iFlags);
		return (true);
	}
}

//-----------------------------------------------------------------------------
void KROW::PrintValuesForInsert(KString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	sSQL += '(';

	bool bComma = false;

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}

		KString sHack = (it.second.sValue.empty()) ? "" : it.second.sValue; // TODO:JOACHIM: REMOVE ME
		sHack.MakeLower();
		sHack.Replace(" ","");
		bool    bHack = ((sHack == "now()") || (sHack == "now(6)") || (sHack == "{{now}}"));

		if (it.second.sValue.empty() && !it.second.IsFlag (NULL_IS_NOT_NIL))
		{
			// Note: this is the default handling for NIL values: to place them in SQL as SQL null
			sSQL += kFormat ("{}\n\tnull", (bComma) ? "," : "");
		}
		else if (bHack || it.second.HasFlag (NUMERIC | BOOLEAN | EXPRESSION))
		{
			sSQL += kFormat ("{}\n\t{}", (bComma) ? "," : "", it.second.sValue); // raw value, no quotes and no processing
		}
		else
		{
			// catch-all logic for all string values
			// Note: if the value is actually NIL ('') and NULL_IS_NOT_NIL is set, then the value will
			// be placed into SQL as '' instead of SQL null.
			sSQL += kFormat ("{}\n\t{}'{}'",
							 (bComma) ? "," : "",
							 iDBType == DBT::SQLSERVER ? "N" : "",
							 EscapeChars (it, iDBType));
		}
		bComma = true;
	}

	sSQL += "\n)";

} // PrintValuesForInsert

//-----------------------------------------------------------------------------
bool KROW::FormInsert (KString& sSQL, DBT iDBType, bool bIdentityInsert/*=false*/, bool bIgnore/*=false*/) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	m_sLastError.clear(); // reset
	sSQL.clear();

	kDebug (3, "before: {}", sSQL);
	
	if (empty())
	{
		m_sLastError = "KROW::FormInsert(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormInsert(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	sSQL += kFormat("insert{} into {} (", bIgnore ? " ignore" : "", GetTablename());

	kDebug (3, GetTablename());

	LogRowLayout();

	bool bComma = false;

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}
		
		sSQL += kFormat ("{}\n\t{}", (bComma) ? "," : "", it.first);
		bComma = true;
	}

	sSQL += "\n) values ";

	PrintValuesForInsert(sSQL, iDBType);

	if (bIdentityInsert && (iDBType == DBT::SQLSERVER || iDBType == DBT::SQLSERVER15))
	{
		sSQL = kFormat("SET IDENTITY_INSERT {} ON \n"
					"{} \n"
					"SET IDENTITY_INSERT {} OFF"
					, GetTablename(), sSQL, GetTablename());
	}
	
	kDebug (3, "after: {}", sSQL);
	
	return (true);

} // FormInsert

//-----------------------------------------------------------------------------
bool KROW::AppendInsert (KString& sSQL, DBT iDBType, bool bIdentityInsert/*=false*/, bool bIgnore/*=true*/) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (sSQL.empty())
	{
		// this is the first record, create the value syntax insert..
		return FormInsert (sSQL, iDBType, bIdentityInsert, bIgnore);
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
bool KROW::FormUpdate (KString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	m_sLastError.clear(); // reset
	sSQL.clear();
	
	if (!size())
	{
		m_sLastError = "KROW::FormUpdate(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormUpdate(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	KROW Keys;

	sSQL += kFormat ("update {} set\n", GetTablename());

	kDebug (3, m_sTablename);

	LogRowLayout();

	bool  bComma = false;
	for (const auto& it : *this)
	{
		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}
		else if (it.second.IsFlag (PKEY))
		{
			Keys.Add (it.first, it.second);
		}
		else if (it.second.HasFlag (EXPRESSION | BOOLEAN))
		{
			sSQL += kFormat ("\t{}{}={}\n", (bComma) ? "," : "", it.first, it.second.sValue);
			bComma = true;
		}
		else
		{
			if (it.second.sValue.empty())
			{
				sSQL += kFormat ("\t{}{}=null\n", (bComma) ? "," : "", it.first);
			}
			else
			{
				if (it.second.HasFlag (NUMERIC | EXPRESSION | BOOLEAN))
				{
					sSQL += kFormat ("\t{}{}={}\n", (bComma) ? "," : "", it.first, EscapeChars (it, iDBType));
				}
				else
				{
					sSQL += kFormat ("\t{}{}={}'{}'\n",
									 (bComma) ? "," : "",
									 it.first,
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
		return (false);
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
		
		if (it.second.HasFlag(NUMERIC | EXPRESSION | BOOLEAN))
		{
			sSQL += kFormat("{}{}={}\n", sPrefix, it.first, EscapeChars (it, iDBType));
		}
		else
		{
			sSQL += kFormat("{}{}={}'{}'\n",
							sPrefix,
							it.first,
							iDBType == DBT::SQLSERVER ? "N" : "",
							EscapeChars (it, iDBType));
		}
	}

	return (true);

} // FormUpdate

//-----------------------------------------------------------------------------
bool KROW::FormSelect (KString& sSQL, DBT iDBType, bool bSelectAllColumns) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	m_sLastError.clear(); // reset
	sSQL.clear();

	if (!size())
	{
		bSelectAllColumns = true;
	}

	if (GetTablename().empty())
	{
		m_sLastError = "KROW::FormSelect(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	kDebug (3, GetTablename());

	if (!bSelectAllColumns)
	{
		sSQL = "select \n";

		LogRowLayout();

		std::size_t iColumns { 0 };

		for (const auto& it : *this)
		{
			if (!it.second.HasFlag (NONCOLUMN | PKEY))
			{
				sSQL += kFormat("\t{} {}\n", (iColumns++) ? "," : "", it.first);
			}
		}

		if (!iColumns)
		{
			bSelectAllColumns = true;
		}
	}

	if (bSelectAllColumns)
	{
		sSQL = "select * \n";
	}


	sSQL += kFormat("  from {}\n", GetTablename());

	std::size_t iKeys { 0 };

	for (const auto& it : *this)
	{
		if (it.second.IsFlag (PKEY) && !it.second.sValue.empty())
		{
			KStringView sPrefix = !iKeys++ ? " where " : "   and ";

			if (it.second.HasFlag(NUMERIC | EXPRESSION | BOOLEAN))
			{
				sSQL += kFormat("{}{}={}\n", sPrefix, it.first, EscapeChars (it, iDBType));
			}
			else
			{
				sSQL += kFormat("{}{}={}'{}'\n",
								sPrefix,
								it.first,
								iDBType == DBT::SQLSERVER ? "N" : "",
								EscapeChars (it, iDBType));
			}
		}
	}

	kDebug (GetDebugLevel()+1, "KROW::FormSelect: select will rely on {} keys", iKeys);

	return (true);

} // FormSelect

//-----------------------------------------------------------------------------
bool KROW::FormDelete (KString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	m_sLastError.clear(); // reset
	sSQL.clear();

	kDebug (3, "before: {}", sSQL);

	if (empty())
	{
		m_sLastError = "KROW::FormDelete(): no columns defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError = "KROW::FormDelete(): no tablename defined.";
		kDebugLog (1, m_sLastError);
		return (false);
	}

	LogRowLayout();

	Index kk = 0;

	sSQL += kFormat("delete from {}\n", GetTablename());

	kDebug (3, GetTablename());

	for (const auto& it : *this)
	{
		if (!it.second.IsFlag (PKEY))
		{
			continue;
		}

		if (it.second.sValue.empty())
		{
			sSQL += kFormat(" {} {} is null\n",(!kk) ? "where" : "  and", it.first);
		}
		else if (it.second.HasFlag(NUMERIC | EXPRESSION | BOOLEAN))
		{
			sSQL += kFormat(" {} {}={}\n",     (!kk) ? "where" : "  and", it.first, EscapeChars (it, iDBType));
		}
		else
		{
			sSQL += kFormat(" {} {}={}'{}'\n",
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
		return (false);
	}
	
	kDebug (3, "after: {}", sSQL);

	return (true);

} // FormDelete

//-----------------------------------------------------------------------------
bool KROW::AddCol (KStringView sColName, const KJSON& Value, KCOL::Flags iFlags, KCOL::Len iMaxLen)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// make sure all type flags are removed
	iFlags &= MODE_FLAGS;

	switch (Value.type())
	{
		case KJSON::value_t::object:
		case KJSON::value_t::array:
			return AddCol(sColName, Value.dump(-1), iFlags | JSON, iMaxLen);

		case KJSON::value_t::string:
		case KJSON::value_t::binary:
			return AddCol(sColName, Value.get<KJSON::string_t>(), iFlags | NOFLAG, iMaxLen);

		case KJSON::value_t::number_integer:
			return AddCol(sColName, Value.get<KJSON::number_integer_t>(), iFlags | NUMERIC, iMaxLen);

		case KJSON::value_t::number_unsigned:
			return AddCol(sColName, Value.get<KJSON::number_unsigned_t>(), iFlags | NUMERIC, iMaxLen);

		case KJSON::value_t::number_float:
			return AddCol(sColName, Value.get<KJSON::number_float_t>(), iFlags | NUMERIC, iMaxLen);

		case KJSON::value_t::boolean:
			return AddCol(sColName, Value.get<KJSON::boolean_t>(), iFlags | BOOLEAN, iMaxLen);

		case KJSON::value_t::null:
			if (iFlags & KROW::NULL_IS_NOT_NIL)
			{
				return AddCol(sColName, "null", KROW::EXPRESSION);
			}
			else
			{
				return AddCol(sColName, "", iFlags | JSON, iMaxLen);
			}

		case KJSON::value_t::discarded:
			kDebug(2, "could not identify JSON type for {}", sColName);
			return false;
	}

	return false;

} // AddCol

//-----------------------------------------------------------------------------
KJSON KROW::to_json (uint64_t iFlags/*=0*/) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	KJSON json;

	for (auto& col : *this)
	{
		kDebug (3, "{:35}: 0x{:08x} = {}", col.first, col.second.GetFlags(), KROW::FlagsToString(col.second.GetFlags()));

		KString sKey = col.first;
		if (iFlags & KEYS_TO_LOWER)
		{
			sKey.MakeLower();
		}
		else if (iFlags & KEYS_TO_UPPER)
		{
			sKey.MakeUpper();
		}

		if (col.second.IsFlag(NONCOLUMN))
		{
			continue;
		}
		else if (col.second.IsFlag(INT64NUMERIC))
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
		else if (col.second.IsFlag(BOOLEAN) || sKey.StartsWith("is_"))
		{
			json[sKey] = col.second.sValue.Bool();
		}
		else if (col.second.IsFlag(NUMERIC))
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
		else if (/*(col.second.iFlags & KROW::NULL_IS_NOT_NIL) &&*/ col.second.sValue.empty())
		{
			json[sKey] = NULL;
		}
#endif
		else if (col.second.IsFlag(JSON))
		{
			// this is a json serialization
			DEKAF2_TRY
			{
				KJSON object;
				bool bOld = KLog::getInstance().ShowStackOnJsonError(false);
				kjson::Parse(object, col.second.sValue);
				KLog::getInstance().ShowStackOnJsonError(bOld);
				json[sKey] = object;
			}
			DEKAF2_CATCH(const KJSON::exception& exc)
			{
#ifdef _MSC_VER
				exc.what();
#endif
				// not a valid json object / array, store it as a string
				kjson::SetStringFromUTF8orLatin1(json[sKey], col.second.sValue);
			}
		}
		else
		{
			// strings
			if (!col.second.sValue.empty()
				&& ((col.second.sValue.front() == '{' && col.second.sValue.back() == '}')
					|| (col.second.sValue.front() == '[' && col.second.sValue.back() == ']')))
			{
				// we assume this is a json serialization
				DEKAF2_TRY
				{
					KJSON object;
					bool bOld = KLog::getInstance().ShowStackOnJsonError(false);
					kjson::Parse(object, col.second.sValue);
					KLog::getInstance().ShowStackOnJsonError(bOld);
					json[sKey] = object;
				}
				DEKAF2_CATCH(const KJSON::exception& exc)
				{
#ifdef _MSC_VER
					exc.what();
#endif
					// not a valid json object / array, store it as a string
					kjson::SetStringFromUTF8orLatin1(json[sKey], col.second.sValue);
				}
			}
			else
			{
				kjson::SetStringFromUTF8orLatin1(json[sKey], col.second.sValue);
			}
		}
	}

	return json;

} // to_json

//-----------------------------------------------------------------------------
KString KROW::to_csv (bool bHeaders/*=false*/, uint64_t iFlags/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// shall we print modified header columns?
	if (DEKAF2_UNLIKELY(bHeaders && (iFlags & (KEYS_TO_LOWER | KEYS_TO_UPPER))))
	{
		// yes -> use a vector of KStrings
		std::vector<KString> Columns;
		Columns.reserve(this->size());

		for (const auto& col : *this)
		{
			kDebug (3, "{:35}: 0x{:08x} = {}", col.first, col.second.GetFlags(), KROW::FlagsToString(col.second.GetFlags()));

			if (col.second.IsFlag(NONCOLUMN))
			{
				continue;
			}

			Columns.push_back(col.first);

			if (iFlags & KEYS_TO_LOWER)
			{
				Columns.back().MakeLower();
			}
			else if (iFlags & KEYS_TO_UPPER)
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
			kDebug (3, "{:35}: 0x{:08x} = {}", col.first, col.second.GetFlags(), KROW::FlagsToString(col.second.GetFlags()));

			if (col.second.IsFlag(NONCOLUMN))
			{
				continue;
			}

			if (DEKAF2_UNLIKELY(bHeaders))
			{
				Columns.push_back(col.first);
			}
			else
			{
				if (DEKAF2_UNLIKELY(col.second.IsFlag(BOOLEAN)))
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
KROW& KROW::operator+=(const KJSON& json)
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

} // end of namespace dekaf2
