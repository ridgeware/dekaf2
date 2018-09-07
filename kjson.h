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
#include "krow.h"
#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"

using LJSON = nlohmann::basic_json<std::map, std::vector, dekaf2::KString >;

namespace dekaf2 {

inline void to_json(LJSON& j, const dekaf2::KStringView& s)
{
	j = LJSON::string_t(s);
}

inline void to_json(LJSON& j, const dekaf2::KStringViewZ& s)
{
	j = LJSON::string_t(s);
}

inline void from_json(const LJSON& j, dekaf2::KStringViewZ& s)
{
	s = j.get<LJSON::string_t>();
}

inline void from_json(const LJSON& j, dekaf2::KStringView& s)
{
	s = j.get<LJSON::string_t>();
}

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

	KJSON() = default;

	KJSON(const LJSON& other)
	: LJSON(other)
	{
	}

	KJSON(LJSON&& other)
	: LJSON(std::move(other))
	{
	}

	using LJSON::LJSON;

	KJSON& operator=(const LJSON& other)
	{
		LJSON::operator=(other);
		m_sLastError.clear();
		return *this;
	}

	KJSON& operator=(LJSON&& other)
	{
		LJSON::operator=(std::move(other));
		m_sLastError.clear();
		return *this;
	}

	LJSON& ToLJSON()
	{
		return *this;
	}

	const LJSON& ToLJSON() const
	{
		return *this;
	}

	bool        Parse     (KStringView sJSON);

	bool        Parse     (KInStream& InStream);

	KJSON       GetObject (KStringView sKey) const;

	KString     GetString (KStringView sKey) const;

	const KString& GetLastError () { return m_sLastError; }

	bool Exists(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end());
	}

	bool IsObject(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_object());
	}

	bool IsArray(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_array());
	}

	bool IsString(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_string());
	}

	bool IsInteger(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_number_integer());
	}

	bool IsFloat(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_number_float());
	}

	bool IsNull(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_null());
	}

	bool IsBoolean(KStringView Key) const
	{
		auto it = LJSON::find(Key);
		return (it != LJSON::end() && it->is_boolean());
	}

	bool Add (const KROW& row);

	KJSON& operator+=(const KROW& row)
	{
		Add(row);
		return *this;
	}

	// make sure the above operator+= does not overwrite all parent operator+=
	using LJSON::operator+=;

#ifndef DEKAF2_EXCEPTIONS
	/// This overload of the operator[] simply calls the existing operator[] of LJSON.
	/// The only additional protection it gives is to not throw when being called on
	/// non-objects.
	template<typename T>
	reference operator[](T&& Key)
	{
		ClearError();

		DEKAF2_TRY
		{
			return LJSON::operator[](std::forward<T>(Key));
		}
		DEKAF2_CATCH (const LJSON::exception& exc)
		{
			FormError(exc);
		}

		return s_empty;
	}
#endif

//----------
private:
//----------

#ifndef DEKAF2_EXCEPTIONS
	/// We do not want this overload of the operator[] as it would abort on
	/// nonexisting keys in debug mode, and lead to undefined behaviour in
	/// release mode
	template<typename T>
	const_reference operator[](T&& Key) const
	{
		static_assert(sizeof(Key) == 0, "this version of operator[] is intentionally blocked");
		return s_empty;
	}
#endif

//----------
public:
//----------

	bool FormError (const LJSON::exception& exc) const;

	/// proper json string escaping
	static void Escape (KStringView sInput, KString& sOutput);
	static KString Escape (KStringView sInput);

	/// wrap the given string with double-quotes and escape it for legal json
	static KString EscWrap (KStringView sString);
	static KString EscWrap (KStringView sName, KStringView sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

	/// do not wrap the given string with double-quotes if it is explicitly known to be Numeric
	template<typename I, typename std::enable_if<!std::is_constructible<KStringView, I>::value, int>::type = 0>
	static KString EscWrap (KStringView sName, I iValue, KStringView sPrefix="\n\t", KStringView sSuffix=",")
	{
		return EscWrapNumeric(sName, KString::to_string(iValue), sPrefix, sSuffix);
	}
	template<typename I, typename std::enable_if<!std::is_constructible<KStringView, I>::value, int>::type = 0>
	static KString EscWrapNumeric (KStringView sName, I iValue, KStringView sPrefix="\n\t", KStringView sSuffix=",")
	{
		return EscWrap(sName, iValue, sPrefix, sSuffix);
	}
	static KString EscWrapNumeric (KStringView sName, KStringView sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

//----------
private:
//----------

	void ClearError() const { m_sLastError.clear(); }

	static value_type s_empty;

	mutable KString m_sLastError;

}; // KJSON

} // end of namespace dekaf2
