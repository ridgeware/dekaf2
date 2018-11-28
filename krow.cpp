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

using namespace dekaf2;

// MySQL requires NUL, the quotes and backslash to be escaped. Do not escape
// by doubling the value, as the result then depends on the context (if inside
// single quotes, double double quotes will be seen as two, and vice verse)
constexpr KStringView ESCAPE_MYSQL { "\'\"\\\0" };
// TODO check the rules for MSSQL, particularly for \0 and \Z
constexpr KStringView ESCAPE_MSSQL { "\'"   };

int16_t detail::KCommonSQLBase::m_iDebugLevel { 2 };

//-----------------------------------------------------------------------------
KString KROW::ColumnInfoForLogOutput (const KCOLS::value_type& it, Index iCol) const
//-----------------------------------------------------------------------------
{
	KString sLogMessage;

	sLogMessage.Format("  col[{:>02}]: {:<25} {}{}{}{}{}{}",
		    iCol,
		    it.first,
		    !it.second.IsFlag (PKEY)       ? "" : " [PKEY]",
		    !it.second.IsFlag (NONCOLUMN)  ? "" : " [NONCOLUMN]",
		    !it.second.IsFlag (EXPRESSION) ? "" : " [EXPRESSION]",
			!it.second.HasFlag(NUMERIC | INT64NUMERIC) ? "" : it.second.IsFlag(INT64NUMERIC) ? " [INT64NUMERIC]" : " [NUMERIC]",
		    !it.second.IsFlag (JSON)       ? "" : " [JSON]",
		    !it.second.IsFlag (BOOLEAN)    ? "" : " [BOOLEAN]");

	return sLogMessage;
}

//-----------------------------------------------------------------------------
KString KROW::EscapeChars (const KROW::value_type& Col, KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as it's own escape char (i.e. it gets doubled up).
	KString sEscaped;
	KStringView sString (Col.second.sValue);

	for (KStringView::size_type iStart; (iStart = sString.find_first_of(sCharsToEscape)) != KStringView::npos; )
	{
		sEscaped += sString.substr(0, iStart);
		auto ch = sString[iStart];
		if (!ch) ch = '0'; // NUL needs special treatment
		sEscaped += (iEscapeChar) ? iEscapeChar : ch;
		sEscaped += ch;
		// prepare for next round
		sString.remove_prefix(++iStart);
	}
	// add the remainder of the input string
	sEscaped += sString;

	// check if we shall clip the string
	auto iMaxLen = Col.second.GetMaxLen();
	if (iMaxLen)
	{
		if (sEscaped.size() > iMaxLen)
		{
			kDebugLog (1, "KSQL: clipping {}='{:.10}...' to {} chars", Col.first, sEscaped, iMaxLen);

			auto cClipped = sEscaped[iMaxLen-1];
			// watch out for a trailing escape:
			if (sCharsToEscape.find(cClipped) != KStringView::npos)
			{
				sEscaped.resize(iMaxLen-1);
			}
			else
			{
				sEscaped.resize(iMaxLen);
			}
		}

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
		case DBT::SYBASE:
			return EscapeChars (Col, ESCAPE_MSSQL);
		case DBT::MYSQL:
		default:
			return EscapeChars (Col, ESCAPE_MYSQL, '\\');
	}

} // EscapeChars

