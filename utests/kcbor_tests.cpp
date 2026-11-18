#include "catch.hpp"
#include <dekaf2/crypto/encoding/kcbor.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/core/strings/kstring.h>
#include <cmath>
#include <limits>
#include <vector>

using namespace dekaf2;

namespace {

struct Example
{
	KStringView sHex;
	KCBOR       Value;
};

KCBOR Counted(int iLast)
{
	auto Array = KCBOR::Array();

	for (int i = 1; i <= iLast; ++i)
	{
		Array.Add(i);
	}

	return Array;
}

} // end of anonymous namespace

TEST_CASE("KCBOR")
{
	SECTION("examples of RFC 8949, appendix A")
	{
		// these are written in their shortest form: reading gives the value, and
		// writing the value gives the same bytes
		std::vector<Example> Examples
		{
			{ "00"                    , 0                                                         },
			{ "01"                    , 1                                                         },
			{ "0a"                    , 10                                                        },
			{ "17"                    , 23                                                        },
			{ "1818"                  , 24                                                        },
			{ "1819"                  , 25                                                        },
			{ "1864"                  , 100                                                       },
			{ "1903e8"                , 1000                                                      },
			{ "1a000f4240"            , 1000000                                                   },
			{ "1b000000e8d4a51000"    , 1000000000000ll                                           },
			{ "1bffffffffffffffff"    , 18446744073709551615ull                                   },
			{ "c249010000000000000000", KCBOR::Tagged(2, KCBOR::Bytes(kUnHex("010000000000000000"))) },
			{ "3bffffffffffffffff"    , KCBOR::Negative(18446744073709551615ull)                  },
			{ "c349010000000000000000", KCBOR::Tagged(3, KCBOR::Bytes(kUnHex("010000000000000000"))) },
			{ "20"                    , -1                                                        },
			{ "29"                    , -10                                                       },
			{ "3863"                  , -100                                                      },
			{ "3903e7"                , -1000                                                     },
			{ "f90000"                , 0.0                                                       },
			{ "f93c00"                , 1.0                                                       },
			{ "fb3ff199999999999a"    , 1.1                                                       },
			{ "f93e00"                , 1.5                                                       },
			{ "f97bff"                , 65504.0                                                   },
			{ "fa47c35000"            , 100000.0                                                  },
			{ "fa7f7fffff"            , 3.4028234663852886e+38                                    },
			{ "fb7e37e43c8800759c"    , 1.0e+300                                                  },
			{ "f90001"                , 5.960464477539063e-8                                      },
			{ "f90400"                , 0.00006103515625                                          },
			{ "f9c400"                , -4.0                                                      },
			{ "fbc010666666666666"    , -4.1                                                      },
			{ "f97c00"                , std::numeric_limits<double>::infinity()                   },
			{ "f9fc00"                , -std::numeric_limits<double>::infinity()                  },
			{ "f4"                    , false                                                     },
			{ "f5"                    , true                                                      },
			{ "f6"                    , KCBOR()                                                   },
			{ "f7"                    , KCBOR::Undefined()                                        },
			{ "f0"                    , KCBOR::Simple(16)                                         },
			{ "f8ff"                  , KCBOR::Simple(255)                                        },
			{ "c074323031332d30332d32315432303a30343a30305a", KCBOR::Tagged(0, "2013-03-21T20:04:00Z") },
			{ "c11a514b67b0"          , KCBOR::Tagged(1, 1363896240)                              },
			{ "c1fb41d452d9ec200000"  , KCBOR::Tagged(1, 1363896240.5)                            },
			{ "d74401020304"          , KCBOR::Tagged(23, KCBOR::Bytes(kUnHex("01020304")))       },
			{ "d818456449455446"      , KCBOR::Tagged(24, KCBOR::Bytes(kUnHex("6449455446")))     },
			{ "d82076687474703a2f2f7777772e6578616d706c652e636f6d", KCBOR::Tagged(32, "http://www.example.com") },
			{ "40"                    , KCBOR::Bytes("")                                          },
			{ "4401020304"            , KCBOR::Bytes(kUnHex("01020304"))                          },
			{ "60"                    , ""                                                        },
			{ "6161"                  , "a"                                                       },
			{ "6449455446"            , "IETF"                                                    },
			{ "62225c"                , "\"\\"                                                    },
			{ "62c3bc"                , "\xc3\xbc"                                                },
			{ "63e6b0b4"              , "\xe6\xb0\xb4"                                            },
			{ "64f0908591"            , "\xf0\x90\x85\x91"                                        },
			{ "80"                    , KCBOR::Array()                                            },
			{ "83010203"              , Counted(3)                                                },
			{ "8301820203820405"      , KCBOR::Array().Add(1).Add(KCBOR::Array().Add(2).Add(3)).Add(KCBOR::Array().Add(4).Add(5)) },
			{ "98190102030405060708090a0b0c0d0e0f101112131415161718181819", Counted(25)          },
			{ "a0"                    , KCBOR::Map()                                              },
			{ "a201020304"            , KCBOR::Map().Add(1, 2).Add(3, 4)                          },
			{ "a26161016162820203"    , KCBOR::Map().Add("a", 1).Add("b", KCBOR::Array().Add(2).Add(3)) },
			{ "826161a161626163"      , KCBOR::Array().Add("a").Add(KCBOR::Map().Add("b", "c"))   },
			{ "a56161614161626142616361436164614461656145", KCBOR::Map().Add("a", "A").Add("b", "B").Add("c", "C").Add("d", "D").Add("e", "E") }
		};

		for (auto& Example : Examples)
		{
			INFO ( Example.sHex );

			KCBOR Value;

			CHECK ( Value.Parse(kUnHex(Example.sHex)) );
			CHECK ( Value.HasError() == false         );
			CHECK ( Value == Example.Value            );
			CHECK ( kHex(Example.Value.Serialize()) == Example.sHex );
		}
	}

	SECTION("examples of RFC 8949 in a longer form")
	{
		// floating point numbers in more bytes than needed, and strings, arrays and
		// maps of indefinite length: reading gives the value, writing the form of
		// the value, which is never longer
		std::vector<Example> Examples
		{
			{ "fa7f800000"                , std::numeric_limits<double>::infinity()               },
			{ "faff800000"                , -std::numeric_limits<double>::infinity()              },
			{ "fb7ff0000000000000"        , std::numeric_limits<double>::infinity()               },
			{ "fbfff0000000000000"        , -std::numeric_limits<double>::infinity()              },
			{ "5f42010243030405ff"        , KCBOR::Bytes(kUnHex("0102030405"))                    },
			{ "7f657374726561646d696e67ff", "streaming"                                           },
			{ "9fff"                      , KCBOR::Array()                                        },
			{ "9f018202039f0405ffff"      , KCBOR::Array().Add(1).Add(KCBOR::Array().Add(2).Add(3)).Add(KCBOR::Array().Add(4).Add(5)) },
			{ "9f01820203820405ff"        , KCBOR::Array().Add(1).Add(KCBOR::Array().Add(2).Add(3)).Add(KCBOR::Array().Add(4).Add(5)) },
			{ "83018202039f0405ff"        , KCBOR::Array().Add(1).Add(KCBOR::Array().Add(2).Add(3)).Add(KCBOR::Array().Add(4).Add(5)) },
			{ "83019f0203ff820405"        , KCBOR::Array().Add(1).Add(KCBOR::Array().Add(2).Add(3)).Add(KCBOR::Array().Add(4).Add(5)) },
			{ "9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff", Counted(25)          },
			{ "bf61610161629f0203ffff"    , KCBOR::Map().Add("a", 1).Add("b", KCBOR::Array().Add(2).Add(3)) },
			{ "826161bf61626163ff"        , KCBOR::Array().Add("a").Add(KCBOR::Map().Add("b", "c")) },
			{ "bf6346756ef563416d7421ff"  , KCBOR::Map().Add("Fun", true).Add("Amt", -2)          },
			// integers and lengths in more bytes than needed
			{ "1800"                      , 0                                                     },
			{ "190001"                    , 1                                                     },
			{ "1b0000000000000001"        , 1                                                     },
			{ "380a"                      , -11                                                   },
			{ "5801aa"                    , KCBOR::Bytes("\xaa")                                  },
			{ "fa3fc00000"                , 1.5                                                   },
			{ "fb3ff8000000000000"        , 1.5                                                   }
		};

		for (auto& Example : Examples)
		{
			INFO ( Example.sHex );

			KCBOR Value;

			CHECK ( Value.Parse(kUnHex(Example.sHex)) );
			CHECK ( Value == Example.Value            );
			CHECK ( Value.Serialize().size() <= Example.sHex.size() / 2 );
			CHECK ( Value.Serialize() == Example.Value.Serialize()     );
		}
	}

	SECTION("zero with a sign, and NaN")
	{
		KCBOR Value;

		CHECK ( Value.Parse(kUnHex("f98000")) );
		CHECK ( Value.IsFloat()               );
		CHECK ( Value.GetFloat() == 0.0       );
		CHECK ( std::signbit(Value.GetFloat()) );
		CHECK ( kHex(Value.Serialize())    == "f98000" );
		CHECK ( kHex(KCBOR(0.0).Serialize()) == "f90000" );

		for (KStringView sHex : { "f97e00", "fa7fc00000", "fb7ff8000000000000" })
		{
			INFO ( sHex );
			CHECK ( Value.Parse(kUnHex(sHex))     );
			CHECK ( Value.IsFloat()               );
			CHECK ( std::isnan(Value.GetFloat())  );
			CHECK ( kHex(Value.Serialize()) == "f97e00" );
		}
	}

	SECTION("all numbers of half precision")
	{
		// each of them is the shortest form of its value
		std::size_t iFailed { 0 };
		KString     sInput;
		KCBOR       Value;

		for (uint32_t iHalf = 0; iHalf <= 0xffff; ++iHalf)
		{
			sInput.clear();
			sInput += static_cast<char>(static_cast<uint8_t>(0xf9));
			sInput += static_cast<char>(static_cast<uint8_t>(iHalf >> 8));
			sInput += static_cast<char>(static_cast<uint8_t>(iHalf & 0xff));

			if (!Value.Parse(sInput) || !Value.IsFloat())
			{
				++iFailed;
			}
			else if (std::isnan(Value.GetFloat()))
			{
				// infinity has the exponent 31 with an empty mantissa
				if ((iHalf & 0x7c00) != 0x7c00 || (iHalf & 0x03ff) == 0)
				{
					++iFailed;
				}
			}
			else if (Value.Serialize() != sInput)
			{
				++iFailed;
			}
		}

		CHECK ( iFailed == 0 );
	}

	SECTION("floating point numbers that need more than half precision")
	{
		// the smallest subnormal number of single precision
		CHECK ( kHex(KCBOR(std::ldexp(1.0, -149)).Serialize()) == "fa00000001"         );
		// one more bit of mantissa than half precision has
		CHECK ( kHex(KCBOR(1.0 + std::ldexp(1.0, -11)).Serialize()) == "fa3f801000"    );
		// below the subnormal numbers of half precision
		CHECK ( kHex(KCBOR(std::ldexp(1.0, -25)).Serialize()) == "fa33000000"          );
		// above the range of half precision
		CHECK ( kHex(KCBOR(65536.0).Serialize()) == "fa47800000"                       );
		// one more bit of mantissa than single precision has
		CHECK ( kHex(KCBOR(1.0 + std::ldexp(1.0, -24)).Serialize()) == "fb3ff0000010000000" );
		// above the range of single precision
		CHECK ( kHex(KCBOR(std::numeric_limits<double>::max()).Serialize()) == "fb7fefffffffffffff" );
		// a float gives the same bytes as the double of its value
		CHECK ( KCBOR(1.5f).Serialize() == KCBOR(1.5).Serialize() );
	}

	SECTION("input that is not well-formed, RFC 8949 appendix F")
	{
		std::vector<KStringView> Inputs
		{
			// the input ends in a head
			"18", "19", "1a", "1b", "1901", "1a0102", "1b01020304050607", "38", "58", "78", "98",
			"9a01ff00", "b8", "d8", "f8", "f900", "fa0000", "fb000000",
			// strings of definite length with short data
			"41", "61", "5affffffff00", "5bffffffffffffffff010203", "7affffffff00", "7b7fffffffffffffff010203",
			// maps and arrays of definite length with too few items
			"81", "818181818181818181", "8200", "a1", "a20102", "a100", "a2000000",
			// a tag number without content
			"c0",
			// strings of indefinite length without a break
			"5f4100", "7f6100",
			// maps and arrays of indefinite length without a break
			"9f", "9f0102", "bf", "bf01020102", "819f", "9f8000", "9f9f9f9f9fffffffff", "9f819f819f9fffffff",
			// reserved values
			"1c", "1d", "1e", "3c", "3d", "3e", "5c", "5d", "5e", "7c", "7d", "7e", "9c", "9d", "9e",
			"bc", "bd", "be", "dc", "dd", "de", "fc", "fd", "fe",
			// simple values below 32 in two bytes
			"f800", "f801", "f818", "f81f",
			// chunks of a string of indefinite length of another type
			"5f00ff", "5f21ff", "5f6100ff", "5f80ff", "5fa0ff", "5fc000ff", "5fe0ff", "7f4100ff",
			// chunks of a string of indefinite length that have an indefinite length
			"5f5f4100ffff", "7f7f6100ffff",
			// a break on its own
			"ff",
			// a break in an array or a map of definite length
			"81ff", "8200ff", "a1ff", "a1ff00", "a100ff", "a20000ff", "9f81ff", "9f829f819f9fffffffff",
			// a break in the place of the value of a map entry
			"bf00ff", "bf000000ff",
			// integers and tags with an indefinite length
			"1f", "3f", "df",
			// no input at all
			""
		};

		for (auto sHex : Inputs)
		{
			INFO ( sHex );

			auto  Value = KCBOR::Array().Add(1);

			CHECK ( Value.Parse(kUnHex(sHex)) == false );
			CHECK ( Value.HasError()                   );
			CHECK ( Value.IsNull()                     );
		}
	}

	SECTION("bytes after the data item")
	{
		KCBOR Value;

		CHECK ( Value.Parse(kUnHex("0102")) == false );
		CHECK ( Value.HasError()                     );
		CHECK ( Value.IsNull()                       );

		// the next data item read clears the error
		CHECK ( Value.Parse(kUnHex("01")) );
		CHECK ( Value.HasError() == false );
		CHECK ( Value == 1                );

		// a sequence of data items, one after the other
		auto        sSequence = kUnHex("0183010203626869" "ff");
		KStringView sInput    = sSequence;

		CHECK ( Value.ParseFirst(sInput) );
		CHECK ( Value == 1               );
		CHECK ( sInput.size() == 8       );

		CHECK ( Value.ParseFirst(sInput) );
		CHECK ( Value == Counted(3)      );
		CHECK ( sInput.size() == 4       );

		CHECK ( Value.ParseFirst(sInput) );
		CHECK ( Value == "hi"            );
		CHECK ( sInput.size() == 1       );

		// a failure leaves the input as it is
		CHECK ( Value.ParseFirst(sInput) == false );
		CHECK ( Value.IsNull()                    );
		CHECK ( sInput.size() == 1                );
	}

	SECTION("nesting")
	{
		// arrays in arrays, the innermost holds an integer
		auto Nested = [](std::size_t iLevels)
		{
			KString sInput(iLevels, static_cast<char>(static_cast<uint8_t>(0x81)));
			sInput += '\0';
			return sInput;
		};

		KCBOR Value;

		CHECK ( Value.Parse(Nested(KCBOR::iDefaultMaxDepth))             );
		CHECK ( Value.Parse(Nested(KCBOR::iDefaultMaxDepth + 1)) == false );
		CHECK ( Value.HasError()                                         );
		CHECK ( Value.Parse(Nested(KCBOR::iDefaultMaxDepth + 1), 100)    );
		CHECK ( Value.Parse(Nested(101), 100)                    == false );

		// tags and maps count as well
		CHECK ( Value.Parse(kUnHex("c1a1018100"), 3)          );
		CHECK ( Value.Parse(kUnHex("c1a1018100"), 2) == false );

		// without any nesting allowed
		CHECK ( Value.Parse(kUnHex("6161"), 0)       );
		CHECK ( Value.Parse(kUnHex("80"), 0) == false );
	}

	SECTION("a map with integers as keys")
	{
		// a COSE key for ES256: key type EC2, algorithm -7, curve P-256, and two coordinates
		KString sX(32, 'x');
		KString sY(32, 'y');

		auto Key = KCBOR::Map()
			.Add( 1, 2)
			.Add( 3, -7)
			.Add(-1, 1)
			.Add(-2, KCBOR::Bytes(sX))
			.Add(-3, KCBOR::Bytes(sY));

		auto sEncoded = Key.Serialize();

		CHECK ( kHex(sEncoded) == KString("a5010203262001215820") + kHex(sX) + "225820" + kHex(sY) );

		KCBOR Read;

		CHECK ( Read.Parse(sEncoded)      );
		CHECK ( Read == Key               );
		CHECK ( Read.IsMap()              );
		CHECK ( Read.size()         == 5  );
		CHECK ( Read(1).GetInt64()  == 2  );
		CHECK ( Read(3).GetInt64()  == -7 );
		CHECK ( Read(-1).GetInt64() == 1  );
		CHECK ( Read(-2).IsBytes()        );
		CHECK ( Read(-2).GetString() == sX );
		CHECK ( Read(-3).GetString() == sY );

		CHECK ( Read.Contains(3)          );
		CHECK ( Read.Contains(2) == false );
		CHECK ( Read(2).IsNull()          );
		// a key of another type with the same number
		CHECK ( Read.Contains(1.0) == false );
		CHECK ( Read.Contains("1") == false );

		// of two entries with the same key the first is found
		CHECK ( Read.Parse(kUnHex("a201020103")) );
		CHECK ( Read.size()        == 2 );
		CHECK ( Read(1).GetInt64() == 2 );

		// a lookup in something that is no map
		CHECK ( Counted(3)(1).IsNull()            );
		CHECK ( Counted(3).Contains(1) == false   );
		CHECK ( KCBOR()("key").IsNull()           );
	}

	SECTION("types and their values")
	{
		KCBOR Null;

		CHECK ( Null.GetType() == KCBOR::Type::Null );
		CHECK ( Null.IsNull()                 );
		CHECK ( Null.GetBool()   == false     );
		CHECK ( Null.GetInt64()  == 0         );
		CHECK ( Null.GetUInt64() == 0         );
		CHECK ( Null.GetFloat()  == 0.0       );
		CHECK ( Null.GetString() == ""        );
		CHECK ( Null.GetArray().empty()       );
		CHECK ( Null.GetMap().empty()         );
		CHECK ( Null.GetTag()    == 0         );
		CHECK ( Null.GetTagged().IsNull()     );
		CHECK ( Null.GetSimple() == 22        );
		CHECK ( Null.size()      == 0         );

		CHECK ( KCBOR(true).IsBool()          );
		CHECK ( KCBOR(true).GetBool()         );
		CHECK ( KCBOR(false).IsBool()         );
		CHECK ( KCBOR(false).GetBool() == false );
		CHECK ( KCBOR(true).GetSimple() == 21 );
		CHECK ( KCBOR(1).GetBool()     == false );

		CHECK ( KCBOR(7).IsUnsigned()         );
		CHECK ( KCBOR(7).IsInteger()          );
		CHECK ( KCBOR(7u).IsUnsigned()        );
		CHECK ( KCBOR(-7).IsNegative()        );
		CHECK ( KCBOR(-7).IsInteger()         );
		CHECK ( KCBOR(-7).GetInt64()  == -7   );
		CHECK ( KCBOR(-7).GetUInt64() == 0    );
		CHECK ( KCBOR(-7).GetNegativeOffset() == 6 );
		CHECK ( KCBOR(static_cast<uint8_t>(200)).GetInt64() == 200 );
		CHECK ( KCBOR(static_cast<int8_t>(-100)).GetInt64() == -100 );

		// the limits of int64_t
		auto iMax = std::numeric_limits<int64_t>::max();
		auto iMin = std::numeric_limits<int64_t>::min();

		CHECK ( KCBOR(iMax).GetInt64() == iMax );
		CHECK ( KCBOR(iMin).GetInt64() == iMin );
		CHECK ( kHex(KCBOR(iMin).Serialize()) == "3b7fffffffffffffff" );
		CHECK ( KCBOR(18446744073709551615ull).GetInt64()  == iMax );
		CHECK ( KCBOR(18446744073709551615ull).GetUInt64() == 18446744073709551615ull );
		CHECK ( KCBOR::Negative(18446744073709551615ull).GetInt64() == iMin );
		CHECK ( KCBOR::Negative(0).GetInt64() == -1 );

		CHECK ( KCBOR(1.5).IsFloat()          );
		CHECK ( KCBOR(1.5).GetFloat() == 1.5  );
		CHECK ( KCBOR(1.5).GetInt64() == 0    );

		KString sText("text");

		CHECK ( KCBOR(sText).IsText()                         );
		CHECK ( KCBOR(KStringView(sText)).IsText()            );
		CHECK ( KCBOR(sText.c_str()).IsText()                 );
		CHECK ( KCBOR("text").GetString() == "text"           );
		CHECK ( KCBOR("text").size()      == 4                );
		CHECK ( KCBOR::Bytes("bytes").IsBytes()               );
		CHECK ( KCBOR::Bytes("bytes").GetString() == "bytes"  );
		// a string of bytes with a zero byte
		CHECK ( KCBOR::Bytes(KStringView("a\0b", 3)).size() == 3 );
		CHECK ( kHex(KCBOR::Bytes(KStringView("a\0b", 3)).Serialize()) == "43610062" );

		auto Tagged = KCBOR::Tagged(55799, Counted(2));

		CHECK ( Tagged.IsTagged()             );
		CHECK ( Tagged.GetTag() == 55799      );
		CHECK ( Tagged.GetTagged() == Counted(2) );
		CHECK ( Tagged.GetArray().empty()     );
		CHECK ( Tagged.size() == 0            );
		CHECK ( kHex(Tagged.Serialize()) == "d9d9f7820102" );

		// simple values
		CHECK ( KCBOR::Simple(20) == KCBOR(false)       );
		CHECK ( KCBOR::Simple(21) == KCBOR(true)        );
		CHECK ( KCBOR::Simple(22) == KCBOR()            );
		CHECK ( KCBOR::Simple(23) == KCBOR::Undefined() );
		CHECK ( KCBOR::Simple(19).IsSimple()            );
		CHECK ( KCBOR::Simple(19).GetSimple() == 19     );
		CHECK ( KCBOR::Simple(32).GetSimple() == 32     );
		CHECK ( kHex(KCBOR::Simple(32).Serialize()) == "f820" );
		// the reserved numbers
		CHECK ( KCBOR::Simple(24).IsNull()              );
		CHECK ( KCBOR::Simple(31).IsNull()              );
		CHECK ( KCBOR::Undefined().IsUndefined()        );
		CHECK ( KCBOR(7).GetSimple() == 0               );
	}

	SECTION("equality")
	{
		CHECK ( KCBOR(1)   == KCBOR(1)   );
		CHECK ( KCBOR(1)   != KCBOR(2)   );
		CHECK ( KCBOR(1)   != KCBOR(1.0) );
		CHECK ( KCBOR(1)   != KCBOR(-1)  );
		CHECK ( KCBOR(0)   != KCBOR()    );
		CHECK ( KCBOR(0)   != KCBOR(false) );
		CHECK ( KCBOR("a") != KCBOR::Bytes("a") );
		CHECK ( KCBOR::Array() != KCBOR::Map()  );
		CHECK ( KCBOR::Array() != KCBOR()       );
		CHECK ( Counted(3) == Counted(3) );
		CHECK ( Counted(3) != Counted(4) );
		CHECK ( KCBOR::Tagged(1, 5) != KCBOR::Tagged(2, 5) );
		CHECK ( KCBOR::Tagged(1, 5) != KCBOR::Array().Add(5) );
		// the order of the entries of a map counts
		CHECK ( KCBOR::Map().Add(1, 2).Add(3, 4) != KCBOR::Map().Add(3, 4).Add(1, 2) );
	}

	SECTION("building")
	{
		// a value of another type becomes an array or a map
		KCBOR Value("text");

		Value.Add(1);

		CHECK ( Value.IsArray()   );
		CHECK ( Value.size() == 1 );
		CHECK ( Value.GetString().empty() );

		Value.Add("key", "value");

		CHECK ( Value.IsMap()     );
		CHECK ( Value.size() == 1 );
		CHECK ( Value.GetArray().empty() );
		CHECK ( Value("key") == "value"  );

		Value.clear();

		CHECK ( Value.IsNull() );
		CHECK ( kHex(Value.Serialize()) == "f6" );

		// appended to a string that has content
		KString sOut("ab");
		Counted(2).Serialize(sOut);

		CHECK ( kHex(sOut) == "6162820102" );

		// lengths beyond 16 and 32 bits
		KString sLong(70000, 'a');

		auto sEncoded = KCBOR(sLong).Serialize();

		CHECK ( sEncoded.size() == 70005 );
		CHECK ( kHex(sEncoded.Left(5)) == "7a00011170" );

		KCBOR Read;

		CHECK ( Read.Parse(sEncoded)      );
		CHECK ( Read.GetString() == sLong );

		CHECK ( kHex(KCBOR(KString(300, 'b')).Serialize().Left(3)) == "79012c" );
	}
}
