/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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

#include "kjson2.h"
#include "../klog.h"
#include "../kexception.h"
#include "../krow.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KString KJSON2::Proxy::Dump(bool bPretty) const
//-----------------------------------------------------------------------------
{
	return m_json->dump(bPretty ? 1 : -1, '\t');
}

//-----------------------------------------------------------------------------
bool KJSON2::Proxy::Parse(KStringView sJson, bool bThrow)
//-----------------------------------------------------------------------------
{
	if (bThrow)
	{
		*m_json = kjson::ParseOrThrow(sJson);
	}
	else
	{
		*m_json = kjson::Parse(sJson);
	}

	return !m_json->is_null();
}

namespace kjson2 {
const KString s_sEmptyString;
const KJSON   s_oEmptyObject = KJSON::object();
const KJSON   s_oEmptyArray  = KJSON::array();
}

//-----------------------------------------------------------------------------
KJSON2::Proxy KJSON2::Proxy::Object() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_object())
	{
		return *m_json;
	}

	kDebug(2, "not a json object, returning empty object");

	return kjson2::s_oEmptyObject;

} // Object

//-----------------------------------------------------------------------------
KJSON2::Proxy KJSON2::Proxy::Array() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_array())
	{
		return *m_json;
	}

	kDebug(2, "not a json array, returning empty array");

	return kjson2::s_oEmptyArray;

} // Array

//-----------------------------------------------------------------------------
const KString& KJSON2::Proxy::String() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_string())
	{
		return m_json->get_ref<const KString&>();
	}

	kDebug(2, "not a json string, returning empty string");

	return kjson2::s_sEmptyString;

} // String

//-----------------------------------------------------------------------------
uint64_t KJSON2::Proxy::UInt64() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_number_integer())
	{
		return m_json->get<uint64_t>();
	}

	kDebug(2, "not a json unsigned, returning 0");

	return 0;

} // UInt64

//-----------------------------------------------------------------------------
int64_t KJSON2::Proxy::Int64() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_number_integer())
	{
		{
			return m_json->get<int64_t>();
		}
	}

	kDebug(2, "not a json signed, returning 0");

	return 0;

} // Int64

//-----------------------------------------------------------------------------
bool KJSON2::Proxy::Bool() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_boolean())
	{
		return m_json->get<bool>();
	}

	kDebug(2, "not a json bool, returning false");

	return false;

} // Bool

//-----------------------------------------------------------------------------
double KJSON2::Proxy::Float() const noexcept
//-----------------------------------------------------------------------------
{
	if (m_json->is_number_float())
	{
		return m_json->get<double>();
	}

	kDebug(2, "not a json float, returning 0.0");

	return 0.0;

} // Float


// --> KJSON2 <--

//-----------------------------------------------------------------------------
KJSON2::KJSON2(const Proxy& proxy)
//-----------------------------------------------------------------------------
: KJSON2(proxy.ToKJSON())
{
}

//-----------------------------------------------------------------------------
KJSON2::KJSON2(const KROW& row)
//-----------------------------------------------------------------------------
{
	to_json(m_json, row);
}

//-----------------------------------------------------------------------------
bool KJSON2::Exists(KStringView sSelector) const
//-----------------------------------------------------------------------------
{
	return !Select(sSelector).is_null();
}

//-----------------------------------------------------------------------------
bool KJSON2::Contains(KStringView sSelector) const
//-----------------------------------------------------------------------------
{
	return false;
}

} // end of namespace dekaf2
