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

#include "kjson.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJSON2
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
		Proxy&         operator=  (const Proxy&)  = default;
		Proxy&         operator=  (Proxy&&)       = default;

		               Proxy      (KJSON&  json ) : m_json(&json        ) {}
		               Proxy      (KJSON2& json2) : m_json(&json2.m_json) {}

		bool           Parse      (KStringView sJson, bool bThrow = false);
		KString        Dump       (bool bPretty = false) const;
		KString        Serialize  (bool bPretty = false) const { return Dump(bPretty); }

		const KJSON&   Object     () const noexcept;
		const KJSON&   Array      () const noexcept;
		const KString& String     () const noexcept;
		uint64_t       UInt       () const noexcept;
		int64_t        Int        () const noexcept;
		bool           Bool       () const noexcept;
		double         Float      () const noexcept;
/*
		KJSON&         Object     () { return *m_json; }
		KJSON&         Array      () { return *m_json; }
		KString&       String     () { return m_json->get_ref<KString& >(); }
		uint64_t&      UInt       () { return m_json->get_ref<uint64_t&>(); }
		int64_t&       Int        () { return m_json->get_ref<int64_t& >(); }
		bool&          Bool       () { return m_json->get_ref<bool&    >(); }
		double&        Float      () { return m_json->get_ref<double&  >(); }
*/
		operator const KJSON&     () const noexcept { return Object(); }
//		operator       KJSON&     ()                { return Object(); }
		operator const KString&   () const noexcept { return String(); }
//		operator       KString&   ()                { return String(); }
		operator       uint64_t   () const noexcept { return UInt();   }
//		operator       uint64_t&  ()                { return UInt();   }
		operator       int64_t    () const noexcept { return Int();    }
//		operator       int64_t&   ()                { return Int();    }
		operator       bool       () const noexcept { return Bool();   }
//		operator       bool&      ()                { return Bool();   }
		operator       double     () const noexcept { return Float();  }
//		operator       double&    ()                { return Float();  }

		template<class Value>
		Proxy& operator=(Value&& value)
		{
			*m_json = std::forward<Value>(value);
			return *this;
		}
/*
		Proxy& operator=(KStringView sString)
		{
			*m_json = sString;
			return *this;
		}
//		Proxy& operator=(uint64_t    iUInt  );
		Proxy& operator=(int64_t     iInt   )
		{
			*m_json = iInt;
			return *this;
		}
//		Proxy& operator=(bool        bBool  );
//		Proxy& operator=(double      dDouble);
		template<class JSON,
		         typename std::enable_if<std::is_same<typename std::decay<JSON>::type, KJSON>::value>::type = 0>
		Proxy& operator=(const JSON& json)
		{
			*m_json = std::forward<JSON>(json);
			return *this;
		}

		Proxy& operator=(const KJSON& json)
		{
			*m_json = json;
			return *this;
		}
*/
		const Proxy    Select     (KStringView sSelector) const { return kjson::Select(*m_json, sSelector); }
		Proxy          Select     (KStringView sSelector)       { return kjson::Select(*m_json, sSelector); }
		const Proxy    operator[] (KStringView sSelector) const { return Select(sSelector); }
		Proxy          operator[] (KStringView sSelector)       { return Select(sSelector); }
		const Proxy    operator[] (const char* sSelector) const { return Select(sSelector); }
		Proxy          operator[] (const char* sSelector)       { return Select(sSelector); }

		const KJSON&   ToKJSON    () const noexcept { return *m_json;    }
		KJSON&         ToKJSON    () noexcept       { return *m_json;    }

		bool           is_object  () const noexcept { return m_json->is_object();         }
		bool           is_array   () const noexcept { return m_json->is_array();          }
		bool           is_string  () const noexcept { return m_json->is_string();         }
		bool           is_integer () const noexcept { return m_json->is_number_integer(); }
		bool           is_float   () const noexcept { return m_json->is_number_float();   }
		bool           is_null    () const noexcept { return m_json->is_null();           }
		bool           is_boolean () const noexcept { return m_json->is_boolean();        }

	private:
		               // this private constructor enables const Proxy construction from
		               // KJSON2::Selector() const
		               Proxy      (const KJSON& json) : m_json(&const_cast<KJSON&>(json)) {}

		KJSON* m_json;

	};

	using self = KJSON2;

	KJSON2() = default;

	KJSON2(KJSON json) : m_json(std::move(json)) { }
	KJSON2(KJSON::initializer_list_t il,
		   bool type_deduction = true,
		   KJSON::value_t manual_type = KJSON::value_t::array) : m_json(std::move(il), type_deduction, manual_type) {}
	template<class String,
	         typename std::enable_if<std::is_constructible<KStringView, String>::value>::type i = 0>
	KJSON2(const String& sJson)     { Parse(sJson);  }

	KJSON2(const Proxy& proxy)      : KJSON2(proxy.ToKJSON()) {}

	bool           Parse      (KStringView sJson, bool bThrow = false) { return Proxy(*this).Parse(sJson, bThrow); }
	KString        Dump       (bool bPretty = false) const             { return Proxy(*this).Dump(bPretty);        }
	KString        Serialize  (bool bPretty = false) const             { return Dump(bPretty);                     }
	KString        dump       ()                     const             { return Dump(false);                       }


	operator const KJSON&     () const { return m_json; }
	operator       KJSON&     ()       { return m_json; }

	const KJSON&   ToKJSON    () const { return Proxy(*this).ToKJSON(); }
	KJSON&         ToKJSON    ()       { return Proxy(*this).ToKJSON(); }

	const Proxy    Select     (KStringView sSelector = KStringView{}) const { return Proxy(*this).Select(sSelector);      }
	Proxy          Select     (KStringView sSelector = KStringView{})       { return Proxy(*this).Select(sSelector);      }
	const Proxy    operator[] (KStringView sSelector) const                 { return Select(sSelector); }
	Proxy          operator[] (KStringView sSelector)                       { return Select(sSelector); }
	const Proxy    operator[] (const char* sSelector) const                 { return Select(sSelector); }
	Proxy          operator[] (const char* sSelector)                       { return Select(sSelector); }

	bool           Exists     (KStringView sSelector) const;
	bool           Contains   (KStringView sSelector) const;
	bool           RecursiveMatchValue(KStringView sSearch) const { return kjson::RecursiveMatchValue(m_json, sSearch); }
	void           Merge      (const KJSON2& other) { kjson::Merge(m_json, other); }
	void           Merge      (const KJSON&  other) { kjson::Merge(m_json, other); }

	bool           is_object  () const noexcept { return m_json.is_object();         }
	bool           is_array   () const noexcept { return m_json.is_array();          }
	bool           is_string  () const noexcept { return m_json.is_string();         }
	bool           is_integer () const noexcept { return m_json.is_number_integer(); }
	bool           is_float   () const noexcept { return m_json.is_number_float();   }
	bool           is_null    () const noexcept { return m_json.is_null();           }
	bool           is_boolean () const noexcept { return m_json.is_boolean();        }

private:

	KJSON          m_json;

}; // KJSON2

// ADL resolvers

inline void to_json  (KJSON2& j, const KROW& row) { to_json   (j.ToKJSON(), row); }
inline void from_json(const KJSON2& j, KROW& row) { from_json (j.ToKJSON(), row); }

inline void to_json  (KJSON2::Proxy& j, const KROW& row) { to_json   (j.ToKJSON(), row); }
inline void from_json(const KJSON2::Proxy& j, KROW& row) { from_json (j.ToKJSON(), row); }

} // end of namespace dekaf2
