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

#if !DEKAF2_KJSON2_IS_DISABLED

#ifdef DEKAF2
	#include "klog.h"
	#include "kstringutils.h"
#else
	#include <cctype>
#endif

#include <cstdlib>
#include <cerrno>

namespace DEKAF2_KJSON_NAMESPACE {

namespace config {

// configuration points

//-----------------------------------------------------------------------------
const char* StripJSONExceptionMessage(const char* sMessage) noexcept
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	return kjson::kStripJSONExceptionMessage(sMessage);
#else
	return sMessage;
#endif
}

//-----------------------------------------------------------------------------
void TrimLeft(KJSON2::StringViewT& sView) noexcept
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	kTrimLeft(sView);
#else
	while (!sView.empty()) { if (std::isspace(sView.front())) { sView.remove_prefix(1); } else break; }
#endif
}

//-----------------------------------------------------------------------------
bool SkipLeadingSpace(KJSON2::IStreamT& InStream) noexcept
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	return kjson::SkipLeadingSpace(InStream);
#else
	// ultimately we should implement a logic like in kjson::SkipLeadingSpace()
	// here, but it is not mission critical, as json is expected to start without
	// leading spaces
	return true;
#endif
}

//-----------------------------------------------------------------------------
bool IsUnsigned(const KJSON2::StringT& sUnsigned) noexcept
//-----------------------------------------------------------------------------
{
	return std::all_of(sUnsigned.begin(), sUnsigned.end(), [](KJSON2::StringT::value_type ch)
	{
#ifdef DEKAF2
		return KASCII::kIsDigit(ch);
#else
		return std::isdigit(ch);
#endif
	});
}

//-----------------------------------------------------------------------------
bool ToInteger(const KJSON2::StringT& sInteger, uint64_t& Unsigned) noexcept
//-----------------------------------------------------------------------------
{
	if (sInteger.empty()) return false;
	KJSON2::string_t::pointer pos;
	Unsigned = std::strtoull(sInteger.c_str(), &pos, 10);
	// we DO want to return ULLONG_MAX in case of an overflow - hence we return true on errno == ERANGE
	return (errno == ERANGE) || static_cast<KJSON2::StringT::size_type>(pos - sInteger.c_str()) == sInteger.size();
}

//-----------------------------------------------------------------------------
bool ToInteger(const KJSON2::StringT& sInteger, int64_t& Signed) noexcept
//-----------------------------------------------------------------------------
{
	if (sInteger.empty()) return false;
	KJSON2::string_t::pointer pos;
	Signed = std::strtoll(sInteger.c_str(), &pos, 10);
	// we DO want to return LLONG_MAX/LLONG_MIN in case of an overflow - hence we return true on errno == ERANGE
	return (errno == ERANGE) || static_cast<KJSON2::StringT::size_type>(pos - sInteger.c_str()) == sInteger.size();
}

//-----------------------------------------------------------------------------
bool ToDouble(const KJSON2::StringT& sFloat, double& Double) noexcept
//-----------------------------------------------------------------------------
{
	if (sFloat.empty()) return false;
	KJSON2::string_t::pointer pos;
	Double = std::strtod(sFloat.c_str(), &pos);
	// we DO want to return HUGE_VAL in case of an overflow - hence we return true on errno == ERANGE
	return (errno == ERANGE) || static_cast<KJSON2::StringT::size_type>(pos - sFloat.c_str()) == sFloat.size();
}

//-----------------------------------------------------------------------------
bool ToBool(const KJSON2::StringT& sBool) noexcept
//-----------------------------------------------------------------------------
{
	int64_t iInteger;
	if (ToInteger(sBool, iInteger)) return iInteger != 0;
	return (sBool == "true" || sBool == "yes" || sBool == "TRUE" || sBool == "YES");
}

//-----------------------------------------------------------------------------
KJSON2::StringT ToString(bool val)
//-----------------------------------------------------------------------------
{
	return val ? "true" : "false";
}

//-----------------------------------------------------------------------------
template<typename T>
KJSON2::StringT ToString(T val)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	return KString::to_string(val);
#else
	return std::to_string(val);
#endif
}

//-----------------------------------------------------------------------------
KJSON2::StringViewT::value_type GetFront(KJSON2::StringViewT sString)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	return sString.front();
#else
	return !sString.empty() ? sString.front() : 0;
#endif
}

//-----------------------------------------------------------------------------
bool RemovePrefix (KJSON2::StringViewT& sString, const KJSON2::StringViewT::value_type ch)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2
	return sString.remove_prefix(ch);
#else
	if (GetFront(sString) == ch)
	{
		sString.remove_prefix(1);
		return true;
	}
	return false;
