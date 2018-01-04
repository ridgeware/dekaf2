/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kjson.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KJSON::parse (KStringView sJSON)
//-----------------------------------------------------------------------------
{
	try {
		base_type::clear();
		m_obj = base_type::parse(sJSON);
		return true;
	}
	catch (const base_type::exception exc)
	{
		return (FormError (exc));
	}

} // parse

//-----------------------------------------------------------------------------
KString KJSON::GetString (KString sKey)
//-----------------------------------------------------------------------------
{
    std::string sReturnMe;

    m_sLastError.clear();

    try {
        sReturnMe = m_obj[sKey.c_str()];
    }
    catch (const KJSON::base_type::exception exc) {
        FormError(exc);
    }

	return (sReturnMe);

} // KJSON::GetString

//-----------------------------------------------------------------------------
KJSON KJSON::GetObject (KString sKey)
//-----------------------------------------------------------------------------
{
    base_type oReturnMe;

    m_sLastError.clear();

    try {
        oReturnMe = m_obj[sKey.c_str()];
		return (oReturnMe);
    }
    catch (const KJSON::base_type::exception exc) {
        FormError(exc);
        return (nullptr);
    }


} // KJSON::GetObject

//-----------------------------------------------------------------------------
bool KJSON::FormError (const base_type::exception exc)
//-----------------------------------------------------------------------------
{
	KString sLastError;
	sLastError.Printf ("JSON[%03d]: %s", exc.id, exc.what());

	return (false);

} // FormError

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KString sString)
//-----------------------------------------------------------------------------
{
	sString.Replace("\r","\\r", /*all=*/true);
	sString.Replace("\n","\\n", /*all=*/true);
	sString.Replace("\"","\\\"", /*all=*/true);

	KString sReturnMe;
	sReturnMe.Format ("\"{}\"", sString);

	return (sReturnMe);

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KString sName, KString sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	sName.Replace("\r","\\r", /*all=*/true);
	sName.Replace("\n","\\n", /*all=*/true);
	sName.Replace("\"","\\\"", /*all=*/true);

	sValue.Replace("\r","\\r", /*all=*/true);
	sValue.Replace("\n","\\n", /*all=*/true);
	sValue.Replace("\"","\\\"", /*all=*/true);

	KString sReturnMe;
	sReturnMe.Format ("{}\"{}\": \"{}\"{}", sPrefix, sName, sValue, sSuffix);

	return (sReturnMe);

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KString sName, int iValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	sName.Replace("\r","\\r", /*all=*/true);
	sName.Replace("\n","\\n", /*all=*/true);
	sName.Replace("\"","\\\"", /*all=*/true);

	KString sReturnMe;
	sReturnMe.Format ("{}\"{}\": {}{}", sPrefix, sName, iValue, sSuffix);

	return (sReturnMe);

} // KSJON::EscWrap

} // end of namespace dekaf2
