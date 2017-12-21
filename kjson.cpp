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
		base_type::parse(sJSON);
		return true;
	}
	catch (const base_type::exception exc)
	{
        return (FormError (exc));
	}

} // parse

//-----------------------------------------------------------------------------
bool KJSON::FormError (const base_type::exception exc)
//-----------------------------------------------------------------------------
{
    KString sLastError;
    sLastError.Printf ("JSON[%03d]: %s", exc.id, exc.what());

    return (false);

} // FormError

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KStringView sString)
//-----------------------------------------------------------------------------
{
	base_type j{sString};
	return j.dump();

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KStringView sName, KStringView sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	base_type j{sName, sValue};
	KString sRet{sPrefix};
	sRet += j.dump();
	sRet += sSuffix;
	return sRet;

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KStringView sName, int iValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	base_type j{sName, iValue};
	KString sRet{sPrefix};
	sRet += j.dump();
	sRet += sSuffix;
	return sRet;

} // KSJON::EscWrap

} // end of namespace dekaf2
