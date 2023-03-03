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
#include "klog.h"
#include "kstringutils.h"

namespace dekaf2 {

const KString KJSON2::s_sEmpty;
const KJSON2  KJSON2::s_oEmpty;

//-----------------------------------------------------------------------------
KJSON2 KJSON2::diff(const KJSON2& left, const KJSON2& right)
//-----------------------------------------------------------------------------
{
	return base::diff(left.ToBase(), right.ToBase());
}

//-----------------------------------------------------------------------------
KString KJSON2::dump(int iIndent, char chIndent, bool bEnsureASCII, const LJSON::error_handler_t error_handler) const
//-----------------------------------------------------------------------------
{
	return base::dump(iIndent, chIndent, bEnsureASCII, error_handler);
}

//-----------------------------------------------------------------------------
bool KJSON2::Parse(KStringView sJson, bool bThrow)
//-----------------------------------------------------------------------------
{
	if (bThrow)
	{
		*this = kjson::ParseOrThrow(sJson);
	}
	else
	{
		*this = kjson::Parse(sJson);
	}

	return !is_null();
}

//-----------------------------------------------------------------------------
bool KJSON2::Parse(KInStream& istream, bool bThrow)
//-----------------------------------------------------------------------------
{
	if (bThrow)
	{
		*this = kjson::ParseOrThrow(istream);
	}
	else
	{
		*this = kjson::Parse(istream);
	}

	return !is_null();
}

//-----------------------------------------------------------------------------
KJSON2::const_reference KJSON2::Object(const_reference Default) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_object())
	{
		return *this;
	}

	kDebug(2, "not a json object");

	return Default;

} // Object

//-----------------------------------------------------------------------------
KJSON2::const_reference KJSON2::Array(const_reference Default) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_array())
	{
		return *this;
	}

	kDebug(2, "not a json array");

	return Default;

} // Array

//-----------------------------------------------------------------------------
const KString& KJSON2::StringRef() const noexcept
//-----------------------------------------------------------------------------
{
	if (is_string())
	{
		return base::get_ref<const KString&>();
	}

	kDebug(2, "not a json string");

	return s_sEmpty;

} // StringRef

//-----------------------------------------------------------------------------
KString& KJSON2::StringRef(KStringView sDefault)
//-----------------------------------------------------------------------------
{
	if (!is_string())
	{
		*this = String(sDefault);
	}

	return base::get_ref<KString&>();

} // StringRef

//-----------------------------------------------------------------------------
KString KJSON2::String(KStringView sDefault) const
//-----------------------------------------------------------------------------
{
	switch (type())
	{
		case value_t::string:
			return base::get<KString>();

		case value_t::number_integer:
			return KString::to_string(base::get<int64_t>());

		case value_t::number_unsigned:
			return KString::to_string(base::get<uint64_t>());

		case value_t::number_float:
			return KString::to_string(base::get<float>());

		case value_t::boolean:
			return base::get<bool>() ? "true" : "false";

		// We do not dump objects and arrays here, nor do we output NULL for null
		// - we just fall back to the default
		// To convert any element use the Print() member
		case value_t::null:
		case value_t::object:
		case value_t::array:
		case value_t::binary:
		case value_t::discarded:
			return sDefault;
	}

	return KString{};

} // String

//-----------------------------------------------------------------------------
uint64_t KJSON2::UInt64(uint64_t iDefault) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_number_integer())
	{
		return base::get<uint64_t>();
	}

	if (is_number_float())
	{
		kDebug(2, "converting unsigned from {} ({})", "float", base::get<double>());

		return base::get<double>();
	}
	else if (is_string())
	{
		kDebug(2, "converting unsigned from {} ({})", "string", base::get_ref<const KString&>());

		auto& sInteger = base::get_ref<const KString&>();

		if (kIsInteger(sInteger, false))
		{
			return sInteger.UInt64();
		}
	}

	kDebug(2, "not a json unsigned");

	return iDefault;

} // UInt64

//-----------------------------------------------------------------------------
int64_t KJSON2::Int64(int64_t iDefault) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_number_integer())
	{
		return base::get<int64_t>();
	}
	else if (is_number_float())
	{
		kDebug(2, "converting signed from {} ({})", "float", base::get<double>());

		return base::get<double>();
	}
	else if (is_string())
	{
		kDebug(2, "converting signed from {} ({})", "string", base::get_ref<const KString&>());

		auto& sInteger = base::get_ref<const KString&>();

		if (kIsInteger(sInteger, true))
		{
			return sInteger.Int64();
		}
	}

	kDebug(2, "not a json signed");

	return iDefault;

} // Int64

//-----------------------------------------------------------------------------
bool KJSON2::Bool(bool bDefault) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_boolean())
	{
		return base::get<bool>();
	}
	else if (is_number_integer())
	{
		return base::get<uint64_t>();
	}
	else if (is_string())
	{
		kDebug(2, "converting bool from {} ({})", "string", base::get_ref<const KString&>());

		return base::get_ref<const KString&>().Bool();
	}

	kDebug(2, "not a json bool");

	return false;

} // Bool

//-----------------------------------------------------------------------------
double KJSON2::Float(double dDefault) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_number_float())
	{
		return base::get<double>();
	}
	else if (is_number_integer())
	{
		kDebug(2, "converting float from integer ({})", base::get<int64_t>());
		return base::get<int64_t>();
	}
	else if (is_string())
	{
		auto& sFloat = base::get_ref<const KString&>();

		kDebug(2, "converting float from {} ({})", "string", sFloat);

		if (kIsFloat(sFloat))
		{
			return sFloat.Double();
		}
	}

	kDebug(2, "not a json float");

	return dDefault;

} // Float

} // end of namespace dekaf2
