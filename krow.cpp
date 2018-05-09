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
#include "kjson.h"

using namespace dekaf2;

constexpr KStringView ESCAPE_MYSQL { "\'\\" };
constexpr KStringView ESCAPE_MSSQL { "\'"   };

int32_t  detail::KCommonSQLBase::m_iDebugLevel{2};

//-----------------------------------------------------------------------------
KString KROW::ColumnInfoForLogOutput (uint32_t ii)
//-----------------------------------------------------------------------------
{
	KString sLogMessage;

	sLogMessage.Format("  col[{:>02}]: {:<25} {}{}{}{}{}",
		    ii,
		    GetName(ii),
		    !IsFlag (ii, PKEY)       ? "" : " [PKEY]",
		    !IsFlag (ii, NONCOLUMN)  ? "" : " [NONCOLUMN]",
		    !IsFlag (ii, EXPRESSION) ? "" : " [EXPRESSION]",
		    !IsFlag (ii, NUMERIC)    ? "" : " [NUMERIC]",
		    !IsFlag (ii, BOOLEAN)    ? "" : " [BOOLEAN]");

	return sLogMessage;
}

//-----------------------------------------------------------------------------
void KROW::EscapeChars (KStringView sString, KString& sEscaped, SQLTYPE iDBType)
//-----------------------------------------------------------------------------
{
	KStringView sCharsToEscape;

	switch (iDBType)
	{
	case DBT_SQLSERVER:
	case DBT_SYBASE:
		sCharsToEscape = ESCAPE_MSSQL;
		break;
	case DBT_MYSQL:
	default:
		sCharsToEscape = ESCAPE_MYSQL;
		break;
	}

	EscapeChars (sString, sEscaped, sCharsToEscape);

} // EscapeChars - v1

//-----------------------------------------------------------------------------
void KROW::EscapeChars (KStringView sString, KString& sEscaped,
                        KStringView sCharsToEscape, KString::value_type iEscapeChar/*=0*/)
//-----------------------------------------------------------------------------
{
	// Note: if iEscapeChar is ZERO, then the char is used as it's own escape char (i.e. it gets doubled up).
	for (auto ch : sString)
	{
		if (sCharsToEscape.find(ch) != KStringView::npos)
		{
			if (iEscapeChar)
			{
				sEscaped += iEscapeChar;
			}
			else
			{
				sEscaped += ch;
			}
		}
		sEscaped += ch;
	}

} // EscapeChars

//-----------------------------------------------------------------------------
void KROW::SmartClip (KStringView sColName, KString& sValue, size_t iMaxLen)
//-----------------------------------------------------------------------------
{
	if (iMaxLen)
	{
		size_t iLen = sValue.length();
		if (iLen > iMaxLen)
		{
			kDebugLog (1, "KSQL: clipping {}='{:.10}...' to {} chars", sColName, sValue, iMaxLen);

			char cClipped = sValue[iMaxLen];
			// watch out for a trailing escape:
			if ((cClipped == '\'') || (cClipped == '\"')) {
				sValue.resize(iMaxLen-1);
			}
			else {
				sValue.resize(iMaxLen);
			}
		}
	}
} // SmartClip

