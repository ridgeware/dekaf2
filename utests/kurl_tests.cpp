#include <map>
#include <tuple>
#include <sstream>
#include <iomanip>

#include <dekaf2/dekaf2.h>
#include "catch.hpp"
#include <dekaf2/kurl.h>

using namespace dekaf2;
using namespace dekaf2::KURL;
using std::get;
using std::map;
using std::tuple;
using std::stringstream;
using std::hex;

//            hint    final   flag
//            get<0>  get<1>  get<2> get<3>   get<4>
typedef tuple<size_t, size_t, bool,  KString, KString> parm_t;
typedef map<KString, parm_t> test_t;
// get<3> is unused but could be used if catch.hpp has output support.
// It is used to identify qualities of URL analyzed by KURL and friends.

typedef KProps<KString, KString, true, false> KProp_t;


#define VIEW_STE(id,source,target,expect) \
	INFO("VIEW[" <<\
		"#id" << "] " <<\
		" source='" << source << "'" <<\
		" target='" << source << "'" <<\
		" expect='" << source << "'" <<\
	)


SCENARIO ( "KURL unit tests on valid data" )
{
	test_t URL_valid;

	URL_valid["http://google.com"] =
		parm_t (0, 17, true, "minimum sensible URL", "");
	URL_valid["http://subsub.sub.go.co.jp"] =
		parm_t (0, 26, true, "valid URL with .co.", "");

	// user:pass in simple URLs
	URL_valid["https://jlettvin@mail.google.com"] =
		parm_t (0, 32, true, "user@host", "");
	URL_valid["https://user:password@mail.google.com"] =
		parm_t (0, 37, true, "user:pass@host", "");
	URL_valid["opaquelocktoken://user:password@point.domain.pizza"] =
		parm_t (0, 50, true, "unusual scheme", "");

	// query string in simple URL
	URL_valid["http://foo.bar?hello=world"] =
		parm_t (0, 26, true, "query on simple URL", "");

	// fragment in simple URL
	URL_valid["http://sushi.bar#uni"] =
		parm_t (0, 20, true, "fragment on simple URL", "");

	// scheme/user/pass/domain/query/fragment
	URL_valid["https://user:password@what.ever.com?please=stop%20the test%90&a=b#now"] =
		parm_t (0, 69, true, "all URL elements in use",
				"https://user:password@what.ever.com?please=stop+the+test%90&a=b#now"
				);

	// TODO This next one fails.  Fix it.
	URL_valid["a://b.c:1/d?e=f#g"] =
		parm_t (0, 17, true, "minimum valid fully populated URL",
				""); //"a://b.c:1%2Fd?e=f#g");

	URL_valid["https://user:password@what.ever.com:8080/home/guest/noop.php?please=stop#now"] =
		parm_t (0, 76, true, "all URL elements in use", "");

	GIVEN ( "a valid string" )
	{
		WHEN ( "parse simple urls" )
		{
			THEN ( "collect and test each" )
			{
				test_t::iterator it;
				for (it = URL_valid.begin(); it != URL_valid.end(); ++it)
				{
					const KString& source = it->first;
					const KStringView svSource =
						KStringView (source.c_str(), source.size ());
					parm_t&  parameter = it->second;
					KString  target{};
					size_t   hint   {get<0>(parameter)};
					size_t   done   {get<1>(parameter)};
					bool     want   {get<2>(parameter)};
					KString  expect {get<4>(parameter)};
					if (expect.size() == 0)
					{
						expect = source;
					}

					dekaf2::KURL::URL kurl  (svSource);
					bool have{kurl.Serialize (target)};

					INFO (svSource);
					if (want != have || target != expect || done != hint)
					{
						CHECK (target == expect);
						dekaf2::KURL::URL kurl  (svSource);
					}
					CHECK (want   == have  );
					CHECK (target == expect);
				}
			}
		}
		WHEN ( "Try all valid hex decodings" )
		{
			THEN ( "Loop through all valid hex digits in both places" )
			{
				KString iDigit{"0123456789ABCDEFabcdef"};
				size_t iHi, iLo, iExpect;
				KString sKey{"convert"};
				KString sBase{sKey + "=%"};
				for (iHi = 0; iHi < iDigit.size(); iHi++)
				{
					for (iLo = 0; iLo < iDigit.size(); iLo++)
					{
						KString sBefore{sBase};
						sBefore += iDigit[iHi];
						sBefore += iDigit[iLo];
						KStringView svConvert{sBefore};
						dekaf2::KURL::Query query;
						svConvert = query.Parse (svConvert);
						//const KProp_t& kprops = query.getProperties();
						KString sAfter = query[sKey];
						INFO ("Before:" + sBefore);
						INFO (" After:" + sAfter);
						CHECK (svConvert == "");
						CHECK (query[sKey].size() == 1);
					}
				}
			}
		}
	}

}

//SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS
SCENARIO ( "KURL unit tests on operators")
{
	test_t URL_valid;

	URL_valid["https://user:password@what.ever.com:8080/home/guest/noop.php?please=stop#now"] =
		parm_t (0, 76, true, "all URL elements in use", "");

	GIVEN ( "valid data")
	{
		WHEN ( "operating on an entire complex URL")
		{
			dekaf2::KURL::URL url;
			THEN ( "Check results for validity")
			{
				test_t::iterator it;
				for (it = URL_valid.begin(); it != URL_valid.end(); ++it)
				{
					KString sSource = it->first;
					KString sTarget;

					url << sSource;
					url >> sTarget;

					CHECK(sSource == sTarget);
				}
			}
		}
		WHEN ( "operating on individual fields of URL")
		{
			dekaf2::KURL::Protocol  protocol;
			dekaf2::KURL::User      user;
			dekaf2::KURL::Domain    domain;
			dekaf2::KURL::Path      path;
			dekaf2::KURL::Query     query;
			dekaf2::KURL::Fragment  fragment;
			dekaf2::KURL::URI       uri;
			THEN ( "Check results for validity")
			{
				KString sProtocol   {"unknown://"};
				KString sUser       {"some.body.is.a:Secretive1.@"};
				KString sDomain     {"East.Podunk.nj"};
				KString sPath       {"/too/many/subdirectories/for/comfort"};
				KString sQuery      {"?team=hermes&language=c++"};
				KString sFragment   {"#partwayDown"};
				KString sURI        {sPath + sQuery + sFragment};

				KString sResult;

				sResult.clear ();
				protocol << sProtocol;
				protocol >> sResult;
				CHECK ( sProtocol == sResult);

				sResult.clear ();
				user << sUser;
				user >> sResult;
				CHECK ( sUser == sResult);

				sResult.clear ();
				domain << sDomain;
				domain >> sResult;
				CHECK ( sDomain == sResult);

				sResult.clear ();
				path << sPath;
				path >> sResult;
				CHECK ( sPath == sResult);

				sResult.clear ();
				query << sQuery;
				query >> sResult;
				CHECK ( sQuery == sResult);

				sResult.clear ();
				fragment << sFragment;
				fragment >> sResult;
				CHECK ( sFragment == sResult);

				sResult.clear ();
				uri << sURI;
				uri >> sResult;
				CHECK ( sURI == sResult);
			}
		}
	}
}

//SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS
SCENARIO ( "KURL unit tests on invalid data")
{
	GIVEN ( "an invalid string" )
	{
		WHEN ( "parsing an empty string" )
		{
			KString sEmptyString{};
			KStringView svEmptyString = sEmptyString;
			dekaf2::KURL::Protocol  protocol;
			dekaf2::KURL::User      user;
			dekaf2::KURL::Domain    domain;
			dekaf2::KURL::Path      path;
			dekaf2::KURL::Query     query;
			dekaf2::KURL::Fragment  fragment;
			dekaf2::KURL::URI       uri;
			dekaf2::KURL::URL       url;

			THEN ( "check responses to empty string" )
			{
				KStringView svProto     {protocol.Parse( svEmptyString)};
				KStringView svUser      {user    .Parse( svProto)};
				KStringView svDomain    {domain  .Parse( svUser)};
				KStringView svPath      {path    .Parse( svDomain)};
				KStringView svQuery     {query   .Parse( svPath)};
				KStringView svFragment  {fragment.Parse( svQuery)};
				KStringView svURI       {uri     .Parse( svFragment)};
				KStringView svURL       {url     .Parse( svURI)};

				// Mandatory: Protocol, Domain, and URL cannot parse empty
				CHECK( protocol.size () == 0 );  // Fail on empty
				CHECK(     user.size () == 0 );
				CHECK(    query.size () == 0 );
				CHECK( fragment.size () == 0 );
			}
		}
		WHEN ( "parsing an invalid path" )
		{
			THEN ( "check for error" )
			{
				KString sBadPath{"fubar"};
				KStringView svBadPath = sBadPath;
				dekaf2::KURL::Path path;
				svBadPath = path.Parse (svBadPath);
				CHECK (svBadPath == sBadPath);
			}
		}
		WHEN ( "parsing an invalid query" )
		{
#if 0  // TODO
			THEN ( "check for missing '='" )
			{
				KString sBadQuery{"hello world"}; // missing ?.*=.*
				KStringView svBadQuery = sBadQuery;
				size_t hint{0};
				dekaf2::KURL::Query query;
				bool bReturn = query.Parse (svBadQuery, hint);
				CHECK (bReturn == false);
			}
#endif

			THEN ( "check for missing hex digits" )
			{
				KString sBadQuery{"hello=world%2"}; // missing %21
				KStringView svBadQuery = sBadQuery;
				dekaf2::KURL::Query query;
				svBadQuery = query.Parse (svBadQuery);
				//const KProp_t& kprops = query.getProperties();
				CHECK (svBadQuery != sBadQuery);
				CHECK (query["hello"] == "world%2");
			}
			THEN ( "check for bad hex digits" )
			{
				KString sBadQuery{"hello=world%gg"}; // bad %gg
				KStringView svBadQuery = sBadQuery;
				dekaf2::KURL::Query query;
				svBadQuery = query.Parse (svBadQuery);
				//const KProp_t& kprops = query.getProperties();
				CHECK (svBadQuery != sBadQuery);
				CHECK (query["hello"] == "world%gg");
			}
		}
		WHEN ( "Try all invalid 2 byte hex decodings" )
		{
			THEN ( "Loop through all invalid hex digits in both places" )
			{
				int i1, i2, iExpect;
				char c1, c2;
				KString sKey{"convert"};
				size_t iLength = sKey.size() + 4;
				int iLoopBegin{0x01};
				int iLoopEnd{0xff};
				for (i2 = iLoopBegin; i2 <= iLoopEnd; ++i2)
				{
					c2 = static_cast<char>(i2);
					if (c2 == '#' || c2 == '&' || c2 == '+')
					{
						// TODO should these special chars be handled better?
						// '\0' Special char used to identify char* end
						// #    Special char used to identify Fragment
						// &    Special char used to separate key:val pairs
						// +    Special char used to convert to ' '
						continue;
					}

					for (i1 = iLoopBegin; i1 <= iLoopEnd; ++i1)
					{
						c1 = static_cast<char>(i1);
						if (c1 == '#' || c1 == '&' || c1 == '+')
						{
							// '\0' Special char used to identify char* end
							// #    Special char used to identify Fragment
							// &    Special char used to separate key:val pairs
							// +    Special char used to convert to ' '
							continue;
						}
						if (isxdigit(c1) && isxdigit(c2))
						{
							continue;
						}
						KString sBefore{sKey};
						KString sValue{};
						sValue += '%';
						sValue += c2;
						sValue += c1;
						sBefore += "=" + sValue;

						KStringView svConvert(sBefore.c_str(), iLength);
						dekaf2::KURL::Query query;
						svConvert = query.Parse (svConvert);
						//const KProp_t& kprops = query.getProperties();
						KString sResult = query[sKey];
						size_t size = sResult.size();
						if (size == 2 || sValue != sResult)
						{
							dekaf2::KURL::Query bad;
							bad.Parse(svConvert);
							sResult.size();
						}
						if (sResult.size() != 3 || sResult != sValue) {
							dekaf2::KURL::Query bad;
							bad.Parse(svConvert);
						}
						CHECK (sResult.size() == 3);
						CHECK (sResult == sValue);
					}

				}
			}
		}
	}

}

//SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS
TEST_CASE ("KURL")
{
	//            hint    final   flag
	//            get<0>  get<1>  get<2> get<3>   get<4>
	typedef tuple<size_t, size_t, bool,  KString, KString> parm_t;
	typedef map<KString, parm_t> test_t;
	// get<3> is unused but could be used if catch.hpp has output support.
	// It is used to identify qualities of URL analyzed by KURL and friends.

	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// These are examples of valid URLs
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

	// simple URLs
	test_t URL_valid;

	URL_valid["http://google.com"] =
		parm_t (0, 17, true, "minimum sensible URL", "");
	URL_valid["http://subsub.sub.go.co.jp"] =
		parm_t (0, 26, true, "valid URL with .co.", "");

	// user:pass in simple URLs
	URL_valid["https://jlettvin@mail.google.com"] =
		parm_t (0, 32, true, "user@host", "");
	URL_valid["https://user:password@mail.google.com"] =
		parm_t (0, 37, true, "user:pass@host", "");
	URL_valid["opaquelocktoken://user:password@point.domain.pizza"] =
		parm_t (0, 50, true, "unusual scheme", "");

	// query string in simple URL
	URL_valid["http://foo.bar?hello=world"] =
		parm_t (0, 26, true, "query on simple URL", "");

	// fragment in simple URL
	URL_valid["http://sushi.bar#uni"] =
		parm_t (0, 20, true, "fragment on simple URL", "");

	// scheme/user/pass/domain/query/fragment
	URL_valid["https://user:password@what.ever.com?please=stop#now"] =
		parm_t (0, 51, true, "all URL elements in use", "");
	// TODO This next one fails.  Fix it.
	//URL_valid["a://b.c:1/d?e=f#g"] =
		//parm_t (0, 17, true, "minimum valid fully populated URL", "");

	URL_valid["https://user:password@what.ever.com:8080/home/guest/noop.php?please=stop#now"] =
		parm_t (0, 76, true, "all URL elements in use", "");

	URL_valid["a://b.c"] =
		parm_t (0, 7, true, "protocol can be 1 char", "a://b.c");

	URL_valid["file:///home/jlettvin/.bashrc"] =
		parm_t (0, 29, true, "file:/// handled", "");


	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// These are examples of valid URLs
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

	test_t URL_invalid;

	// TODO fix this one too.
	//URL_invalid["http://home/jlettvin/.bashrc"] =
		//parm_t (0, 0, false, "no TLD (Top Level Domain)", "");

	URL_invalid["I Can Has Cheezburger."] =
		parm_t (0, 0, false, "Garbage text", "");

	URL_invalid["\x01\x02\x03\x04\xa0\xa1\xa2\xfd\xfe\xff"] =
		parm_t (0, 0, false, "Non-ASCII", "");


	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// Data for tests designed early
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

		KString sProto1 {"https://"};
		KString sProto2 {"http://"};
		KString sUser   {"jlettvin:password@"};
		KString sDomain {"jlettvin.github.com:80"};
		KString sPath   {"/foo/bar"};
		KString sQuery  {"?baz=beef"};
		KString sFragment{"#fun"};
		KString sURI    {"/home/guest/noop.php?please=stop#now"};

		SECTION ("kurl simple test")
		{
			KString source1{sProto1+sUser+sDomain};
			KString source2{sProto2+sUser+sDomain};

			KString target;
			KString expect1 ("https://");

			dekaf2::KURL::Protocol kproto1  (source1);
			dekaf2::KURL::Protocol kproto2  (source2);
			dekaf2::KURL::Protocol kproto3  (kproto1);

			dekaf2::KURL::Query kqueryparms  (sQuery);

			Fragment kfragment  (sFragment);

			dekaf2::KURL::URI kuri  (sURI);

			dekaf2::KURL::URI kuriBad ("<b>I'm bold</b>");

			dekaf2::KURL::URL kurl;

			KString kurlOut{};
			INFO(kurl.Serialize(kurlOut));

			kproto1.Serialize (target);

			int how =  target.compare(expect1);
			size_t size = kproto1.size ();

			dekaf2::KURL::URL soperator;
			KString toperator;

			soperator << source1;
			soperator >> toperator;

			CHECK (toperator == source1);

			CHECK (how == 0);
			CHECK (size == 8);

			CHECK (kproto1 == kproto3);
			CHECK (kproto3 != kproto2);
		}

		SECTION ("Protocol User Domain simple test")
		{
			KString source1{sProto1+sUser+sDomain};
			KString source2{sProto2+sUser+sDomain};

			KString target;
			KString expect{source1};

			dekaf2::KURL::Protocol kproto (source1);
			source1 = source1.substr (kproto.size ());

			dekaf2::KURL::User kuser (source1);
			source1 = source1.substr (kuser.size ());

			dekaf2::KURL::Domain kdomain (source1);

			kproto .Serialize (target);
			kuser  .Serialize (target);
			kdomain.Serialize (target);

			int iDiff = target.compare(expect);

			CHECK (iDiff == 0);
		}

		SECTION ("Protocol/Domain hint offset")
		{
				KString target;
				KString& expect{sDomain};

				dekaf2::KURL::Domain kdomain  (sDomain);

				kdomain.Serialize (target);

				CHECK (target == expect);
		}

		//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		// SOLO unit tests
		//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

		SECTION ("Protocol move")
		{
			KString protocol("https://");
			dekaf2::KURL::Protocol k1 (protocol);
			dekaf2::KURL::Protocol k2;
			k1 = std::move(k2);
			k2 = KURL::Protocol("http://");
			CHECK (k2 != k1);
		}

		SECTION ("Protocol solo unit (pass https://)")
		{
			KString solo ("https://");
			KString expect{solo};
			KString target;
			size_t hint{0};

			dekaf2::KURL::Protocol kproto  (solo);
			hint = kproto.size ();

			bool ret = kproto.Serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
		}

		SECTION ("Protocol solo unit (pass even with missing slash)")
		{
			KString solo ("https:/");
			KString expect{"https://"};
			KString target{};

			dekaf2::KURL::Protocol kproto  (solo);

			bool ret = kproto.Serialize (target);

			if (target != expect)
			{
				dekaf2::KURL::Protocol kproto  (solo);
			}

			CHECK (ret == true);
			CHECK (target == expect);
		}

		SECTION ("Protocol solo unit (fail missing ://)")
		{
			KString solo ("https");
			KString expect{};
			KString target{};

			dekaf2::KURL::Protocol kproto  (solo);

			bool ret = kproto.Serialize (target);

			CHECK (ret == false);
			CHECK (target == expect);
		}

		SECTION ("Protocol solo unit (pass mailto:)")
		{
			KString solo ("mailto:");
			KString expect{"mailto:"};
			KString target{};

			dekaf2::KURL::Protocol kproto  (solo);

			bool ret = kproto.Serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (kproto.size () == expect.size());
		}

		SECTION ("User solo unit (pass foo:bar@)")
		{
			KString solo ("foo:bar@");
			KString expect{"foo:bar@"};
			KString target{};
			size_t hint{0};

			dekaf2::KURL::User kuserinfo  (solo);

			bool ret = kuserinfo.Serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (kuserinfo.size() == expect.size());
		}

		SECTION ("KURL bulk valid tests")
		{
			test_t::iterator it;
			for (it = URL_valid.begin(); it != URL_valid.end(); ++it)
			{
				const KString& source = it->first;
				parm_t&  parameter = it->second;
				KString  expect{source};
				KString  target{};
				size_t   hint{get<0>(parameter)};
				size_t   done{get<1>(parameter)};
				bool     want{get<2>(parameter)};

				dekaf2::KURL::URL kurl  (source);
				bool have{kurl.Serialize (target)};

				if (want != have || target != expect)
				{
					CHECK (source == expect);
				}
				INFO (target);
				INFO (expect);
				CHECK (want   == have  );
				CHECK (target == expect);
				CHECK (done   == target.size ()  );
			}
		}

		SECTION ("KURL bulk KStringView valid tests")
		{
			test_t::iterator it;
			for (it = URL_valid.begin(); it != URL_valid.end(); ++it)
			{
				const KString& source = it->first;
				INFO(source << "---" << source.c_str() << "---" << source.size());
				const KStringView svSource =
					KStringView (source.c_str (), source.size ());
				parm_t&  parameter = it->second;
				KString  expect{source};
				KString  target{};
				size_t   hint{get<0>(parameter)};
				size_t   done{get<1>(parameter)};
				bool     want{get<2>(parameter)};

				dekaf2::KURL::URL kurl  (svSource);
				bool have{kurl.Serialize (target)};

				if (want != have || target != expect)
				{
					CHECK (source == expect);
				}
				CHECK (want   == have  );
				CHECK (target == expect);
				CHECK (done   == target.size () );
			}
		}

		SECTION ("KURL bulk invalid tests")
		{
			test_t::iterator it;
			for (it = URL_invalid.begin(); it != URL_invalid.end(); ++it)
			{
				const KString& source = it->first;
				parm_t&  parameter = it->second;
				KString  target{};
				size_t   hint		{get<0>(parameter)};
				size_t   done		{get<1>(parameter)};
				bool     want		{get<2>(parameter)};
				KString  feature	{get<3>(parameter)};
				KString  expect     {get<4>(parameter)};

				dekaf2::KURL::URL kurl  (source);
				bool have{kurl.Serialize (target)};

				if (want != have || target != expect)
				{
					CHECK (target == expect);
					dekaf2::KURL::URL kurl  (source);
				}
				CHECK (want   == have  );
				CHECK (target == expect);
			}
		}

		SECTION ("KURL query properties")
		{
			KString ksQueryParms {"?hello=world&hola=mundo&space=%20"};
			dekaf2::KURL::Query query (ksQueryParms);
			//const KProp_t& kprops = query.getProperties();
			CHECK (query["hello"] == "world");
			CHECK (query["hola"] == "mundo");
			CHECK (query["space"] == " ");
		}

}