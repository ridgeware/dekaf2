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

#pragma once

#include "../kjson.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KJSON2
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	class Proxy
	{
		// allow access to the private constructor for KJSON2
		friend class KJSON2;

	public:

		using self = Proxy;

		               Proxy      ()              = delete;
		               Proxy      (const Proxy&)  = default;
		               Proxy      (Proxy&&)       = default;
		               Proxy      (KJSON&  json ) : m_json(&json        ) {}
		               Proxy      (KJSON2& json2) : m_json(&json2.m_json) {}

		Proxy&         operator=  (const Proxy&)  = default;
		Proxy&         operator=  (Proxy&&)       = default;
		template<class Value>
		Proxy&         operator=(Value&& value)
		{
			*m_json = std::forward<Value>(value);
			return *this;
		}

		bool           Parse      (KStringView sJson, bool bThrow = false);
		KString        Dump       (bool bPretty = false) const;
		KString        Serialize  (bool bPretty = false) const { return Dump(bPretty); }

		Proxy          Object     () const noexcept;
		Proxy          Array      () const noexcept;
		const KString& String     () const noexcept;
		KStringView    StringView () const noexcept { return String(); }
		uint64_t       UInt64     () const noexcept;
		int64_t        Int64      () const noexcept;
		bool           Bool       () const noexcept;
		double         Float      () const noexcept;

		operator const KString&   () const noexcept { return String(); }
		operator       uint64_t   () const noexcept { return UInt64(); }
		operator       int64_t    () const noexcept { return Int64();  }
		operator       bool       () const noexcept { return Bool();   }
		operator       double     () const noexcept { return Float();  }

		const Proxy    Select     (KStringView sSelector) const { return kjson::Select(GetConstJSONRef(), sSelector); }
		Proxy          Select     (KStringView sSelector)       { return kjson::Select(GetJSONRef()     , sSelector); }
		const Proxy    Select     (std::size_t iSelector) const { return kjson::Select(GetConstJSONRef(), iSelector); }
		Proxy          Select     (std::size_t iSelector)       { return kjson::Select(GetJSONRef()     , iSelector); }
		const Proxy    operator[] (KStringView sSelector) const { return Select(sSelector); }
		Proxy          operator[] (KStringView sSelector)       { return Select(sSelector); }
		const Proxy    operator[] (const char* sSelector) const { return Select(sSelector); }
		Proxy          operator[] (const char* sSelector)       { return Select(sSelector); }
		const Proxy    operator[] (std::size_t iSelector) const { return Select(iSelector); }
		Proxy          operator[] (std::size_t iSelector)       { return Select(iSelector); }

		const KJSON&   ToKJSON    () const noexcept { return GetConstJSONRef(); }
		KJSON&         ToKJSON    () noexcept       { return GetJSONRef();      }

		KJSON::value_t type       () const noexcept { return m_json->type();                 }
		bool           is_object  () const noexcept { return m_json->is_object();            }
		bool           is_array   () const noexcept { return m_json->is_array();             }
		bool           is_string  () const noexcept { return m_json->is_string();            }
		bool           is_integer () const noexcept { return m_json->is_number_integer();    }
		bool           is_unsigned() const noexcept { return m_json->is_number_unsigned();   }
		bool           is_signed  () const noexcept { return is_integer() && !is_unsigned(); }
		bool           is_float   () const noexcept { return m_json->is_number_float();      }
		bool           is_null    () const noexcept { return m_json->is_null();              }
		bool           is_boolean () const noexcept { return m_json->is_boolean();           }

	private:
		               // this private constructor enables const Proxy construction from
		               // KJSON2::Selector() const
		               Proxy      (const KJSON& json) : m_json(&const_cast<KJSON&>(json)) {}

		const KJSON&   GetConstJSONRef() const { return *m_json; }
		KJSON&         GetJSONRef()            { return *m_json; }

		KJSON* m_json;

	}; // Proxy

	using self = KJSON2;

	KJSON2() = default;

	KJSON2(KJSON json) : m_json(std::move(json)) { }
	KJSON2(KJSON::initializer_list_t il,
		   bool type_deduction = true,
		   KJSON::value_t manual_type = KJSON::value_t::array) : m_json(std::move(il), type_deduction, manual_type) {}
	template<class String,
	         typename std::enable_if<std::is_constructible<KStringView, String>::value>::type i = 0>
	KJSON2(const String& sJson)     { Parse(sJson);  }
	KJSON2(const Proxy& proxy);
	KJSON2(const KROW& row);

	bool           Parse      (KStringView sJson, bool bThrow = false) { return GetProxy().Parse(sJson, bThrow); }
	KString        Dump       (bool bPretty = false) const             { return GetConstProxy().Dump(bPretty);        }
	KString        Serialize  (bool bPretty = false) const             { return Dump(bPretty);                     }
	KString        dump       ()                     const             { return Dump(false);                       }


	const KJSON&   ToKJSON    () const { return m_json; }
	KJSON&         ToKJSON    ()       { return m_json; }

	operator const KJSON&     () const { return m_json; }
	operator       KJSON&     ()       { return m_json; }

	const Proxy    Select     (KStringView sSelector = KStringView{}) const { return GetConstProxy().Select(sSelector); }
	Proxy          Select     (KStringView sSelector = KStringView{})       { return GetProxy().Select(sSelector);      }
	const Proxy    operator[] (KStringView sSelector) const                 { return Select(sSelector); }
	Proxy          operator[] (KStringView sSelector)                       { return Select(sSelector); }
	const Proxy    operator[] (const char* sSelector) const                 { return Select(sSelector); }
	Proxy          operator[] (const char* sSelector)                       { return Select(sSelector); }

	bool           Exists     (KStringView sSelector) const;
	bool           Contains   (KStringView sSelector) const;
	bool           RecursiveMatchValue(KStringView sSearch) const { return kjson::RecursiveMatchValue(m_json, sSearch); }
	void           Merge      (const KJSON2& other) { kjson::Merge(m_json, other); }
	void           Merge      (const KJSON&  other) { kjson::Merge(m_json, other); }

	bool           is_object  () const noexcept { return m_json.is_object();             }
	bool           is_array   () const noexcept { return m_json.is_array();              }
	bool           is_string  () const noexcept { return m_json.is_string();             }
	bool           is_integer () const noexcept { return m_json.is_number_integer();     }
	bool           is_unsigned() const noexcept { return m_json.is_number_unsigned();    }
	bool           is_signed  () const noexcept { return is_integer() && !is_unsigned(); }
	bool           is_float   () const noexcept { return m_json.is_number_float();       }
	bool           is_null    () const noexcept { return m_json.is_null();               }
	bool           is_boolean () const noexcept { return m_json.is_boolean();            }

	template<typename T>
	self&          operator+= (T&& value) { m_json.operator+=(std::forward<T>(value)); return *this; }