//-----------------------------------------------------------------------------
bool KROW::FormInsert (KString& sSQL, SQLTYPE iDBType, bool fIdentityInsert/*=false*/)
//-----------------------------------------------------------------------------
{
	m_sLastError = ""; // reset

	kDebugLog (3, "KROW:FormInsert: before: {}", sSQL);
	
	if (!size()) {
		m_sLastError.Format("KROW::FormInsert(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty()) {
		m_sLastError.Format("KROW::FormInsert(): no tablename defined.");

		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	uint32_t ii;

	KString sAdd;
	sAdd.Format("insert into {} (\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormInsert: {}", GetTablename());

	for (ii=0; ii < size(); ++ii)
	{
		kDebugLog (3, ColumnInfoForLogOutput(ii));

		if (IsFlag (ii, NONCOLUMN)) {
			continue;
		}
		
		sAdd.Format ("\t{}{}\n", (ii) ? "," : "", GetName(ii));
		sSQL += sAdd;
	}

	sSQL += ") values (\n";

	for (ii=0; ii < size(); ++ii)
	{
		if (IsFlag (ii, NONCOLUMN)) {
			continue;
		}

		KStringView sValue   = GetValue(ii);  // note: GetValue() never returns NULL, it might return '' (which Joe calls NIL)

		if (sValue.empty() && !IsFlag (ii, NULL_IS_NOT_NIL)) {
			// Note: this is the default handling for NIL values: to place them in SQL as SQL null
			sAdd.Format ("\t{}null\n", (ii) ? "," : "");
			sSQL += sAdd;
		}
		else if (IsFlag (ii, NUMERIC) || IsFlag (ii, EXPRESSION) || IsFlag (ii, BOOLEAN))
		{
			sAdd.Format ("\t{}{}\n", (ii) ? "," : "", sValue); // raw value, no quotes and no processing
			sSQL += sAdd;
		}
		else
		{
			// catch-all logic for all string values
			// Note: if the value is actually NIL ('') and NULL_IS_NOT_NIL is set, then the value will
			// be placed into SQL as '' instead of SQL null.
			KString sEscaped;
			EscapeChars (sValue, sEscaped, iDBType);
			SmartClip   (GetName(ii), sEscaped, MaxLength(ii));
			sAdd.Format ("\t{}'{}'\n", (ii) ? "," : "", sEscaped);
			sSQL += sAdd;
		}

	}

	sSQL += ")";
	
	if(fIdentityInsert) {
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
bool KROW::FormUpdate (KString& sSQL, SQLTYPE iDBType)
//-----------------------------------------------------------------------------
{
	m_sLastError = ""; // reset
	
	if (!size()) {
		m_sLastError.Format("KROW::FormUpdate(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty()) {
		m_sLastError.Format("KROW::FormUpdate(): no tablename defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	KROW Keys;

	KString sAdd;
	sAdd.Format ("update {} set\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormUpdate: {}", m_sTablename);

	for (uint32_t ii=0, jj=0; ii < size(); ++ii)
	{
		kDebugLog (3, ColumnInfoForLogOutput(ii));

		if (IsFlag (ii, NONCOLUMN)) {
			continue;
		}
		else if (IsFlag (ii, PKEY)) {
			KCOL col (GetValue(ii), GetFlags(ii), MaxLength(ii));
			Keys.Add (GetName(ii), col);
		}
		else if (IsFlag (ii, EXPRESSION) || IsFlag (ii, BOOLEAN)) {
			sAdd.Format ("\t{}{}={}\n", (jj++) ? "," : "", GetName(ii), GetValue(ii));
			sSQL += sAdd;
		}
		else
		{
			KStringView sValue   = GetValue(ii);

			if (sValue.empty()) {
				sAdd.Format ("\t{}{}=null\n", (jj++) ? "," : "", GetName(ii));
				sSQL += sAdd;
			}
			else {
				KString sEscaped;
				EscapeChars (sValue, sEscaped, iDBType);
				SmartClip   (GetName(ii), sEscaped, MaxLength(ii));

				if (IsFlag (ii, NUMERIC) || IsFlag (ii, EXPRESSION) || IsFlag (ii, BOOLEAN))
				{
					sAdd.Format ("\t{}{}={}\n", (jj++) ? "," : "",
						GetName(ii), sEscaped);
				}
				else {
					sAdd.Format ("\t{}{}='{}'\n", (jj++) ? "," : "", GetName(ii), sEscaped);
				}
				sSQL += sAdd;
			}
		}
	}

	kDebugLog (GetDebugLevel()+1, "KROW::FormUpdate: update will rely on {} keys", Keys.size());
	//Keys.DebugPairs (0, "FormUpdate: primary keys:");

	if (Keys.size() == 0) {
		m_sLastError.Format("KROW::FormUpdate({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, "{}", m_sLastError);
		//DebugPairs (1);
		return (false);
	}

	for (uint32_t kk=0; kk < Keys.size(); ++kk)
	{
		KStringView sValue = Keys.GetValue(kk);
		KString sEscaped;
		EscapeChars (sValue, sEscaped, iDBType);
		
		KString sPrefix;
		if (!kk) {
			sPrefix = " where ";
		}
		else {
			sPrefix = "   and ";
		}
		
		if (Keys.IsFlag(kk, NUMERIC) || Keys.IsFlag(kk, EXPRESSION) || Keys.IsFlag(kk, BOOLEAN)) {
			sAdd.Format("{}{}={}\n", sPrefix, Keys.GetName(kk),
				sEscaped);
		}
		else {
			sAdd.Format("{}{}='{}'\n", sPrefix, Keys.GetName(kk), sEscaped);
		}
		sSQL += sAdd;
	}

	return (true);

} // FormUpdate

//-----------------------------------------------------------------------------
bool KROW::FormDelete (KString& sSQL, SQLTYPE iDBType)
//-----------------------------------------------------------------------------
{
	m_sLastError = ""; // reset

	kDebugLog (3, "KROW:FormDelete: before: {}", sSQL);

	if (!size()) {
		m_sLastError.Format("KROW::FormDelete(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	if (m_sTablename.empty()) {
		m_sLastError.Format("KROW::FormDelete(): no tablename defined.");
		kDebugLog (1, "{}", m_sLastError);
		return (false);
	}

	uint32_t   kk   = 0;

	KString sAdd;
	sAdd.Format("delete from {}\n", GetTablename());
	sSQL += sAdd;

	kDebugLog (3, "KROW:FormDelete: {}", m_sTablename);

	for (uint32_t ii=0; ii < size(); ++ii)
	{
		kDebugLog (3, ColumnInfoForLogOutput(ii));

		if (!IsFlag (ii, PKEY)) {
			continue;
		}

		KStringView sValue = GetValue(ii);
		KString     sEscaped;
		EscapeChars (sValue, sEscaped, iDBType);

		KString sAdd;
		SmartClip(GetName(ii),sEscaped,MaxLength(ii));
		if (sValue.empty())
		{
			sAdd.Format(" {} {} is null\n",(!kk) ? "where" : "  and", GetName(ii));
		}
		else if (IsFlag(ii, NUMERIC) || IsFlag(ii, EXPRESSION) || IsFlag(ii, BOOLEAN))
		{
			sAdd.Format(" {} {}={}\n",     (!kk) ? "where" : "  and", GetName(ii),
				sEscaped);
		}
		else
		{
			sAdd.Format(" {} {}='{}'\n", (!kk) ? "where" : "  and", GetName(ii), sEscaped);
		}
		sSQL += sAdd;
		

		++kk;
	}

	if (!kk) {
		m_sLastError.Format("KROW::FormDelete({}): no primary key[s] defined in column list", GetTablename());
		kDebugLog (1, "{}", m_sLastError);
		//DebugPairs (1);
		return (false);
	}
	
	kDebugLog (3, "KROW:FormDelete: after: {}", sSQL);

	return (true);

} // FormDelete

//-----------------------------------------------------------------------------
KString KROW::ToJSON (uint8_t iIndent/*=0*/, bool bWrapInCurlies/*=true*/)
//-----------------------------------------------------------------------------
{
	KString sJSON;
	KString sIndent;
	while (iIndent--) {
		sIndent += "\t";
	}

	m_sLastError = ""; // reset

	kDebugLog (3, "KROW:ToJSON: before: {}", sJSON);
	
	if (!size())
	{
		m_sLastError.Format("KROW::ToJSON(): no columns defined.");
		kDebugLog (1, "{}", m_sLastError);
		return sJSON;
	}

	if (bWrapInCurlies)
	{
		sJSON += sIndent;
		sJSON += "{\n";
	}

	kDebugLog (3, "KROW:ToJSON: {}", GetTablename());

	// variables used within the loop
	KStringView sName;
	KStringView sValue;
	bool        bFirst = true;

	for (uint32_t ii=0; ii < size(); ++ii)
	{
		kDebugLog (3, ColumnInfoForLogOutput(ii));

		if (IsFlag (ii, NONCOLUMN)) {
			continue;
		}

		sName    = GetName(ii);
		sValue   = GetValue(ii);  // note: GetValue() never returns NULL, it might return '' (which Joe calls NIL)

		if (sName.empty()) {
			continue;
		}

		if (!bFirst) {
			sJSON += ",\n";
		}

		sJSON += sIndent;
		if (sValue.empty() && !IsFlag (ii, NULL_IS_NOT_NIL))
		{
			sJSON += KJSON::EscWrapNumeric (sName, "null", bWrapInCurlies ? "\t" : "", "");
		}
		else if (IsFlag (ii, BOOLEAN))
		{
			// NOTE: Boolean values should be output as the word true and false without quotes around them.
			KStringView sTrueFalse = ((sValue == "0") || (sValue == "false") || (sValue == "FALSE")) ? "false" : "true";
			sJSON += KJSON::EscWrapNumeric (sName, sTrueFalse, bWrapInCurlies ? "\t" : "", "");
		}
		else if (IsFlag (ii, NUMERIC) || IsFlag (ii, EXPRESSION) || IsFlag(ii, BOOLEAN))
		{
			sJSON += KJSON::EscWrapNumeric (sName, sValue, bWrapInCurlies ? "\t" : "", "");
		}
		else // catch-all logic for all string values
		{
			sJSON += KJSON::EscWrap (sName, sValue, bWrapInCurlies ? "\t" : "", "");
		}
		bFirst = false;
	}

	// NOTE: If we are not wrapping the entire JSON object in curlies, we leave off the newline
	//       because the caller may need to append this object with another krow JSON object, or add
	//       more fields like array fields, so the caller may need to add a comma before newline
	if (bWrapInCurlies)
	{
		sJSON += "\n";
		sJSON += sIndent;
		sJSON += "}\n";
	}
	
	kDebugLog (3, "KROW:ToJSON: after: {}", sJSON);
	
	return sJSON;

} // ToJSON
