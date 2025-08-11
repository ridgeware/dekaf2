#include "catch.hpp"

#include <dekaf2/ktemperature.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringstream.h>
#include <vector>

using namespace dekaf2;

constexpr bool constexpr_tests()
{
	static_assert(sizeof(KCelsius   ) == sizeof(KCelsius   ::value_type), "KCelsius is too large");
	static_assert(sizeof(KKelvin    ) == sizeof(KKelvin    ::value_type), "KKelvin is too large");
	static_assert(sizeof(KFahrenheit) == sizeof(KFahrenheit::value_type), "KFahrenheit is too large");

	constexpr KCelsius c0(0.0);
	static_assert(KKelvin    (c0).get() == 273.15, "0°C to K failed");
	static_assert(KFahrenheit(c0).get() == 32.0,   "0°C to °F failed");

	constexpr KCelsius c100(100.0);
	static_assert(KKelvin    (c100).get() == 373.15, "100°C to K failed");
	static_assert(KFahrenheit(c100).get() == 212,    "100°C to °F failed");

	static_assert(KCelsius   (  0) == KKelvin    (273.15), "0°C == 273.15K failed");
	static_assert(KFahrenheit( 32) == KCelsius   (  0),    "32°F == 0°C failed");
	static_assert(KFahrenheit(-40) == KCelsius   (-40),    "-40°F == -40°C failed");
	static_assert(KCelsius   ( 10) != KFahrenheit( 32),    "10°C != 32°F failed");

	return true;
}

static_assert(constexpr_tests(), "constexpr tests failed");

TEST_CASE("KTemperature")
{
	SECTION("creation, conversion")
	{
		KCelsius Celsius(17.5);
		CHECK (  17.5_C     == Celsius          );
		CHECK (  17.5       == Celsius.get()    );
		CHECK (  17.5       == Celsius.Double() );
		CHECK (  18         == Celsius.Int()    );
		CHECK (  "18°C"     == Celsius.String() );
		CHECK (  290.65_K   == KKelvin(Celsius) );
		CHECK (  63.5_F     == KFahrenheit(Celsius) );
		CHECK (  "291K"     == KKelvin(Celsius).String() );
		CHECK (  "64°F"     == KFahrenheit(Celsius).String() );
		CHECK (  "18°C"     == Celsius.String(0) );
		CHECK (  "17.5°C"   == Celsius.String(1) );
		CHECK (  "17.5°C"   == Celsius.String(1, true) );
		CHECK (  "17.5"     == Celsius.String(1, false) );
		CHECK (  "17.500°C" == Celsius.String(3) );
		Celsius += 10.0_C;
		CHECK (  27.5       == Celsius.Double() );
		Celsius -= 10.0_C;
		CHECK (  17.5       == Celsius.Double() );
		CHECK (  18         == Celsius.Int()    );
		KKelvin Kelvin(Celsius);
		CHECK ( 290.65_K    == Kelvin           );
		CHECK ( 290.65      == Kelvin.get()     );
		CHECK ( 291         == Kelvin.Int()     );
		CHECK ( 290.65      == Kelvin.Double()  );
		CHECK ( "291K"      == Kelvin.String()  );
		CHECK ( "291K"      == Kelvin.String(0) );
		CHECK ( "290.6K"    == Kelvin.String(1) );
		CHECK ( "290.6K"    == Kelvin.String(1, true) );
		CHECK ( "290.6"     == Kelvin.String(1, false) );
		CHECK ( "290.650K"  == Kelvin.String(3) );
		auto Kelvin2 = 0_K;
		Kelvin2 += 10.0_K;
		CHECK ( 10.0        == Kelvin2.Double() );
		Kelvin2 += 17.0_K;
		CHECK ( 27.0        == Kelvin2.Double() );
		KFahrenheit Fahrenheit = Celsius + 10_C;
		CHECK ( 81.5_F      == Fahrenheit         );
		CHECK ( 81.5        == Fahrenheit.get()   );
		CHECK ( 81.5        == Fahrenheit.Double());
		CHECK ( -10.0       == (Celsius - Fahrenheit).Double() );
		CHECK ( "82°F"      == Fahrenheit.String());
		CHECK ( "82°F"      == Fahrenheit.String(0));
		CHECK ( "81.5°F"    == Fahrenheit.String(1));
		CHECK ( "81.5°F"    == Fahrenheit.String(1, true));
		CHECK ( "81.5"      == Fahrenheit.String(1, false));
		CHECK ( "81.500°F"  == Fahrenheit.String(3));
	}

	SECTION("comparison")
	{
		CHECK ( 18_C    < 18.1_C );
		CHECK ( 18.2_C  > 18.1_C );
		CHECK ( 18.1_C == 18.1_C );
		CHECK ( 18.2_C != 18.1_C );
		CHECK ( 18.1_C >= 18.1_C );
		CHECK ( 18.1_C <= 18.1_C );

		CHECK ( 18_K    < 18.1_K );
		CHECK ( 18.2_K  > 18.1_K );
		CHECK ( 18.1_K == 18.1_K );
		CHECK ( 18.2_K != 18.1_K );
		CHECK ( 18.1_K >= 18.1_K );
		CHECK ( 18.1_K <= 18.1_K );

		CHECK ( 18_F    < 18.1_F );
		CHECK ( 18.2_F  > 18.1_F );
		CHECK ( 18.1_F == 18.1_F );
		CHECK ( 18.2_F != 18.1_F );
		CHECK ( 18.1_F >= 18.1_F );
		CHECK ( 18.1_F <= 18.1_F );
	}

	SECTION("comparison mixed types")
	{
		CHECK ( 18_C    < 50_C    );
		CHECK ( 18.2_C  > 18.1_F  );
		CHECK ( 18.0_C == 64.40_F );
		CHECK ( 18.1_C != 64.40_F );
		CHECK ( 18.1_C >= 64.40_F );
		CHECK ( 18.0_C <= 64.40_F );

		CHECK ( 18_C    < 291.25_K );
		CHECK ( 18.2_C  > 291.15_K );
		CHECK ( 18.0_C == 291.15_K );
		CHECK ( 18.1_C != 291.15_K );
		CHECK ( 18.0_C >= 291.15_K );
		CHECK ( 18.0_C <= 291.15_K );

		CHECK ( 64.50_F  < 291.25_K );
		CHECK ( 64.59_F  > 291.25_K );
		CHECK ( 64.58_F == 291.25_K );
		CHECK ( 64.59_F != 291.25_K );
		CHECK ( 64.59_F >= 291.25_K );
		CHECK ( 64.58_F <= 291.25_K );
	}

	SECTION("formatting (fmt)")
	{
		auto c = 38.3_C;
		auto f = KFahrenheit(c);
		auto k = KKelvin(c);
		auto s = kFormat("it is hot: {} equals {} and {}", c, f, k);
		CHECK ( s == "it is hot: 38°C equals 101°F and 311K" );
	}

	SECTION("formatting (std::ostream)")
	{
		auto c = 38.3_C;
		auto f = KFahrenheit(c);
		auto k = KKelvin(c);
		KString sFormatted;
		KOutStringStream oss(sFormatted);
		oss << "it is hot: " << c << " equals " << f << " and " << k;
		CHECK ( sFormatted == "it is hot: 38°C equals 101°F and 311K" );
	}
}
