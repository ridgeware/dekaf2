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
		               Proxy      (KJSON&  json ) : m_json(&json        ) {}
		               Proxy      (KJSON2& json2) : m_json(&json2.m_json) {}

		template<class Value>
		self&          operator=(Value&& value)
		{
			*m_json = std::forward<Value>(value);
			return *this;
		}

		bool           Parse      (KStringView sJson, bool bThrow = false);
		KString        Dump       (bool bPretty = false) const;
		KString        Serialize  (bool bPretty = false) const { return Dump(bPretty); }

		self           Object     () const noexcept;
		self           Array      () const noexcept;
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

		const self     Select     (KStringView sSelector) const { return kjson::Select(GetConstJSONRef(), sSelector); }
		self           Select     (KStringView sSelector)       { return kjson::Select(GetJSONRef()     , sSelector); }
		const self     Select     (std::size_t iSelector) const { return kjson::Select(GetConstJSONRef(), iSelector); }
		self           Select     (std::size_t iSelector)       { return kjson::Select(GetJSONRef()     , iSelector); }
		const self     operator[] (KStringView sSelector) const { return Select(sSelector); }
		self           operator[] (KStringView sSelector)       { return Select(sSelector); }
		const self     operator[] (const char* sSelector) const { return Select(sSelector); }
		self           operator[] (const char* sSelector)       { return Select(sSelector); }
		const self     operator[] (std::size_t iSelector) const { return Select(iSelector); }
		self           operator[] (std::size_t iSelector)       { return Select(iSelector); }

		const KJSON&   ToKJSON    () const noexcept { return GetConstJSONRef(); }
		KJSON&         ToKJSON    () noexcept       { return GetJSONRef();      }

		bool           Exists     ()                   const noexcept { return GetConstJSONRef().is_null() == false;                   }
		bool           Contains   (KStringView sSearch)         const { return kjson::Contains(GetConstJSONRef(), sSearch);            }
		bool           RecursiveMatchValue(KStringView sSearch) const { return kjson::RecursiveMatchValue(GetConstJSONRef(), sSearch); }
		self&          Merge      (KJSON other)                       { kjson::Merge(GetJSONRef(), std::move(other)); return *this;    }

		template<typename T>
		self&          operator+= (T&& value)         { return Merge(std::forward<T>(value));  }

		KJSON::value_t type         () const noexcept { return m_json->type();                 }
		bool           is_object    () const noexcept { return m_json->is_object();            }
		bool           is_array     () const noexcept { return m_json->is_array();             }
		bool           is_binary    () const noexcept { return m_json->is_binary();            }
		bool           is_primitive ()const noexcept  { return m_json->is_primitive();         }
		bool           is_structured()const noexcept  { return m_json->is_structured();        }
		bool           is_string    () const noexcept { return m_json->is_string();            }
		bool           is_integer   () const noexcept { return m_json->is_number_integer();    }
		bool           is_unsigned  () const noexcept { return m_json->is_number_unsigned();   }
		bool           is_signed    () const noexcept { return is_integer() && !is_unsigned(); }
		bool           is_float     () const noexcept { return m_json->is_number_float();      }
		bool           is_null      () const noexcept { return m_json->is_null();              }
		bool           is_boolean   () const noexcept { return m_json->is_boolean();           }

	private:
		               // this private constructor enables const Proxy construction from
		               // KJSON2::Selector() const
		               Proxy      (const KJSON& json) : m_json(&const_cast<KJSON&>(json)) {}

		const KJSON&   GetConstJSONRef() const { return *m_json; }
		KJSON&         GetJSONRef()            { return *m_json; }

		KJSON* m_json;

	}; // Proxy

	using self = KJSON2;

	               // make all defaults valid
	               KJSON2() = default;

	               // converting constructors
	               KJSON2(Proxy proxy)        : m_json(proxy.ToKJSON()) {}
	               KJSON2(KJSON::initializer_list_t il,
		                  bool type_deduction = true,
		                  KJSON::value_t manual_type = KJSON::value_t::array)
	               : m_json(std::move(il), type_deduction, manual_type) {}
	               template <class T,
	                         typename std::enable_if<std::is_constructible<KJSON, T>::value, int>::type = 0>
	               KJSON2(T&& value) noexcept : m_json(std::forward<T>(value)) {}

	bool           Parse      (KStringView sJson, bool bThrow = false) { return CreateProxy().Parse(sJson, bThrow); }
	KString        Dump       (bool bPretty = false)             const { return CreateConstProxy().Dump(bPretty);   }
	KString        Serialize  (bool bPretty = false)             const { return Dump(bPretty);                      }
	KString        dump       ()                                 const { return Dump(false);                        }

	const KJSON&   ToKJSON    () const { return m_json; }
	KJSON&         ToKJSON    ()       { return m_json; }

	operator const KJSON&     () const { return m_json; }
	operator       KJSON&     ()       { return m_json; }

	const Proxy    Select     (KStringView sSelector = KStringView{}) const { return CreateConstProxy().Select(sSelector); }
	Proxy          Select     (KStringView sSelector = KStringView{})       { return CreateProxy().Select(sSelector);      }
	const Proxy    Select     (std::size_t iSelector)                 const { return CreateConstProxy().Select(iSelector); }
	Proxy          Select     (std::size_t iSelector)                       { return CreateProxy().Select(iSelector);      }
	const Proxy    operator[] (KStringView sSelector)                 const { return Select(sSelector); }
	Proxy          operator[] (KStringView sSelector)                       { return Select(sSelector); }
	const Proxy    operator[] (const char* sSelector)                 const { return Select(sSelector); }
	Proxy          operator[] (const char* sSelector)                       { return Select(sSelector); }
	const Proxy    operator[] (std::size_t iSelector)                 const { return Select(iSelector); }
	Proxy          operator[] (std::size_t iSelector)                       { return Select(iSelector); }

	bool           Exists     ()                            const { return CreateConstProxy().Exists();                     }
	bool           Contains   (KStringView sSearch)         const { return CreateConstProxy().Contains(sSearch);            }
	bool           RecursiveMatchValue(KStringView sSearch) const { return CreateConstProxy().RecursiveMatchValue(sSearch); }
	self&          Merge      (KJSON other)                       { CreateProxy().Merge(std::move(other)); return *this;    }

	template<typename T>
	self&          operator+=   (T&& value)       { return Merge(std::forward<T>(value));      }

	KJSON::value_t type         () const noexcept { return CreateConstProxy().type();          }
	bool           is_object    () const noexcept { return CreateConstProxy().is_object();     }
	bool           is_array     () const noexcept { return CreateConstProxy().is_array();      }
	bool           is_binary    () const noexcept { return CreateConstProxy().is_binary();     }
	bool           is_primitive () const noexcept { return CreateConstProxy().is_primitive();  }
	bool           is_structured() const noexcept { return CreateConstProxy().is_structured(); }
	bool           is_string    () const noexcept { return CreateConstProxy().is_string();     }
	bool           is_integer   () const noexcept { return CreateConstProxy().is_integer();    }
	bool           is_unsigned  () const noexcept { return CreateConstProxy().is_unsigned();   }
	bool           is_signed    () const noexcept { return CreateConstProxy().is_signed();     }
	bool           is_float     () const noexcept { return CreateConstProxy().is_float();      }
	bool           is_null      () const noexcept { return CreateConstProxy().is_null();       }
	bool           is_boolean   () const noexcept { return CreateConstProxy().is_boolean();    }

private:

	const Proxy    CreateConstProxy() const noexcept { return m_json; }
	Proxy          CreateProxy()            noexcept { return m_json; }

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
