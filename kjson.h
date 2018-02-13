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

#pragma once

#include <nlohmann/json.hpp>
#include "kstring.h"
#include "kstringview.h"

namespace nlohmann {

inline void to_json(json& j, const ::dekaf2::KString& s)
{
	j = nlohmann::json{s.ToStdString()};
}

inline void from_json(const json& j, ::dekaf2::KString& s)
{
	s = j.get<std::string>();
}

inline void to_json(json& j, const ::dekaf2::KStringView& s)
{
	std::string s1(s.data(), s.size());
	j = nlohmann::json{s1};
}

inline void from_json(const json& j, ::dekaf2::KStringView& s)
{
	s = j.get<std::string>();
}

}

namespace dekaf2 {

using LJSON = nlohmann::json;

//using LJSON = nlohmann::basic_json<std::map, std::vector, KString >;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJSON : public LJSON
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------

	using string_t        = LJSON::string_t;
	using value_type      = LJSON::value_type;
	using reference       = LJSON::reference;
	using const_reference = LJSON::const_reference;

	template<class T>
	KJSON(initializer_list_t init,
			bool type_deduction = true,
			value_t manual_type = value_t::array
)
	: LJSON(init, type_deduction, manual_type)
	{
	}

	template<class...Args>
	KJSON(Args&&...args)
	: LJSON(std::forward<Args>(args)...)
	{
	}
/*
	KJSON& operator=(const KJSON& other)
	{
		LJSON::operator=(other);
		m_sLastError = other.m_sLastError;
		return *this;
	}

	KJSON& operator=(KJSON&& other)
	{
		LJSON::operator=(std::move(other));
		m_sLastError = std::move(other.m_sLastError);
		return *this;
	}
*/
	bool        Parse     (KStringView sJSON);
	string_t    GetString (const KString& sKey);
	KJSON       GetObject (const KString& sKey);
	const KString& GetLastError () { return m_sLastError; }

	bool Exists(const KString& Key) const
	{
		return LJSON::find(Key) != LJSON::end();
	}

	bool IsObject(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_object());
	}

	bool IsArray(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_array());
	}

	bool IsString(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_string());
	}

	bool IsInteger(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_number_integer());
	}

	bool IsFloat(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_number_float());
	}

	bool IsNull(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_null());
	}

	bool IsBoolean(const KString& Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_boolean());
	}

	/// This overload of the operator[] simply calls the existing operator[] of LJSON.
	/// The only additional protection it gives is to not throw when being called on
	/// non-objects.
	template<typename T>
	reference operator[](T* Key)
	{
		ClearError();

		try
		{
			return LJSON::operator[](Key);
		}
		catch (const LJSON::exception& exc)
		{
			FormError(exc);
		}

		static value_type s_empty;

		return s_empty;
	}

	/// We do not want this overload of the operator[] as it would abort on nonexisting keys
	template<typename T>
	const_reference operator[](T* Key) const
	{
		static_assert(sizeof(Key) == 0, "this version of operator[] is intentionally blocked");
		static value_type s_empty;
		return s_empty;
	}

	reference operator[](const KString& Key)
	{
		return operator[](Key.c_str());
	}

/*
	const LJSON& Object   () const { return m_obj; }
	LJSON&      Object    () { return m_obj; }
*/

	bool        FormError (const LJSON::exception& exc) const;

	/// wrap the given string with double-quotes and escape it for legal json
	static KString EscWrap (KString sString);
	static KString EscWrap (KString sName, KString sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

	/// do not wrap the given string with double-quotes if it is explicitly known to be Numeric
	static KString EscWrap (KString sName, int iNumber, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	static KString EscWrapNumeric (KString sString, int iNumber, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	static KString EscWrapNumeric (KString sName, KString sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

//----------
private:
//----------
	void        ClearError() const { m_sLastError.clear(); }

	mutable KString m_sLastError;

}; // KJSON

} // end of namespace dekaf2
