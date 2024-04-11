#include "catch.hpp"

#include <dekaf2/kprops.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kcompatibility.h>
#include <dekaf2/kinstringstream.h>
#include <dekaf2/kfilesystem.h>
#include <unordered_map>
#include <map>
using namespace dekaf2;

namespace {

KTempDir TempDir;

}

TEST_CASE("KProps") {

	SECTION("basic instantiation tests for KProps")
	{

		SECTION("test perfect forwarding edge cases")
		{
			typedef KProps<KString, KString, false, true> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 1", "value 2");

			CHECK ( data.size()   == 1  );
			CHECK ( data["key 1"]  == "value 2" );

			KString k1("key 10");
			KString k2("key 10");
			KString v1("value 10");
			KString v2("value 20");

			data.Add(std::move(k1), std::move(v1));
			data.Add(std::move(k2), std::move(v2));

			CHECK ( data.size()   == 2 );
			CHECK ( data["key 10"]      == "value 20" );
			CHECK ( data["key 10"_ks]   == "value 20" );
			CHECK ( data["key 10"_ksv]  == "value 20" );
			CHECK ( data["key 10"_ksz]  == "value 20" );

#ifdef DO_MOVE_TESTS
#ifdef DEKAF2_HAS_FULL_CPP_17
			CHECK ( k1 == KString{} );
			CHECK ( v1 == KString{} );
#endif
#endif

			KString k100("key 100");
			KString v100("value 100");
			data.Add(std::move(k100), v100);

			CHECK ( data.size()   == 3 );
			CHECK ( data["key 100"]  == "value 100" );
#ifdef DO_MOVE_TESTS
#ifdef DEKAF2_HAS_FULL_CPP_17
			CHECK ( k100 == KString{} );
#endif
#endif
			CHECK ( v100 == KString{"value 100"} );

			KString k200("key 200");
			KString v200("value 200");
			data.Add(k200, std::move(v200));

			CHECK ( data.size()   == 4 );
			CHECK ( data["key 200"]  == "value 200" );
			CHECK ( k200 == KString{"key 200"} );
#ifdef DO_MOVE_TESTS
#ifdef DEKAF2_HAS_FULL_CPP_17
			CHECK ( v200 == KString{} );
#endif
#endif
		}

		SECTION("Sequential non-unique KProps, bulk generation")
		{
			typedef KProps<KString, KString, true, false> CMyKProps;
			CMyKProps data;

			for (size_t ct = 0; ct < 1000; ++ct)
			{
				data.Add(KString::to_string(ct), KString::to_string(ct));
			}

			CHECK ( data.size()      == 1000  );
			CHECK ( data["934"]      == "934" );
			CHECK ( data[28].second  == "28"  );
			CHECK ( data[528].second == "528" );

		}

		SECTION("Sequential unique KProps, bulk generation")
		{
			typedef KProps<KString, KString, true, true> CMyKProps;
			CMyKProps data;

			for (size_t ct = 0; ct < 1000; ++ct)
			{
				data.Add(KString::to_string(ct), KString::to_string(ct));
			}

			CHECK ( data.size()      == 1000  );
			CHECK ( data["934"]      == "934" );
			CHECK ( data[28].second  == "28"  );
			CHECK ( data[528].second == "528" );

		}

		SECTION("non-sequential non-unique KProps, bulk generation")
		{
			typedef KProps<KString, KString, false, false> CMyKProps;
			CMyKProps data;

			for (size_t ct = 0; ct < 1000; ++ct)
			{
				data.Add(KString::to_string(ct), KString::to_string(ct));
			}

			CHECK ( data.size()      == 1000  );
			CHECK ( data["934"]      == "934" );

		}

		SECTION("Non-sequential unique KProps, bulk generation")
		{
			typedef KProps<KString, KString, false, true> CMyKProps;
			CMyKProps data;

			for (size_t ct = 0; ct < 1000; ++ct)
			{
				data.Add(KString::to_string(ct), KString::to_string(ct));
			}

			CHECK ( data.size()      == 1000  );
			CHECK ( data["934"]      == "934" );

		}

		SECTION("Sequential non-unique KProps with <KString, KString>")
		{
			typedef KProps<KString, KString, true, false> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 2", "value 2");
			data.Add("key 1", "value 3");

			SECTION("storage sequence")
			{
				auto it = data.begin();

				CHECK( it->first     == KString("key 1")   );
				CHECK( it->second    == KString("value 1") );
				CHECK( (++it)->first == KString("key 2")   );
				CHECK( it->second    == KString("value 2") );
				CHECK( (++it)->first == KString("key 1")   );
				CHECK( it->second    == KString("value 3") );
			}
		}

		SECTION("removal in sequential non-unique KProps with <KString, KString>")
		{
			typedef KProps<std::string, std::string, true, false> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 2", "value 2");
			data.Remove("key 1");
			data.Add("key 1", "value 3");

			SECTION("storage sequence")
			{
				auto it = data.begin();

				CHECK( it->first     == KString("key 2")   );
				CHECK( it->second    == KString("value 2") );
				CHECK( (++it)->first == KString("key 1")   );
				CHECK( it->second    == KString("value 3") );
			}
		}

		SECTION("non-sequential non-unique KProps with <KString, KString>")
		{
			typedef KProps<KString, KString, false, false> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 2", "value 2");
			data.Add("key 1", "value 3");

			std::multimap<KString, KString> rmap;
			rmap.emplace("key 1", "value 1");
			rmap.emplace("key 1", "value 3");
			rmap.emplace("key 2", "value 2");

			auto find = [&rmap](CMyKProps::const_iterator it)
			{
				auto range = rmap.equal_range(it->first);
				for (auto rit = range.first; rit != range.second; ++rit)
				{
					if (rit->second == it->second)
					{
						rmap.erase(rit);
						return true;
					}
				}
				return false;
			};


			SECTION("storage sequence")
			{
				auto it = data.begin();

				CHECK( data.size() == 3   );
				CHECK( find(it) == true   );
				CHECK( find(++it) == true );
				CHECK( find(++it) == true );
			}
		}

		SECTION("Sequential unique KProps with <KString, KString>")
		{
			typedef KProps<KString, KString, true, true> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 2", "value 2");
			data.Add("key 1", "value 3");
			SECTION("storage sequence")
			{
				auto it = data.begin();

				CHECK( it->first     == KString("key 1")   );
				CHECK( it->second    == KString("value 3") );
				CHECK( (++it)->first == KString("key 2")   );
				CHECK( it->second    == KString("value 2") );
			}
		}

		SECTION("non-sequential unique KProps with <KString, KString>")
		{
			typedef KProps<KString, KString, false, true> CMyKProps;
			CMyKProps data;

			data.Add("key 1", "value 1");
			data.Add("key 2", "value 2");
			data.Add("key 1", "value 3");

			std::map<KString, KString> rmap;
			rmap.emplace("key 1", "value 3");
			rmap.emplace("key 2", "value 2");

			auto find = [&rmap](CMyKProps::const_iterator it)
			{
				auto mit = rmap.find(it->first);
				if (mit == rmap.end())
				{
					return false;
				}
				if (mit->second != it->second)
				{
					return false;
				}
				rmap.erase(mit);
				return true;
			};

			SECTION("storage sequence")
			{
				auto it = data.begin();

				CHECK( data.size() == 2   );
				CHECK( find(it) == true   );
				CHECK( find(++it) == true );
			}
		}

	}

	SECTION("Sequential non-unique KProps with <size_t, size_t>")
	{
		KProps<size_t, size_t> data;

		data.Add(1, 2);
		data.Add(5, 6);
		data.Add(3, 4);

		SECTION("storage sequence")
		{
			auto it = data.begin();

			CHECK( it->first     == 1 );
			CHECK( it->second    == 2 );
			CHECK( (++it)->first == 5 );
			CHECK( it->second    == 6 );
			CHECK( (++it)->first == 3 );
			CHECK( it->second    == 4 );
		}

		SECTION("key (size_t) index access")
		{
			CHECK ( data.size()      == 3   );
			data.at(1).second = 100;
			CHECK( data.at(1).second == 100 );
			CHECK( data.at(4).second == 0   );
		}

	}

	SECTION("Sequential KProps with <KString, KString>")
	{
		typedef KProps<KString, KString> CMyKProps;
		CMyKProps data;

		KString s1("hello");
		KString s2("bonjour");
		KString ss1("test");
		KString ss2("toast");
		data.Add(s1, s2);
		data.Add("test", "toast");
		data.Set(std::move(ss1), std::move(ss2));
		data.Add(s1, std::move(s2));

		SECTION("storage sequence")
		{
			CMyKProps::iterator it = data.begin();

			CHECK( it->first     == KString("hello")   );
			CHECK( it->second    == KString("bonjour") );
			CHECK( (++it)->first == KString("test")    );
			CHECK( it->second    == KString("toast")   );
			CHECK( (++it)->first == KString("hello")   );
			CHECK( it->second    == KString("bonjour") );
		}

		SECTION("key (string) index access")
		{
			CHECK ( data.size() == 3 );
			data["test"] = "champaign";
			CHECK( data[1].second == KString("champaign") );
			CHECK( data["test"]   == KString("champaign") );
			CHECK( data[4].second == KString("")          );
		}

		KProps<KString, KString> data2;
		data2.Add("color", "black");
		data2.Add("color", "blue");
		data2.Add("color", "red");

		SECTION("duplicate keys")
		{
			CHECK ( data2.size() == 3 );
			CHECK ( data2[0].first  == KString("color") );
			CHECK ( data2[0].second == KString("black") );
			CHECK ( data2[1].first  == KString("color") );
			CHECK ( data2[1].second == KString("blue")  );
			CHECK ( data2[2].first  == KString("color") );
			CHECK ( data2[2].second == KString("red")   );
		}

		data += data2;

		SECTION("merge")
		{
			CHECK ( data.size() == 6 );
			CHECK ( data[0].first  == KString("hello")     );
			CHECK ( data[0].second == KString("bonjour")   );
			CHECK ( data[1].first  == KString("test")      );
			CHECK ( data[1].second == KString("toast")     );
			CHECK ( data[2].first  == KString("hello")     );
			CHECK ( data[2].second == KString("bonjour")   );
			CHECK ( data[3].first  == KString("color")     );
			CHECK ( data[3].second == KString("black")     );
			CHECK ( data[4].first  == KString("color")     );
			CHECK ( data[4].second == KString("blue")      );
			CHECK ( data[5].first  == KString("color")     );
			CHECK ( data[5].second == KString("red")       );
		}

		SECTION("ranges")
		{
			CHECK ( data.size() == 6 );
			auto range = data.GetMulti("color");
			CHECK ( std::distance(range.first, range.second) == 3);
		}

		SECTION("single replacement")
		{
			data.Set("color", "blue", "yellow");
			CHECK ( data[3].first  == KString("color")     );
			CHECK ( data[3].second == KString("black")     );
			CHECK ( data[4].first  == KString("color")     );
			CHECK ( data[4].second == KString("yellow")    );
			CHECK ( data[5].first  == KString("color")     );
			CHECK ( data[5].second == KString("red")       );
		}

		SECTION("multi replacement")
		{
			data.Set("color", "white");
			CHECK ( data.size() == 6 );
			CHECK ( data[3].first  == KString("color")     );
			CHECK ( data[3].second == KString("white")     );
			CHECK ( data[4].first  == KString("color")     );
			CHECK ( data[4].second == KString("white")     );
			CHECK ( data[5].first  == KString("color")     );
			CHECK ( data[5].second == KString("white")     );
		}

		SECTION("associative and index access on unexisting elements")
		{
			CHECK ( data.size() == 6                     );
			auto s = data["flag"];
			CHECK ( data["flag"] == KString("")          );
			CHECK ( data.size() == 7                     );
			data["flag"] = "whatever";
			CHECK ( data["flag"]  == KString("whatever") );
			data["abcde"] = "whatelse";
			CHECK ( data.size() == 8                     );
			CHECK ( data["abcde"] == KString("whatelse") );
		}
	}

	SECTION("Non-Sequential KProps with <KString, KString>")
	{
		KProps<KString, KString, false> data;

		KString s1("hello");
		KString s2("bonjour");
		data.Add(s1, s2);
		data.Add("test", "toast");
		data.Add(s1, std::move(s2));

		KProps<KString, KString, false> data2;
		data2.Add("color", "black");
		data2.Add("color", "blue");
		data2.Add("color", "red");

		SECTION("duplicate keys")
		{
			CHECK ( data2.size() == 3 );
			data2.Set("color", "white");
			CHECK ( data2.size() == 3 );
		}

		SECTION("duplicate keys merged")
		{
			CHECK ( data.size() == 3 );
			CHECK ( data2.size() == 3 );
			data += data2;
			CHECK ( data.size() == 6 );
			data.Set("color", "white");
			CHECK ( data.size() == 6 );

		}

	}


	SECTION("Sequential Unique KProps with <KString, KString>")
	{
		typedef KProps<KString, KString, true, true> CMyKProps;
		CMyKProps data;

		KString s1("hello");
		KString s2("bonjour");
		KString s3("goodbye");
		data.Add(s1, s2);
		data.Add("test", "toast");
		data.Remove("hello");
		data.Add(s1, std::move(s3));

		SECTION("storage sequence")
		{
			CMyKProps::iterator it = data.begin();

			CHECK( it->first     == KString("test")    );
			CHECK( it->second    == KString("toast")   );
			CHECK( (++it)->first == KString("hello")   );
			CHECK( it->second    == KString("goodbye") );
		}

		SECTION("key (string) index access")
		{
			CHECK ( data.size() == 2 );
			data["test"] = "champaign";
			CHECK( data[0].second == KString("champaign") );
			CHECK( data["test"] == KString("champaign") );
			CHECK( data[4].second  == KString("")          );
		}

		CMyKProps data2;
		data2.Add("color", "black");
		data2.Add("color", "blue");
		data2.Add("color", "red");

		SECTION("duplicate keys")
		{
			CHECK ( data2.size() == 1 );
			CHECK ( data2.begin()->first  == KString("color") );
			CHECK ( data2.begin()->second == KString("red")   );
		}

		data += data2;

		SECTION("merge")
		{
			CHECK ( data.size() == 3 );
			CHECK ( data[0].first == KString("test")      );
			CHECK ( data[0].second == KString("toast")     );
			CHECK ( data[1].first  == KString("hello")     );
			CHECK ( data[1].second == KString("goodbye")   );
			CHECK ( data[2].first  == KString("color")     );
			CHECK ( data[2].second == KString("red")       );
		}

		SECTION("ranges")
		{
			CHECK ( data.size() == 3 );
			auto range = data.GetMulti("color");
			CHECK ( std::distance(range.first, range.second) == 1);
		}

		SECTION("single replacement")
		{
			data.Set("color", "red", "yellow");
			CHECK ( data[2].first  == KString("color")     );
			CHECK ( data[2].second == KString("yellow")    );
		}

		SECTION("multi replacement")
		{
			data.Set("color", "white");
			CHECK ( data.size() == 3 );
			CHECK ( data[2].first  == KString("color")     );
			CHECK ( data[2].second == KString("white")     );
		}

		SECTION("associative and index access on unexisting elements")
		{
			CHECK ( data.size() == 3                     );
			auto s = data["flag"];
			CHECK ( data["flag"] == KString("")          );
			CHECK ( data.size() == 4                     );
			data["flag"] = "whatever";
			CHECK ( data["flag"]  == KString("whatever") );
			data["abcde"] = "whatelse";
			CHECK ( data.size() == 5                     );
			CHECK ( data["abcde"] == KString("whatelse") );
		}

	}

	SECTION("order through replaces on non-unique")
	{
		typedef KProps<KString, KString, true, false> CMyKProps;
		CMyKProps data;

		data.Add("key 1", "value 1");
		data.Add("key 2", "value 2");
		data.Set("key 1", "value 3");

		SECTION("storage sequence 1")
		{
			auto it = data.cbegin();

			CHECK ( data.size() == 2                   );
			CHECK( it->first     == KString("key 1")   );
			CHECK( it->second    == KString("value 3") );
			CHECK( (++it)->first == KString("key 2")   );
			CHECK( it->second    == KString("value 2") );
		}

		data.Add("key 1", "value 4");
		data.Add("key 1", "value 5");
		data.Set("key 1", "value 4", "value 6");

		SECTION("storage sequence 2")
		{
			auto it = data.cbegin();

			CHECK ( data.size() == 4                   );
			CHECK( it->first     == KString("key 1")   );
			CHECK( it->second    == KString("value 3") );
			CHECK( (++it)->first == KString("key 2")   );
			CHECK( it->second    == KString("value 2") );
			CHECK( (++it)->first == KString("key 1")   );
			CHECK( it->second    == KString("value 6") );
			CHECK( (++it)->first == KString("key 1")   );
			CHECK( it->second    == KString("value 5") );
		}

	}

	SECTION("order through replaces on unique")
	{
		typedef KProps<KString, KString, true, true> CMyKProps;
		CMyKProps data;

		data.Add("key 1", "value 1");
		data.Add("key 2", "value 2");
		data.Set("key 1", "value 3");

		SECTION("storage sequence 1")
		{
			auto it = data.cbegin();

			CHECK ( data.size() == 2                   );
			CHECK( it->first     == KString("key 1")   );
			CHECK( it->second    == KString("value 3") );
			CHECK( (++it)->first == KString("key 2")   );
			CHECK( it->second    == KString("value 2") );
		}

		data.Add("key 1", "value 4");
		data.Add("key 1", "value 5");
		data.Set("key 1", "value 4", "value 6");

		SECTION("storage sequence 2")
		{
			auto it = data.cbegin();

			CHECK ( data.size() == 2                   );
			CHECK( it->first     == KString("key 1")   );
			CHECK( it->second    == KString("value 6") );
			CHECK( (++it)->first == KString("key 2")   );
			CHECK( it->second    == KString("value 2") );
		}
	}

	SECTION("assignments")
	{
		typedef KProps<KString, KString, true, true> CMyKProps;
		CMyKProps data1;
		CMyKProps data2;

		data1.Add("hello", "world");
		data1.Add("copy", "me");

		SECTION("copy assignment")
		{
			CHECK ( data1.size() == 2                   );
			data2 = data1;
			CHECK ( data2.size() == 2                   );
		}

		SECTION("move assignment")
		{
			CHECK ( data1.size() == 2                   );
			data2 = std::move(data1);
			CHECK ( data2.size() == 2                   );
			CHECK ( data1.size() == 0                   );
		}
	}

	SECTION("store and load")
	{
		// prepare the data for the operation
		KProps<KString, KString> MyProps, NewProps;
		MyProps.emplace("row1-col1", "row1-col2");
		MyProps.emplace("row2-col1", "row2-col2");
		MyProps.emplace("row3-col1", "row3-col2");
		MyProps.emplace("row4-col1", "row4-col2");

		KString sFilename = kFormat("{}{}kprops-stored.txt", TempDir.Name(), kDirSep);
		CHECK( MyProps.Store(sFilename) == 4 );
		CHECK( NewProps.Load(sFilename) == 4 );
		CHECK( MyProps == NewProps );

		static constexpr KStringView sExpectedData = "#! KPROPS\nrow1-col1=row1-col2\nrow2-col1=row2-col2\nrow3-col1=row3-col2\nrow4-col1=row4-col2\n";
		auto sData = kReadAll(sFilename);
		CHECK ( sData == sExpectedData );
	}

	SECTION("std::map")
	{
		std::map<KString, KString> map;
		map.emplace( "key1", "value1" );
		map.emplace( "key2", "value2" );
		map.emplace( "key3", "value3" );
		map.emplace( "key4", "value4" );
		KStringView sv("key1");
		CHECK ( map.find("key1")     != map.end() );
		CHECK ( map.find(sv) != map.end() );
		CHECK ( map.find("key1"_ksz) != map.end() );
		CHECK ( map["key1"] == "value1" );
		CHECK ( map[sv] == "value1" );
	}

	SECTION("insert")
	{
		KProps<KString, KString, false, true> props1;
		auto pair1 = props1.insert( { "key1", "value1" });
		CHECK ( pair1.second == true );
		pair1 = props1.insert( { "key1", "value2" });
		CHECK ( pair1.second == false );
		CHECK ( pair1.first->second == "value1" );

		KProps<KString, KString, false, false> props2;
		auto pair2 = props2.insert( { "key1", "value1" });
		CHECK ( pair2.second == true );
		pair2 = props2.insert( { "key1", "value2" });
		CHECK ( pair2.second == true );
		CHECK ( pair2.first->second == "value2" );

		KProps<KString, KString, true, true> props3;
		auto pair3 = props3.insert( { "key1", "value1" });
		CHECK ( pair3.second == true );
		pair3 = props3.insert( { "key1", "value2" });
		CHECK ( pair3.second == false );
		CHECK ( pair3.first->second == "value1" );

		KProps<KString, KString, true, false> props4;
		auto pair4 = props4.insert( { "key1", "value1" });
		CHECK ( pair4.second == true );
		pair4 = props4.insert( { "key1", "value2" });
		CHECK ( pair4.second == true );
		CHECK ( pair4.first->second == "value2" );
	}

	SECTION("insert_or_assign")
	{
		KProps<KString, KString, false, true> props1;
		auto pair1 = props1.insert_or_assign("key1", "value1");
		CHECK ( pair1.second == true );
		pair1 = props1.insert_or_assign("key1", "value2");
		CHECK ( pair1.second == false );
		CHECK ( pair1.first->second == "value2" );

		KProps<KString, KString, false, false> props2;
		auto pair2 = props2.insert_or_assign("key1", "value1");
		CHECK ( pair2.second == true );
		pair2 = props2.insert_or_assign("key1", "value2");
		CHECK ( pair2.second == true );
		CHECK ( pair2.first->second == "value2" );

		KProps<KString, KString, true, true> props3;
		auto pair3 = props3.insert_or_assign("key1", "value1");
		CHECK ( pair3.second == true );
		pair3 = props3.insert_or_assign("key1", "value2");
		CHECK ( pair3.second == false );
		CHECK ( pair3.first->second == "value2" );

		KProps<KString, KString, true, false> props4;
		auto pair4 = props4.insert_or_assign("key1", "value1");
		CHECK ( pair4.second == true );
		pair4 = props4.insert_or_assign("key1", "value2");
		CHECK ( pair4.second == true );
		CHECK ( pair4.first->second == "value2" );
	}
    
    SECTION("load unique")
    {
        KString sFile = R"(test=abc
test=def
test=xyz
)";
                          
        KInStringStream iss(sFile);
        
        KProps<KString, KString, true, true> props;
        CHECK ( props.Load(iss) == 1     );
        CHECK ( props.size()    == 1     );
        CHECK ( props["test"]   == "xyz" );
    }

    SECTION("load non-unique")
    {
        KString sFile = R"(test=abc
test=def
test=xyz
)";
                          
        KInStringStream iss(sFile);
        
        KProps<KString, KString, true, false> props;
        CHECK ( props.Load(iss) == 3     );
        CHECK ( props.size()    == 3     );
        CHECK ( props["test"]   == "xyz" );
    }

    SECTION("load non-sequential unique")
    {
        KString sFile = R"(test=abc
test=def
test=xyz
)";
                          
        KInStringStream iss(sFile);
        
        KProps<KString, KString, false, true> props;
        CHECK ( props.Load(iss) == 1     );
        CHECK ( props.size()    == 1     );
        CHECK ( props["test"]   == "xyz" );
    }

    SECTION("load non-sequential non-unique")
    {
        KString sFile = R"(test=abc
test=def
test=xyz
)";
                          
        KInStringStream iss(sFile);
        
        KProps<KString, KString, false, false> props;
        CHECK ( props.Load(iss) == 3     );
        CHECK ( props.size()    == 3     );
        CHECK ( props["test"]   == "xyz" );
    }

}