private:

	const Proxy    GetConstProxy() const { return m_json; }
	Proxy          GetProxy()            { return m_json; }

	KJSON          m_json;

}; // KJSON2

DEKAF2_PUBLIC
inline
bool operator==(const KJSON2& left, const KJSON2& right)
{
	return left.ToKJSON() == right.ToKJSON();
}

DEKAF2_PUBLIC
inline
bool operator==(const KJSON2::Proxy& left, const KJSON2::Proxy& right)
{
	return left.ToKJSON() == right.ToKJSON();
}

template<typename T, typename U,
         typename std::enable_if<std::is_same<typename std::decay<T>::type, KJSON2::Proxy>::value &&
                                 std::is_constructible<KStringView, U>::value, int>::type = 0>
DEKAF2_PUBLIC
bool operator==(T&& left, U&& right)
{
	return left.StringView() == right;
}

DEKAF2_PUBLIC
inline
KString Print(const KJSON2& json2) noexcept
{
	return Print(json2.ToKJSON());
}

DEKAF2_PUBLIC
inline
KString Print(const KJSON2::Proxy& proxy) noexcept
{
	return Print(proxy.ToKJSON());
}

DEKAF2_PUBLIC
inline
std::ostream& operator <<(std::ostream& stream, KJSON2::Proxy proxy)
{
	stream << Print(proxy.ToKJSON());
	return stream;
}

} // end of namespace dekaf2
