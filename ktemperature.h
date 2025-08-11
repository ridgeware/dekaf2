/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

/// @file ktemperature.h
/// constexpr temperature unit conversions and comparisons

#include "kdefinitions.h"
#include "kstring.h"
#include <cmath>
#include <type_traits>
#include <ostream>

DEKAF2_NAMESPACE_BEGIN

class KKelvin;
class KCelsius;
class KFahrenheit;

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KTemperature
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using value_type = double;

	constexpr KTemperature() = default;
	constexpr KTemperature(value_type value) : m_Temp(value) {}

	/// get the temperature representation as a double
	constexpr double   get              () const { return m_Temp;                }
	/// get the temperature representation as a double
	constexpr double   Double           () const { return get();                 }
	/// get the temperature representation rounded to an int
	constexpr int      Int              () const { return std::lround(Double()); }
	/// get the temperature representation as a double
	constexpr explicit operator double  () const { return Double();              }
	/// get the temperature representation rounded to an int
	constexpr explicit operator int     () const { return Int();                 }

	/// get the accepted deviation for two values being compared equal
	static constexpr value_type Epsilon ()       { return 1e-9;                  }

//------
protected:
//------

	KString ToString (uint8_t iPrecision, const char* szUnit) const { return kFormat("{:.{}f}{}", m_Temp, iPrecision, szUnit); }

	static constexpr value_type celsius_to_kelvin     (value_type c) { return c + 273.15;             }
	static constexpr value_type kelvin_to_celsius     (value_type k) { return k - 273.15;             }
	static constexpr value_type celsius_to_fahrenheit (value_type c) { return c * 9.0 / 5.0 + 32.0;   }
	static constexpr value_type fahrenheit_to_celsius (value_type f) { return (f - 32.0) * 5.0 / 9.0; }

	value_type m_Temp { 0.0 };

}; // KTemperature

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCelsius : public detail::KTemperature
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = detail::KTemperature;

//------
public:
//------

	constexpr KCelsius() = default;
	/// construct from a value type
	constexpr explicit KCelsius(value_type value) : base(value) {}
	/// construct and convert from KKelvin
	constexpr KCelsius(const KKelvin& Kelvin);
	/// construct and convert from KFahrenheit
	constexpr KCelsius(const KFahrenheit& Fahrenheit);

	constexpr KCelsius& operator+=(const KCelsius& other) { m_Temp += other.m_Temp; return *this; }
	constexpr KCelsius& operator-=(const KCelsius& other) { m_Temp -= other.m_Temp; return *this; }

	/// return the unit string for Celsius (°C)
	constexpr const char* Unit() const { return "°C"; }

	/// print temperature value in Celsius as string
	/// @par iPrecision the precision for the fixed point conversion, defaults to 0
	/// @par bWithUnit if true, the unit (°C) will be printed, too, defaults to true
	/// @return a KString with the given precision and possible unit
	KString String(uint8_t iPrecision = 0, bool bWithUnit = true) const { return base::ToString(iPrecision, bWithUnit ? Unit() : ""); }

}; // KCelsius

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KKelvin : public detail::KTemperature
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = detail::KTemperature;

//------
public:
//------

	constexpr KKelvin() = default;
	/// construct from a value type
	constexpr explicit KKelvin(value_type value)     : base(value) {}
	/// construct and convert from KCelsius
	constexpr KKelvin(const KCelsius&    Celsius)    : base(celsius_to_kelvin(Celsius.get())) {}
	/// construct and convert from KFahrenheit
	constexpr KKelvin(const KFahrenheit& Fahrenheit) : base(celsius_to_kelvin(KCelsius(Fahrenheit).get())) {}

	constexpr KKelvin& operator+=(const KKelvin& other) { m_Temp += other.m_Temp; return *this; }
	constexpr KKelvin& operator-=(const KKelvin& other) { m_Temp -= other.m_Temp; return *this; }

	/// return the unit string for Kelvin (K)
	constexpr const char* Unit() const { return "K"; }

	/// print temperature value in Kelvin as string
	/// @par iPrecision the precision for the fixed point conversion, defaults to 0
	/// @par bWithUnit if true, the unit (K) will be printed, too, defaults to true
	/// @return a KString with the given precision and possible unit
	KString String(uint8_t iPrecision = 0, bool bWithUnit = true) const { return base::ToString(iPrecision, bWithUnit ? Unit() : ""); }

}; // KKelvin

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KFahrenheit : public detail::KTemperature
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = detail::KTemperature;