#endif
}

} // end of namespace config

const KJSON2::StringT KJSON2::s_sEmpty;
const KJSON2          KJSON2::s_oEmpty;

//-----------------------------------------------------------------------------
KJSON2 KJSON2::diff(const KJSON2& left, const KJSON2& right)
//-----------------------------------------------------------------------------
{
	return base::diff(left.ToBase(), right.ToBase());
}

//-----------------------------------------------------------------------------
KJSON2::StringT KJSON2::dump(int iIndent, char chIndent, bool bEnsureASCII, const KJSON2::base::error_handler_t error_handler) const
//-----------------------------------------------------------------------------
{
	return base::dump(iIndent, chIndent, bEnsureASCII, error_handler);
}

//-----------------------------------------------------------------------------
void KJSON2::Serialize (OStreamT& ostream, bool bPretty) const
//-----------------------------------------------------------------------------
{
	if (!bPretty)
	{
#ifdef DEKAF2
		ostream.ostream() << ToBase();
#else
		ostream << ToBase();
#endif
	}
	else
	{
#ifdef DEKAF2
		ostream.ostream() << std::setw(1) << std::setfill('\t') << ToBase();
#else
		ostream << std::setw(1) << std::setfill('\t') << ToBase();
#endif
	}

} // Serialize

//-----------------------------------------------------------------------------
bool KJSON2::Parse(StringViewT sJson, bool bThrow)
//-----------------------------------------------------------------------------
{
	// reset the json object
	*this = KJSON2{};

	config::TrimLeft(sJson);

	// avoid throwing an exception for empty input - JSON will simply
	// be empty too, so no error.

	if (!sJson.empty())
	{
		DEKAF2_TRY
		{
			*this = base::parse(sJson.cbegin(), sJson.cend());
		}

		DEKAF2_CATCH (const exception& ex)
		{
			kDebug(2, config::StripJSONExceptionMessage(ex.what()));

			if (bThrow)
			{
				throw ex;
			}

			*this = KJSON2{};
		}
	}

	return !is_null();

} // Parse

//-----------------------------------------------------------------------------
bool KJSON2::Parse(IStreamT& istream, bool bThrow)
//-----------------------------------------------------------------------------
{
	*this = KJSON2{};

	if (!config::SkipLeadingSpace(istream))
	{
		// empty input == empty json
		return true;
	}

	DEKAF2_TRY
	{
		istream >> ToBase();
		return true;
	}

	DEKAF2_CATCH (const exception& ex)
	{
		kDebug(2, config::StripJSONExceptionMessage(ex.what()));

		if (bThrow)
		{
			throw ex;
		}

		*this = KJSON2{};
	}

	return !is_null();

} // Parse

//-----------------------------------------------------------------------------
KJSON2::const_reference KJSON2::Object(const_reference Default) const noexcept
//-----------------------------------------------------------------------------
{
	if (is_object())
	{
		return *this;
	}

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

	return Default;

} // Array

//-----------------------------------------------------------------------------
const KJSON2::StringT& KJSON2::ConstString() const noexcept
//-----------------------------------------------------------------------------
{
	if (is_string())
	{
		return base::get_ref<const KJSON2::StringT&>();
	}

	return s_sEmpty;

} // ConstString

//-----------------------------------------------------------------------------
KJSON2::StringT& KJSON2::String(StringViewT sDefault)
//-----------------------------------------------------------------------------
{
	if (!is_string())
	{
		*this = Print(sDefault, false);
	}

	return base::get_ref<KJSON2::StringT&>();

} // String

//-----------------------------------------------------------------------------
KJSON2::StringT KJSON2::Print(StringViewT sDefault, bool bSerializeAll) const
//-----------------------------------------------------------------------------
{
	switch (type())
	{
		case value_t::string:
			return base::get<KJSON2::StringT>();

		case value_t::number_integer:
			return config::ToString(base::get<int64_t>());

		case value_t::number_unsigned:
			return config::ToString(base::get<uint64_t>());

		case value_t::number_float:
			return config::ToString(base::get<float>());

		case value_t::boolean:
			return config::ToString(base::get<bool>());

		// We only dump objects and arrays or output NULL for null
		// if bSerializeAll is true
		case value_t::object:
		case value_t::array:
		case value_t::binary:
		case value_t::discarded:
			if (bSerializeAll) return Serialize();
			break;

		case value_t::null:
			if (bSerializeAll) return "NULL";
			break;
	}

	return StringT(sDefault);

} // Print

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
		return base::get<double>();
	}
	else if (is_string())
	{
		auto& sInteger = base::get_ref<const KJSON2::StringT&>();

		uint64_t iInteger;

		if (config::ToInteger(sInteger, iInteger))
		{
			return iInteger;
		}
	}

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
		return base::get<double>();
	}
	else if (is_string())
	{
		auto& sInteger = base::get_ref<const KJSON2::StringT&>();

		int64_t iInteger;

		if (config::ToInteger(sInteger, iInteger))
		{
			return iInteger;
		}
	}

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
		return config::ToBool(base::get_ref<const KJSON2::StringT&>());
	}

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
		return base::get<int64_t>();
	}
	else if (is_string())
	{
		auto& sFloat = base::get_ref<const KJSON2::StringT&>();

		double Double;

		if (config::ToDouble(sFloat, Double))
		{
			return Double;
		}
	}

	return dDefault;

} // Float