//-----------------------------------------------------------------------------
bool KROW::FormInsert (KString& sSQL, DBT iDBType, bool fIdentityInsert/*=false*/) const
//-----------------------------------------------------------------------------
{
	m_sLastError.clear(); // reset

	kDebugLog (3, "KROW:FormInsert: before: {}", sSQL);
	
	if (!size())
	{
		m_sLastError.Format("KROW::FormInsert(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError.Format("KROW::FormInsert(): no tablename defined.");

		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	KString sAdd;
	sAdd.Format("insert into {} (\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormInsert: {}", GetTablename());

	bool bComma = false;
	int16_t iii = 0;
	for (const auto& it : *this)
	{
		kDebugLog (3, ColumnInfoForLogOutput(it, iii++));

		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}
		
		sAdd.Format ("\t{}{}\n", (bComma) ? "," : "", it.first);
		sSQL += sAdd;
		bComma = true;
	}

	sSQL += ") values (\n";

	bComma = false;
	for (const auto& it : *this)
	{
		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}

		if (it.second.sValue.empty() && !it.second.IsFlag (NULL_IS_NOT_NIL))
		{
			// Note: this is the default handling for NIL values: to place them in SQL as SQL null
			sAdd.Format ("\t{}null\n", (bComma) ? "," : "");
			sSQL += sAdd;
		}
		else if (it.second.HasFlag (/*NUMERIC | BOOLEAN |*/ EXPRESSION)) // TODO:JOACHIM: remove temp hack to quote everything
		{
			sAdd.Format ("\t{}{}\n", (bComma) ? "," : "", it.second.sValue); // raw value, no quotes and no processing
			sSQL += sAdd;
		}
		else
		{
			// catch-all logic for all string values
			// Note: if the value is actually NIL ('') and NULL_IS_NOT_NIL is set, then the value will
			// be placed into SQL as '' instead of SQL null.
			sAdd.Format ("\t{}'{}'\n", (bComma) ? "," : "", EscapeChars (it, iDBType));
			sSQL += sAdd;
		}
		bComma = true;
	}

	sSQL += ")";
	
	if (fIdentityInsert)
	{
		sAdd = sSQL;
		sSQL.Format("SET IDENTITY_INSERT {} ON \n"
					"{} \n"
					"SET IDENTITY_INSERT {} OFF"
					, GetTablename(), sAdd, GetTablename());
	}
	
	kDebugLog (3, "KROW:FormInsert: after: {}", sSQL);
	
	return (true);

} // FormInsert