//------
public:
//------

	constexpr KFahrenheit() = default;
	/// construct from a value type
	constexpr explicit KFahrenheit(value_type value) : base(value) {}
	/// construct and convert from KCelsius
	constexpr KFahrenheit(const KCelsius& Celsius)   : base(celsius_to_fahrenheit(Celsius.get())) {}
	/// construct and convert from KKelvin
	constexpr KFahrenheit(const KKelvin&  Kelvin)    : base(celsius_to_fahrenheit(KCelsius(Kelvin).get())) {}

	constexpr KFahrenheit& operator+=(const KFahrenheit& other) { m_Temp += other.m_Temp; return *this; }
	constexpr KFahrenheit& operator-=(const KFahrenheit& other) { m_Temp -= other.m_Temp; return *this; }

	/// return the unit string for Fahrenheit (°F)
	constexpr const char* Unit() const { return "°F"; }

	/// print temperature value in Fahrenheit as string
	/// @par iPrecision the precision for the fixed point conversion, defaults to 0
	/// @par bWithUnit if true, the unit (°F) will be printed, too, defaults to true
	/// @return a KString with the given precision and possible unit
	KString String(uint8_t iPrecision = 0, bool bWithUnit = true) const { return base::ToString(iPrecision, bWithUnit ? Unit() : ""); }

}; // KFahrenheit

constexpr KCelsius::KCelsius(const KKelvin&     Kelvin)     : base(kelvin_to_celsius    (Kelvin.get())) {}
constexpr KCelsius::KCelsius(const KFahrenheit& Fahrenheit) : base(fahrenheit_to_celsius(Fahrenheit.get())) {}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr T1 operator+(T1 left, const T2& right)
{
	left += T1(right);
	return left;
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr T1 operator-(T1 left, const T2& right)
{
	left -= T1(right);
	return left;
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator<(const T1& left, const T2& right)
{
	return KCelsius(left).get() < KCelsius(right).get();
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator>(const T1& left, const T2& right)
{
	return right < left;
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator>=(const T1& left, const T2& right)
{
	return !(left < right);
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator<=(const T1& left, const T2& right)
{
	return !(right < left);
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator==(const T1& left, const T2& right)
{
	auto diff = KCelsius(left).get() - KCelsius(right).get();
	return (diff < detail::KTemperature::Epsilon() && diff > -detail::KTemperature::Epsilon());
}

template<typename T1, typename T2,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value &&
                                 std::is_base_of<detail::KTemperature, T2>::value, int>::type = 0>
constexpr bool operator!=(const T1& left, const T2& right)
{
	return !(left == right);
}

constexpr KCelsius    operator "" _C(long double value)        { return KCelsius    { static_cast<KCelsius   ::value_type>(value) }; }
constexpr KFahrenheit operator "" _F(long double value)        { return KFahrenheit { static_cast<KFahrenheit::value_type>(value) }; }
constexpr KKelvin     operator "" _K(long double value)        { return KKelvin     { static_cast<KKelvin    ::value_type>(value) }; }
constexpr KCelsius    operator "" _C(unsigned long long value) { return KCelsius    { static_cast<KCelsius   ::value_type>(value) }; }
constexpr KFahrenheit operator "" _F(unsigned long long value) { return KFahrenheit { static_cast<KFahrenheit::value_type>(value) }; }
constexpr KKelvin     operator "" _K(unsigned long long value) { return KKelvin     { static_cast<KKelvin    ::value_type>(value) }; }

template<typename T1,
         typename std::enable_if<std::is_base_of<detail::KTemperature, T1>::value, int>::type = 0>
DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, T1 Temperature)
{
	auto s = Temperature.String();
	stream.write(s.data(), s.size());
	return stream;
}

DEKAF2_NAMESPACE_END

#if DEKAF2_HAS_INCLUDE("kformat.h")

// kFormat formatters

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KCelsius> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KCelsius& Temperature, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Temperature.String(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KKelvin> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KKelvin& Temperature, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Temperature.String(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KFahrenheit> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KFahrenheit& Temperature, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Temperature.String(), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // of has #include "kformat.h"