//-----------------------------------------------------------------------------
KJSON2::const_reference	KJSON2::ConstElement(StringViewT sElement) const noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (DEKAF2_LIKELY(is_object()))
		{
			auto it = find(sElement);

			if (DEKAF2_LIKELY(it != end()))
			{
				return it.value();
			}
		}

		return s_oEmpty;
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(2, config::StripJSONExceptionMessage(exc.what()));
	}

	return s_oEmpty;

} // ConstElement

//-----------------------------------------------------------------------------
KJSON2::reference KJSON2::Element(StringViewT sElement) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		// direct element access
		if (DEKAF2_LIKELY(is_object()))
		{
			// need an object key here
			auto it = find(sElement);

			if (DEKAF2_LIKELY(it != end()))
			{
				return it.value();
			}
			else
			{
				// insert a new json null object here
				return MakeRef(ToBase()[sElement]);
			}
		}

		// this is an array, a primitive, or null..
		// make this an object
		*this = object();
		// and create a new null object
		return MakeRef(ToBase()[sElement]);
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(2, config::StripJSONExceptionMessage(exc.what()));
	}

	return *this;

} // Element

//-----------------------------------------------------------------------------
KJSON2::const_reference	KJSON2::ConstElement(size_type iElement) const noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (is_array())
		{
			return MakeRef(base::at(iElement));
		}
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(2, config::StripJSONExceptionMessage(exc.what()));
	}

	return s_oEmpty;

} // ConstElement

//-----------------------------------------------------------------------------
KJSON2::reference KJSON2::Element(size_type iElement) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (!is_array())
		{
			// make this an array
			*this = array();
		}
		// expand array if necessary
		return MakeRef(ToBase()[iElement]);
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(2, config::StripJSONExceptionMessage(exc.what()));
	}

	return *this;

} // Element

//-----------------------------------------------------------------------------
KJSON2::reference KJSON2::Append (KJSON2 other)
//-----------------------------------------------------------------------------
{
	// this operation will always succeed

	DEKAF2_TRY
	{
		if (is_null())
		{
			// make it an array
			*this = array();
		}
		else if (!is_array())
		{
			if (is_object() && other.is_object())
			{
				// merge both objects
				push_back(other);
				return *this;
			}

			// we have to make left an array to be able to append and
			// move the existing value into the first element of the (new) array
			KJSON2 copy = array();
			copy.base::push_back(std::move(*this));
			*this = std::move(copy);
		}

		base::push_back(std::move(other));
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(2, config::StripJSONExceptionMessage(exc.what()));
	}

	return *this;

} // Append

#ifdef DEKAF2
namespace {

//-----------------------------------------------------------------------------
KString ToJsonPointer(KStringView sSelector)
//-----------------------------------------------------------------------------
{
	// TODO make this a char loop
	// assumes we have a json path in sSelector
	KString sPointer;
	sPointer.reserve(sSelector.size());

	static const KFindSetOfChars Trim(" \f\n\r\t\v\b]");
	static const KFindSetOfChars Dots(".[");

	for (const auto& sElement : sSelector.Split(Dots, Trim))
	{
		if (!sElement.empty())
		{
			sPointer += '/';
			sPointer += sElement;
		}
	}

	return sPointer;

} // ToJsonPointer

} // end of anonymous namespace
#endif