//-----------------------------------------------------------------------------
bool KROW::FormUpdate (KString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	m_sLastError.clear(); // reset
	
	if (!size())
	{
		m_sLastError.Format("KROW::FormUpdate(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError.Format("KROW::FormUpdate(): no tablename defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	KROW Keys;

	KString sAdd;
	sAdd.Format ("update {} set\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormUpdate: {}", m_sTablename);

	bool  bComma = false;
	Index iii = 0; // only needed for logging..
	for (const auto& it : *this)
	{
		kDebugLog (3, ColumnInfoForLogOutput(it, iii++));

		if (it.second.IsFlag (NONCOLUMN))
		{
			continue;
		}
		else if (it.second.IsFlag (PKEY))
		{
			KCOL col (it.second.sValue, it.second.GetFlags(), it.second.GetMaxLen());
			Keys.Add (it.first, col);
		}
		else if (it.second.HasFlag (EXPRESSION | BOOLEAN))
		{
			sAdd.Format ("\t{}{}={}\n", (bComma) ? "," : "", it.first, it.second.sValue);
			sSQL += sAdd;
			bComma = true;
		}
		else
		{
			if (it.second.sValue.empty())
			{
				sAdd.Format ("\t{}{}=null\n", (bComma) ? "," : "", it.first);
				sSQL += sAdd;
			}
			else
			{
				if (it.second.HasFlag (NUMERIC | EXPRESSION | BOOLEAN))
				{
					sAdd.Format ("\t{}{}={}\n", (bComma) ? "," : "", it.first, EscapeChars (it, iDBType));
				}
				else
				{
					sAdd.Format ("\t{}{}='{}'\n", (bComma) ? "," : "", it.first, EscapeChars (it, iDBType));
				}
				sSQL += sAdd;
			}
			bComma = true;
		}
	}

	kDebugLog (GetDebugLevel()+1, "KROW::FormUpdate: update will rely on {} keys", Keys.size());

	if (Keys.empty())
	{
		m_sLastError.Format("KROW::FormUpdate({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, "{}", m_sLastError);
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
			sAdd.Format("{}{}={}\n", sPrefix, it.first, EscapeChars (it, iDBType));
		}
		else
		{
			sAdd.Format("{}{}='{}'\n", sPrefix, it.first, EscapeChars (it, iDBType));
		}
		sSQL += sAdd;
	}

	return (true);

} // FormUpdate

//-----------------------------------------------------------------------------
bool KROW::FormDelete (KString& sSQL, DBT iDBType) const
//-----------------------------------------------------------------------------
{
	m_sLastError.clear(); // reset

	kDebugLog (3, "KROW:FormDelete: before: {}", sSQL);

	if (!size())
	{
		m_sLastError.Format("KROW::FormDelete(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty())
	{
		m_sLastError.Format("KROW::FormDelete(): no tablename defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	Index   kk = 0;
	KString sAdd;

	sAdd.Format("delete from {}\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormDelete: {}", m_sTablename);

	for (const auto& it : *this)
	{
		kDebugLog (3, ColumnInfoForLogOutput(it, kk));

		if (!it.second.IsFlag (PKEY))
		{
			continue;
		}

		KString sAdd;
		if (it.second.sValue.empty())
		{
			sAdd.Format(" {} {} is null\n",(!kk) ? "where" : "  and", it.first);
		}
		else if (it.second.HasFlag(NUMERIC | EXPRESSION | BOOLEAN))
		{
			sAdd.Format(" {} {}={}\n",     (!kk) ? "where" : "  and", it.first, EscapeChars (it, iDBType));
		}
		else
		{
			sAdd.Format(" {} {}='{}'\n",   (!kk) ? "where" : "  and", it.first, EscapeChars (it, iDBType));
		}
		sSQL += sAdd;

		++kk;
	}

	if (!kk)
	{
		m_sLastError.Format("KROW::FormDelete({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}
	
	kDebugLog (3, "KROW:FormDelete: after: {}", sSQL);

	return (true);

} // FormDelete

//-----------------------------------------------------------------------------
bool KROW::AddCol (KStringView sColName, const KJSON& Value, KCOL::Flags iFlags, KCOL::Len iMaxLen)
//-----------------------------------------------------------------------------
{
	if (Value.is_object() || Value.is_array())
	{
		return AddCol(sColName, Value.dump(-1), iFlags ? iFlags : static_cast<uint16_t>(JSON), iMaxLen);
	}
	else if (Value.is_string())
	{
		return AddCol(sColName, Value.get<KJSON::string_t>(), iFlags, iMaxLen);
	}
	else if (Value.is_number())
	{
		if (Value.is_number_float())
		{
			return AddCol(sColName, Value.get<KJSON::number_float_t>(), iFlags ? iFlags : static_cast<uint16_t>(NUMERIC), iMaxLen);
		}
		else
		{
			return AddCol(sColName, Value.get<KJSON::number_integer_t>(), iFlags ? iFlags : static_cast<uint16_t>(NUMERIC), iMaxLen);
		}
	}
	else if (Value.is_boolean())
	{
		return AddCol(sColName, Value.get<KJSON::boolean_t>(), iFlags ? iFlags : static_cast<uint16_t>(BOOLEAN), iMaxLen);
	}
	else if (Value.is_null())
	{
		return AddCol(sColName, "", iFlags ? iFlags : static_cast<uint16_t>(JSON), iMaxLen);
	}
	else
	{
		kDebugLog(2, "KROW: could not identify JSON type for {}", sColName);
		return false;
	}
}

//-----------------------------------------------------------------------------
KJSON KROW::to_json () const
//-----------------------------------------------------------------------------
{
	KJSON json;

	for (auto& col : *this)
	{
		if (col.second.IsFlag(NONCOLUMN))
		{
			continue;
		}
		else if (col.second.IsFlag(INT64NUMERIC))
		{
			// large integers > 53 bits have no representation in JSON and need to
			// be stored as string values..
			json[col.first] = col.second.sValue;
		}
		else if (col.second.IsFlag(NUMERIC))
		{
			if (col.second.sValue.Contains('.'))
			{
				json[col.first] = col.second.sValue.Double();
			}
			else
			{
				json[col.first] = col.second.sValue.Int64();
			}
		}
		else if (col.second.IsFlag(BOOLEAN))
		{
			json[col.first] = col.second.sValue.Bool();
		}
		#if 0
		else if (/*(col.second.iFlags & KROW::NULL_IS_NOT_NIL) &&*/ col.second.sValue.empty())
		{
			json[col.first] = NULL;
		}
		#endif
		else if (col.second.IsFlag(JSON))
		{
			// this is a json serialization
			DEKAF2_TRY
			{
				KJSON object;
				kjson::Parse(object, col.second.sValue);
				json[col.first] = object;
			}
			DEKAF2_CATCH(const KJSON::exception& exc)
			{
				// not a valid json object / array, store it as a string
				json[col.first] = col.second.sValue;
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
					kjson::Parse(object, col.second.sValue);
					json[col.first] = object;
				}
				DEKAF2_CATCH(const KJSON::exception& exc)
				{
					// not a valid json object / array, store it as a string
					json[col.first] = col.second.sValue;
				}
			}
			else
			{
				json[col.first] = col.second.sValue;
			}
		}
	}

	return json;

} // json


//-----------------------------------------------------------------------------
KROW& KROW::operator+=(const KJSON& json)
//-----------------------------------------------------------------------------
{
	for (auto& it : json.items())
	{
		AddCol(it.key(), it.value());
	}
	return *this;

} // operator+=(KJSON)