//-----------------------------------------------------------------------------
KJSON2::const_reference KJSON2::RecurseJSONPointer(const_reference json, StringViewT sSelector, uint16_t iRecursions)
//-----------------------------------------------------------------------------
{
	// -> /segment/moreSegment/object/array/2/object/property
	// we are wrapped in a catch/try that will return an empty object on failure

	if (iRecursions > iMaxJSONPointerDepth)
	{
		kWarning("recursion depth > {}", iMaxJSONPointerDepth);
		return s_oEmpty;
	}

	if (DEKAF2_UNLIKELY(config::RemovePrefix(sSelector, '/') == false))
	{
		kDebug(2, "expected a leading '/', but none found");
	}

	// isolate the next segment
	auto iNext    = sSelector.find('/');
	auto sSegment = sSelector.substr(0, iNext);

	const KJSON2* next;

	if (json.is_object())
	{
		// need an object key here
		auto it = json.find(sSegment);

		if (DEKAF2_UNLIKELY(it == json.end()))
		{
			// abort traversal here - we are const and cannot add missing segments
			return s_oEmpty;
		}

		next = &it.value();
	}
	else if (json.is_array())
	{
		// need an array index here
		uint64_t iIndex;

		if (!config::ToInteger(sSegment, iIndex))
		{
			return s_oEmpty;
		}
		// if this will throw it will be caught in the caller and return an empty object
		next = MakePtr(&json.at(iIndex));
	}
	else
	{
		// further segment requested, but this is a primitive (or null)..
		return s_oEmpty;
	}

	if (iNext == KStringView::npos)
	{
		return *next;
	}

	sSelector.remove_prefix(iNext);

	return RecurseJSONPointer(*next, sSelector, ++iRecursions);

} // RecurseJSONPointer

//-----------------------------------------------------------------------------
KJSON2::const_reference& KJSON2::ConstSelect (StringViewT sSelector) const noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		switch (config::GetFront(sSelector))
		{
#ifdef DEKAF2
			case '.': // a dotted notation
				return RecurseJSONPointer(*this, ToJsonPointer(sSelector));
#endif
			case '/': // a json pointer
				return RecurseJSONPointer(*this, sSelector);

			default:
				return ConstElement(sSelector);
		}
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(1, config::StripJSONExceptionMessage(exc.what()));
	}

	return s_oEmpty;

} // ConstSelect

//-----------------------------------------------------------------------------
KJSON2::reference KJSON2::RecurseJSONPointer(reference json, StringViewT sSelector, uint16_t iRecursions)
//-----------------------------------------------------------------------------
{
	// -> /segment/moreSegment/object/array/2/object/property

	if (DEKAF2_UNLIKELY(iRecursions > iMaxJSONPointerDepth))
	{
		kWarning("recursion depth > {}", iMaxJSONPointerDepth);
		return json;
	}

	if (DEKAF2_UNLIKELY(config::RemovePrefix(sSelector, '/') == false))
	{
		kDebug(2, "expected a leading '/', but none found");
	}

	// isolate the next segment
	auto iNext      = sSelector.find('/');
	auto sSegment   = sSelector.substr(0, iNext);

	KJSON2* next;

	if (DEKAF2_LIKELY(json.is_object()))
	{
		// need an object key here
		auto it = json.find(sSegment);

		if (DEKAF2_LIKELY(it != json.end()))
		{
			next = &it.value();
		}
		else
		{
			// insert a new json null object here
			next = &json[sSegment];
		}
	}
	else if (json.is_array())
	{
		uint64_t iIndex;

		if (!config::ToInteger(sSegment, iIndex))
		{
			// make this an object
			json = LJSON::object();
			// and create a new null object
			next = &json[sSegment];
		}
		else
		{
			// expand array if necessary
			next = &json[iIndex];
		}
	}
	else
	{
		// further segment requested, but this is a primitive or null..
		uint64_t iIndex;

		if (!config::ToInteger(sSegment, iIndex))
		{
			// make this an object
			json = LJSON::object();
			// and create a new null object
			next = &json[sSegment];
		}
		else
		{
			// make this an array
			json = array();
			// and create a new null object
			next = &json[iIndex];
		}
	}

	if (iNext == KStringView::npos)
	{
		return *next;
	}

	sSelector.remove_prefix(iNext);

	return RecurseJSONPointer(*next, sSelector, ++iRecursions);

} // RecurseJSONPointer

//-----------------------------------------------------------------------------
KJSON2::reference KJSON2::Select (StringViewT sSelector) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		switch (config::GetFront(sSelector))
		{
#ifdef DEKAF2
			case '.': // a dotted notation
				return RecurseJSONPointer(*this, ToJsonPointer(sSelector));
#endif
			case '/': // a json pointer
				return RecurseJSONPointer(*this, sSelector);

			default:
				return Element(sSelector);
		}
	}

	DEKAF2_CATCH (const exception& exc)
	{
		kDebug(1, config::StripJSONExceptionMessage(exc.what()));
	}

	return *this;

} // Select

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr uint16_t KJSON2::iMaxJSONPointerDepth;
#endif

} // end of DEKAF2_KJSON_NAMESPACE

#endif // of !DEKAF2_KJSON2_IS_DISABLED
