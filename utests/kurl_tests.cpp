#include <map>
#include <tuple>

#include <dekaf2/dekaf2.h>
#include <dekaf2/kctype.h>
#include "catch.hpp"
#include <dekaf2/kurl.h>

using namespace dekaf2;

//            hint    final   flag
//            get<0>  get<1>  get<2> get<3>   get<4>
typedef std::tuple<size_t, size_t, bool,  KString, KString> parm_t;
typedef std::map<KString, parm_t> test_t;
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

// speed the permutation tests up by an order of magnitude..
#define CCHECK(x) if ((x) == false) CHECK(x)

SCENARIO ( "KURL unit tests on valid data" )
{
    test_t URL_valid;

    URL_valid["http://google.com"] =
        parm_t (0, 17, true, "minimum sensible URL", "");
    URL_valid["http://subsub.sub.go.co.jp"] =
        parm_t (0, 26, true, "valid URL with .co.", "");

    // user:pass in simple URLs
    URL_valid["https://johndoe@mail.google.com"] =
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
                    size_t   hint   {std::get<0>(parameter)};
                    size_t   done   {std::get<1>(parameter)};
                    bool     want   {std::get<2>(parameter)};
                    KString  expect {std::get<4>(parameter)};
                    if (expect.size() == 0)
                    {
                        expect = source;
                    }

                    dekaf2::KURL kurl  (svSource);
                    bool have{kurl.Serialize (target)};

                    INFO (svSource);
                    if (want != have || target != expect || done != hint)
                    {
                        CHECK (target == expect);
                        dekaf2::KURL kurl  (svSource);
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
                size_t iHi, iLo;
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
                        dekaf2::url::KQuery query;
                        svConvert = query.Parse (svConvert);
                        //const KProp_t& kprops = query.getProperties();
                        KString sAfter = query[sKey];
                        INFO ("Before:" + sBefore);
                        INFO (" After:" + sAfter);
                        CCHECK (svConvert == "");
                        CCHECK (query[sKey].size() == 1);
                    }
                }
            }
        }
    }

}

SCENARIO ( "KURL unit tests on operators")
{
    test_t URL_valid;

    URL_valid["https://user:password@what.ever.com:8080/home/guest/noop.php?please=stop#now"] =
        parm_t (0, 76, true, "all URL elements in use", "");

    GIVEN ( "valid data")
    {
        WHEN ( "operating on an entire complex URL")
        {
            dekaf2::KURL url;
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
            dekaf2::url::KProtocol  protocol;
            dekaf2::url::KUser      user;
            dekaf2::url::KPassword  password;
            dekaf2::url::KDomain    domain;
            dekaf2::url::KPath      path;
            dekaf2::url::KQuery     query;
            dekaf2::url::KFragment  fragment;
            dekaf2::KResource       uri;
            THEN ( "Check results for validity")
            {
                KString sProtocol   {"unknown://"};
                KString sUser       {"some.body.is.a@"};
                KString sPassword   {"Secretive1.@"};
                KString sDomain     {"East.Podunk.nj"};
                KString sPath       {"/too/many/subdirectories/for/comfort"};
                KString sQuery      {"?team=hermes&language=c++"};
                KString sFragment   {"#partwayDown"};
                KString sURI        {sPath + sQuery};

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
                password << sPassword;
                password >> sResult;
                CHECK ( sPassword == sResult);

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

SCENARIO ( "KURL unit tests on invalid data")
{
    GIVEN ( "an invalid string" )
    {
        WHEN ( "parsing an empty string" )
        {
            KStringView svEmptyString;
            dekaf2::url::KProtocol  protocol;
            dekaf2::url::KUser      user;
            dekaf2::url::KDomain    domain;
            dekaf2::url::KPath      path;
            dekaf2::url::KQuery     query;
            dekaf2::url::KFragment  fragment;
            dekaf2::KResource       resource;
            dekaf2::KURL            url;

            THEN ( "check responses to empty string" )
            {
                KStringView svProto     {protocol.Parse( svEmptyString)};
                KStringView svUser      {user    .Parse( svEmptyString)};
                KStringView svDomain    {domain  .Parse( svEmptyString)};
                KStringView svPath      {path    .Parse( svEmptyString)};
                KStringView svQuery     {query   .Parse( svEmptyString)};
                KStringView svFragment  {fragment.Parse( svEmptyString)};
                KStringView svResource  {resource.Parse( svEmptyString)};
                KStringView svURL       {url     .Parse( svEmptyString)};

                // Mandatory: Protocol, Domain, and URL cannot parse empty
                CHECK( protocol.empty () == true );  // Fail on empty
                CHECK(     user.empty () == true );
                CHECK(   domain.empty () == true );
                CHECK(     path.empty () == true );
                CHECK(    query.empty () == true );
                CHECK( fragment.empty () == true );
                CHECK( resource.empty () == true );
                CHECK(      url.empty () == true );
                
                CHECK(    svProto.empty () == true );
                CHECK(     svUser.empty () == true );
                CHECK(   svDomain.empty () == true );
                CHECK(     svPath.empty () == true );
                CHECK(    svQuery.empty () == true );
                CHECK( svFragment.empty () == true );
                CHECK( svResource.empty () == true );
                CHECK(      svURL.empty () == true );

            }
        }
        WHEN ( "parsing an invalid path" )
        {
            THEN ( "check for error" )
            {
                KString sBadPath{"fubar"};
                KStringView svBadPath = sBadPath;
                dekaf2::url::KPath path;
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
                dekaf2::url::KQuery query;
                bool bReturn = query.Parse (svBadQuery, hint);
                CHECK (bReturn == false);
            }
#endif

            THEN ( "check for missing hex digits" )
            {
                KString sBadQuery{"hello=world%2"}; // missing %21
                KStringView svBadQuery = sBadQuery;
                dekaf2::url::KQuery query;
                svBadQuery = query.Parse (svBadQuery);
                //const KProp_t& kprops = query.getProperties();
                CHECK (svBadQuery != sBadQuery);
                CHECK (query["hello"] == "world%2");
            }
            THEN ( "check for bad hex digits" )
            {
                KString sBadQuery{"hello=world%gg"}; // bad %gg
                KStringView svBadQuery = sBadQuery;
                dekaf2::url::KQuery query;
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
                int i1, i2;
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
						if (KASCII::kIsXDigit(c1) && KASCII::kIsXDigit(c2))
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
                        dekaf2::url::KQuery query;
                        svConvert = query.Parse (svConvert);
                        //const KProp_t& kprops = query.getProperties();
                        KString sResult = query[sKey];
                        size_t size = sResult.size();
                        if (size == 2 || sValue != sResult)
                        {
                            dekaf2::url::KQuery bad;
                            bad.Parse(svConvert);
                            sResult.size();
                        }
                        if (sResult.size() != 3 || sResult != sValue) {
                            dekaf2::url::KQuery bad;
                            bad.Parse(svConvert);
                        }
                        CCHECK (sResult.size() == 3);
                        CCHECK (sResult == sValue);
                    }

                }
            }
        }
    }

}

TEST_CASE ("KURL")
{
    //            hint    final   flag
    //            get<0>  get<1>  get<2> get<3>   get<4>
    typedef std::tuple<size_t, size_t, bool,  KString, KString> parm_t;
    typedef std::map<KString, parm_t> test_t;
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
    URL_valid["https://johndoe@mail.google.com"] =
        parm_t (0, 31, true, "user@host", "");
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

    URL_valid["file:///home/johndoe/.bashrc"] =
        parm_t (0, 28, true, "file:/// handled", "");


    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    // These are examples of valid URLs
    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

    test_t URL_invalid;

    // TODO fix this one too.
    //URL_invalid["http://home/johndoe/.bashrc"] =
        //parm_t (0, 0, false, "no TLD (Top Level Domain)", "");

    URL_invalid["I Can Has Cheezburger."] =
        parm_t (0, 0, true, "Garbage text", "I%20Can%20Has%20Cheezburger.");

    URL_invalid["\x01\x02\x03\x04\xa0\xa1\xa2\xfd\xfe\xff"] =
        parm_t (0, 0, true, "Non-ASCII", "%01%02%03%04%A0%A1%A2%FD%FE%FF");


    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    // Data for tests designed early
    //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

        KString sProto1 {"https://"};
        KString sProto2 {"http://"};
        KString sUser   {"johndoe:"};
        KString sPassword{"password@"};
        KString sDomain {"johndoe.github.com:80"};
        KString sPath   {"/foo/bar"};
        KString sQuery  {"?baz=beef"};
        KString sFragment{"#fun"};
        KString sURI    {"/home/guest/noop.php?please=stop#now"};

        SECTION ("kurl simple test")
        {
            KString source1{sProto1+sUser+sPassword+sDomain};
            KString source2{sProto2+sUser+sPassword+sDomain};

            KString target;
            KString expect1 ("https://");

            dekaf2::url::KProtocol kproto1  (source1);
            dekaf2::url::KProtocol kproto2  (source2);
            dekaf2::url::KProtocol kproto3  (kproto1);

            dekaf2::url::KQuery kqueryparms  (sQuery);

            dekaf2::url::KFragment kfragment  (sFragment);

            dekaf2::KResource kuri  (sURI);

            dekaf2::KResource kuriBad ("<b>I'm bold</b>");

            dekaf2::KURL kurl;

            KString kurlOut{};
            INFO(kurl.Serialize(kurlOut));

            kproto1.Serialize (target);

            int how =  target.compare(expect1);
            bool empty = kproto1.empty ();
            //size_t size = kproto1.size ();

            dekaf2::KURL soperator;
            KString toperator;

            soperator << source1;
            soperator >> toperator;

            CHECK (toperator == source1);

            CHECK (how == 0);
            CHECK (empty == false);
            //CHECK (size == 8);

            CHECK (kproto1 == kproto3);
            CHECK (kproto3 != kproto2);
        }

        SECTION ("Protocol User Domain simple test")
        {
            KString source1{sProto1+sUser+sPassword+sDomain};
            KString source2{sProto2+sUser+sPassword+sDomain};

            KString target;
            KString expect{source1};

            dekaf2::url::KProtocol kproto;
            source1 = kproto.Parse (source1);

            dekaf2::url::KUser kuser;
            source1 = kuser.Parse (source1);

            dekaf2::url::KPassword kpassword;
            source1 = kpassword.Parse (source1);

            dekaf2::url::KDomain kdomain;
            source1 = kdomain.Parse(source1);

            dekaf2::url::KPort kport (source1);

            kproto   .Serialize (target);
            kuser    .Serialize (target);
            kpassword.Serialize (target);
            kdomain  .Serialize (target);
            kport    .Serialize (target);

            int iDiff = target.compare(expect);

            CHECK (iDiff == 0);
            CHECK (target == expect);
        }

        SECTION ("Protocol/Domain hint offset")
        {
                KString target;
                KString expect{sDomain};

                dekaf2::url::KDomain kdomain;
                sDomain = kdomain.Parse(sDomain);

                dekaf2::url::KPort kport(sDomain);

                kdomain.Serialize (target);
                kport.Serialize (target);

                CHECK (target == expect);
        }

        //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
        // SOLO unit tests
        //||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

        SECTION ("Protocol move")
        {
            KString protocol("https://");
            dekaf2::url::KProtocol k1 (protocol);
            dekaf2::url::KProtocol k2;
            k1 = std::move(k2);
            k2 = url::KProtocol("http://");
            CHECK (k2 != k1);
        }

        SECTION ("Protocol solo unit (pass https://)")
        {
            KString solo ("https://");
            KString expect{solo};
            KString target;

            dekaf2::url::KProtocol kproto;
            kproto.Parse(solo);

            bool ret = kproto.Serialize (target);

            CHECK (ret == true);
            CHECK (target == expect);
        }

        SECTION ("Protocol solo unit (pass even with missing slash)")
        {
            KString solo ("https:/");
            KString expect{"https://"};
            KString target{};

            dekaf2::url::KProtocol kproto  (solo);

            bool ret = kproto.Serialize (target);

            if (target != expect)
            {
                dekaf2::url::KProtocol kproto  (solo);
            }

            CHECK (ret == true);
            CHECK (target == expect);
        }

        SECTION ("Protocol solo unit (succeed missing ://)")
        {
            KString solo ("https");
			KString expect{"https://"};
            KString target{};

            dekaf2::url::KProtocol kproto  (solo);

            bool ret1 = kproto.empty();
            bool ret = kproto.Serialize (target);

            CHECK (ret == true);
            CHECK (ret1 == false);
            CHECK (target == expect);
        }

        SECTION ("Protocol solo unit (pass mailto:)")
        {
            KString solo ("mailto:");
            KString expect{"mailto:"};
            KString target{};

            dekaf2::url::KProtocol kproto  (solo);

            bool ret = kproto.Serialize (target);

            CHECK (ret == true);
            CHECK (target == expect);
        }

        SECTION ("User solo unit (pass foo:bar@)")
        {
            KString solo ("foo:bar@");
            KString expect{"foo:bar@"};
            KString target{};

            dekaf2::url::KUser kuser;
            solo = kuser.Parse(solo);

            dekaf2::url::KPassword kpassword;
            solo = kpassword.Parse(solo);

            bool ret = kuser.Serialize (target) && kpassword.Serialize(target);

            CHECK (ret == true);
            CHECK (target == expect);
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
                size_t   done{std::get<1>(parameter)};
                bool     want{std::get<2>(parameter)};

                dekaf2::KURL kurl  (source);
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
                size_t   done{std::get<1>(parameter)};
                bool     want{std::get<2>(parameter)};

                dekaf2::KURL kurl  (svSource);
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
                bool     want    { std::get<2>(parameter) };
                KString  feature { std::get<3>(parameter) };
                KString  expect  { std::get<4>(parameter) };

                dekaf2::KURL kurl  (source);
                bool have{kurl.Serialize (target)};

                if (want != have || target != expect)
                {
                    CHECK (target == expect);
                    dekaf2::KURL kurl  (source);
                }
                CHECK (want   == have  );
                CHECK (target == expect);
            }
        }

        SECTION ("KURL query properties")
        {
            KString ksQueryParms {"?hello=world&hola=mundo&space=%20"};
            dekaf2::url::KQuery query (ksQueryParms);
            //const KProp_t& kprops = query.getProperties();
            CHECK (query["hello"] == "world");
            CHECK (query["hola"] == "mundo");
            CHECK (query["space"] == " ");
        }

        SECTION ("One-off coverage items")
        {
            KString ksHttpSlashless{"http:"};
            KString ksTarget;
            dekaf2::url::KProtocol protocol (ksHttpSlashless);
            protocol.Serialize (ksTarget);
            CHECK (ksTarget.size () == 0);

            ksTarget.clear();
            KString ksQueryNoEqual{"?a=b&fubar"};
            KStringView svQueryNoEqual{ksQueryNoEqual};
            dekaf2::url::KQuery queryNoEqual (svQueryNoEqual);
            CHECK (queryNoEqual.empty () == false); // this is simply an empty value

            ksTarget.clear();
            KString ksQueryBadKey{"?fu%2=bar"};
            KStringView svQueryBadKey{ksQueryBadKey};
            dekaf2::url::KQuery queryBadKey (svQueryBadKey);
            CHECK (queryBadKey.empty () == false);
        }
}

TEST_CASE ("KURL formerly missing")
{
    SECTION ("KURL base domain")
    {
        KStringView svDomain("abc.test.com");
        KURL Domain(svDomain);
        KString svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = "test.com";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = "test.co.uk";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = "www.test.co.uk";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = ".test.com";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = "lot.of.name.co.fragments.test.de";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "TEST");

        svDomain = "test";
        Domain = svDomain;
        svResult = Domain.GetBaseDomain();
        CHECK (svResult == "");
    }

    SECTION ("KURL ip addresses")
    {
        KStringView svURL("http://192.168.178.2:8080/and/a/path?with=cheese&with=onions#salt");
        KURL URL(svURL);
        KStringView svResult = URL.Domain;
        CHECK (svResult == "192.168.178.2");

        svURL = "http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:8080/and/a/path?with=cheese&with=onions#salt";
        URL = svURL;
        svResult = URL.Domain;
        CHECK (svResult == "[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]");
        KString ser;
        url::KQuery Query(URL.Query);
        Query.Serialize(ser);
        CHECK(ser == "?with=cheese&with=onions");

        svURL = "http://[::192.9.5.5]:8080/and/a/path?with=cheese&with=onions#salt";
        URL = svURL;
        svResult = URL.Domain;
        CHECK (svResult == "[::192.9.5.5]");

        svURL = "http://[3ffe:2a00:100:7031::]:8080/and/a/path?with=cheese&with=onions#salt";
        URL = svURL;
        svResult = URL.Domain;
        CHECK (svResult == "[3ffe:2a00:100:7031::]");
    }

    SECTION ("KURL no schema")
    {
        KStringView svURL("my.domain.name:8080/and/a/path?with=cheese&with=onions#salt");
        KURL URL(svURL);
        CHECK (URL.Domain == "my.domain.name");
    }

    SECTION ("KURL copy assignment")
    {
        KStringView svURL;
        KURL URL1;
        KURL URL2;
        KString serialized1, serialized2;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
        URL1.Parse(svURL);
        URL1.Serialize(serialized1);
        CHECK ( serialized1 == svURL );

        URL2 = URL1;
        URL2.Serialize(serialized2);
        CHECK ( serialized1 == serialized2 );
    }

    SECTION ("KURL move assignment")
    {
        KStringView svURL;
        KURL URL1;
        KURL URL2;
        KString serialized1, serialized2;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
        URL1.Parse(svURL);
        URL1.Serialize(serialized1);
        CHECK ( serialized1 == svURL );

        URL2 = std::move(URL1);
        URL2.Serialize(serialized2);
        CHECK ( serialized1 == serialized2 );
    }

    SECTION ("KURL copy constructor")
    {
        KStringView svURL;
        KURL URL1;
        KString serialized1, serialized2;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
        URL1.Parse(svURL);
        URL1.Serialize(serialized1);
        CHECK ( serialized1 == svURL );

        KURL URL2(URL1);
        URL2.Serialize(serialized2);
        CHECK ( serialized1 == serialized2 );
    }

    SECTION ("KURL move constructor")
    {
        KStringView svURL;
        KURL URL1;
        KString serialized1, serialized2;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
        URL1.Parse(svURL);
        URL1.Serialize(serialized1);
        CHECK ( serialized1 == svURL );

        KURL URL2(std::move(URL1));
        URL2.Serialize(serialized2);
        CHECK ( serialized1 == serialized2 );
    }

    SECTION ("KURL self expressions")
    {
        KStringView svURL;
        KURL URL;
        KString sSerialized;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
        URL   = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == svURL );

        svURL = "http://news.example.com/money/$5.2-billion-merger";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://news.example.com/money/%245.2-billion-merger" );

        svURL = "https://spam.example.com/viagra-only-$2-per-pill*";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "https://spam.example.com/viagra-only-%242-per-pill%2A" );

        svURL = "http://example.com/path/foo:bogus";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://example.com/path/foo%3Abogus" );

        svURL = "http://example.com/path+test/foo%20bogus?foo+bogus%2Btest#foo%20bogus+test";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://example.com/path%2Btest/foo%20bogus?foo+bogus%2Btest#foo%20bogus%2Btest" );

    }

    SECTION("KURL new design")
    {
        KStringView svURL;
        KURL URL;
        KString sSerialized;

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
		URL = KURL(svURL);
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == svURL );

        svURL = "http://news.example.com/money/$5.2-billion-merger";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://news.example.com/money/%245.2-billion-merger" );

        svURL = "https://spam.example.com/viagra-only-$2-per-pill*";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "https://spam.example.com/viagra-only-%242-per-pill%2A" );

        svURL = "http://example.com/path/foo:bogus";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://example.com/path/foo%3Abogus" );

        svURL = "http://example.com/path+test/foo%20bogus?foo+bogus%2Btest#foo%20bogus+test";
		URL = svURL;
        sSerialized.clear();
        URL.Serialize(sSerialized);
        CHECK ( sSerialized == "http://example.com/path%2Btest/foo%20bogus?foo+bogus%2Btest#foo%20bogus%2Btest" );

        svURL = "whatever://fred:secret@www.test.com:7654/works.html;param;a=b;multi=a,b,c,d;this=that?foo=bar&you=me#fragment";
		URL = svURL;
        svURL = URL.Password;
        CHECK ( svURL == "secret" );
        KString bar = URL.Query.get().find("foo")->second;
        CHECK ( bar == "bar" );
        URL.Query.get().Set("foo", "röb");
        URL.Query->Set("you", "whø");
        URL.Path = "/changed.xml";
        URL.Protocol = "https://";
        sSerialized.clear();
        URL.Serialize(sSerialized);
        svURL = "https://fred:secret@www.test.com:7654/changed.xml?foo=r%C3%B6b&you=wh%C3%B8#fragment";
        CHECK ( sSerialized == svURL );
        CHECK ( URL.Protocol.getProtocol() == dekaf2::url::KProtocol::HTTPS );
        CHECK ( URL.Protocol == dekaf2::url::KProtocol::HTTPS );
    }

    SECTION("KURL various schemes")
    {
        KURL URL;

        URL = "http://that.server.name/with_a_path"_ksv;
        CHECK ( URL.IsHttpURL() == true );
        CHECK ( URL.Domain.get() == "that.server.name" );
        CHECK ( URL.Path.get() == "/with_a_path" );
		CHECK ( URL.Serialize() == "http://that.server.name/with_a_path" );

        URL = "log.server.my.domain:35";
        CHECK ( URL.IsHttpURL() == false );
        CHECK ( URL.Domain.get() == "log.server.my.domain" );
		CHECK ( URL.Port.get() == 35 );
		CHECK ( URL.Port == 35 );
        CHECK ( URL.Path.get() == "" );
		CHECK ( URL.Serialize() == "log.server.my.domain:35" );

		URL = "/path/to/file"_ksz;
		CHECK ( URL.IsHttpURL() == false );
		CHECK ( URL.Domain.empty() == true );
		CHECK ( URL.Path.get() == "/path/to/file" );
		CHECK ( URL.Serialize() == "/path/to/file" );

		URL = "//domain.com:35/path/to/file"_ks;
		CHECK ( URL.IsHttpURL() == true );
		CHECK ( URL.Domain.get() == "domain.com" );
		CHECK ( URL.Port.get() == 35 );
		CHECK ( URL.Path.get() == "/path/to/file" );
		CHECK ( URL.Serialize() == "//domain.com:35/path/to/file" );
    }

    SECTION("KURL ugly")
    {
        KURL URL;
        URL = "http://any.one/A.css,,_style.css+css,,_custom005.css+vendor,,_masterslider,,_style,,_masterslider.css+vendor,,_masterslider,,_skins,,_default,,_style.css+vendor,,_masterslider,,_style,,_ms-fullscreen.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.carousel.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.theme.default.css,Mcc.WnnFPiCXw1.css.pagespeed.cf.oRioQJq-Az.css";
        CHECK ( URL.IsHttpURL() == true );
        CHECK ( URL.Domain.get() == "any.one" );
        CHECK ( URL.Path.get() == "/A.css,,_style.css+css,,_custom005.css+vendor,,_masterslider,,_style,,_masterslider.css+vendor,,_masterslider,,_skins,,_default,,_style.css+vendor,,_masterslider,,_style,,_ms-fullscreen.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.carousel.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.theme.default.css,Mcc.WnnFPiCXw1.css.pagespeed.cf.oRioQJq-Az.css" );

        URL = "/A.css,,_style.css+css,,_custom005.css+vendor,,_masterslider,,_style,,_masterslider.css+vendor,,_masterslider,,_skins,,_default,,_style.css+vendor,,_masterslider,,_style,,_ms-fullscreen.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.carousel.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.theme.default.css,Mcc.WnnFPiCXw1.css.pagespeed.cf.oRioQJq-Az.css";
        CHECK ( URL.Domain.get() == "" );
        CHECK ( URL.Path.get() == "/A.css,,_style.css+css,,_custom005.css+vendor,,_masterslider,,_style,,_masterslider.css+vendor,,_masterslider,,_skins,,_default,,_style.css+vendor,,_masterslider,,_style,,_ms-fullscreen.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.carousel.css+vendor,,_OwlCarousel2-2.2.1,,_dist,,_assets,,_owl.theme.default.css,Mcc.WnnFPiCXw1.css.pagespeed.cf.oRioQJq-Az.css" );
	}
    
	SECTION("KURL broken")
	{
		KURL URL;
		URL = "/assets/heise/heise/css/heise.css?4e5ca4e953c65b217210";
		CHECK ( URL.Domain.get() == "" );
		CHECK ( URL.Serialize() == "/assets/heise/heise/css/heise.css?4e5ca4e953c65b217210" );

		URL = "/stil/ho/ho.css?4af42cac6c10ef07d3df";
		CHECK ( URL.Domain.get() == "" );
		CHECK ( URL.Serialize() == "/stil/ho/ho.css?4af42cac6c10ef07d3df" );

		URL = "/avw-bin/ivw/CP/barfoo/ho/4006197/0.gif?d=1872002745";
		CHECK ( URL.Domain.get() == "" );
		CHECK ( URL.Serialize() == "/avw-bin/ivw/CP/barfoo/ho/4006197/0.gif?d=1872002745" );
	}

	SECTION("KURL tricky")
	{
		KURL URL;
		URL = "https://localhost/Some/Path/5C86463AA7BAFCE4?url=http://some.host.com/resources/images/image1.gif";
		CHECK ( URL.Protocol.get() == "https://" );
		CHECK ( URL.Domain.get() == "localhost" );
		CHECK ( URL.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( URL.Query["url"] == "http://some.host.com/resources/images/image1.gif" );

		URL = "/Some/Path/5C86463AA7BAFCE4?url=http://some.host.com/resources/images/image1.gif";
		CHECK ( URL.Protocol.get() == "" );
		CHECK ( URL.Domain.get() == "" );
		CHECK ( URL.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( URL.Query["url"] == "http://some.host.com/resources/images/image1.gif" );

		URL = "//localhost/Some/Path/5C86463AA7BAFCE4?url=http://some.host.com/resources/images/image1.gif";
		CHECK ( URL.Protocol.get() == "//" );
		CHECK ( URL.Domain.get() == "localhost" );
		CHECK ( URL.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( URL.Query["url"] == "http://some.host.com/resources/images/image1.gif" );
	}

	SECTION("KResource")
	{
		KResource Resource;
		KURL URL;
		URL = "https://user:pass@localhost:876/Some/Path/5C86463AA7BAFCE4";
		Resource = URL;
		CHECK ( Resource.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( Resource.Serialize() == "/Some/Path/5C86463AA7BAFCE4" );
		Resource = "https://user:pass@localhost:876/Some/Path/5C86463AA7BAFCE4";
		CHECK ( Resource.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( Resource.Serialize() == "/Some/Path/5C86463AA7BAFCE4" );
		Resource = "/Some/Path/5C86463AA7BAFCE4";
		CHECK ( Resource.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( Resource.Serialize() == "/Some/Path/5C86463AA7BAFCE4" );
		Resource = "Some/Path/5C86463AA7BAFCE4";
		CHECK ( Resource.Serialize() == "/Path/5C86463AA7BAFCE4" );
		Resource = "Some";
		CHECK ( Resource.Serialize() == "" );
		Resource = "/Some";
		CHECK ( Resource.Serialize() == "/Some" );
		Resource = "/Some/";
		CHECK ( Resource.Serialize() == "/Some/" );
		Resource = "https://localhost/Some/Path/5C86463AA7BAFCE4?url=http://some.host.com/resources/images/image1.gif";
		CHECK ( Resource.Path.get() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( Resource.Serialize() == "/Some/Path/5C86463AA7BAFCE4?url=http%3A//some.host.com/resources/images/image1.gif" );
	}

	SECTION("decoded / encoded")
	{
		KURL URL;
		URL = "https://user:pass@local.org:876/Some/Path/5C86463AA7BAFCE4?url=http%3A%2F%2Fsome%2Ehost%2Ecom%2Fresources%2Fimages%2Fimage1%2Egif#fragment%2Fhere";
		CHECK ( URL.Path.Decoded() == "/Some/Path/5C86463AA7BAFCE4" );
		CHECK ( URL.Domain.Decoded() == "local.org" );
		CHECK ( URL.Protocol.Decoded() == "https://" );
		CHECK ( URL.Query.Decoded() == "url=http%3A//some.host.com/resources/images/image1.gif" );
		CHECK ( URL.Fragment.Decoded() == "fragment/here" );
		CHECK ( URL.User.Decoded() == "user" );
		CHECK ( URL.Password.Decoded() == "pass" );
	}

	SECTION("is subdomain")
	{
		url::KDomain Domain("my.domain");
		CHECK ( kIsSubDomainOf(Domain, url::KDomain("www.my.domain")) == true );
		CHECK ( kIsSubDomainOf(Domain, url::KDomain(".my.domain")) == true );
		CHECK ( kIsSubDomainOf(Domain, url::KDomain("my.domain")) == true );
		CHECK ( kIsSubDomainOf(Domain, url::KDomain("www.your.domain")) == false );
		CHECK ( kIsSubDomainOf(Domain, url::KDomain("www.my.dot")) == false );
		CHECK ( kIsSubDomainOf(Domain, url::KDomain("www.MY.DoMain")) == true );
	}

	SECTION("auto range")
	{
		KURL URL = "http://www.test.com/path?parm1=val1&parm2=val2&parm3=val3"_ksv;
		KString sOut;

		for (const auto& it : URL.Query)
		{
			if (!sOut.empty())
			{
				sOut += '|';
			}
			sOut += it.first;
			sOut += '-';
			sOut += it.second;
		}

		CHECK ( sOut == "parm1-val1|parm2-val2|parm3-val3" );
	}
}

TEST_CASE ("KURL regression tests")
{
	KURL URL("http://third.second-level.com/path/TomatoJuice/2B66-3D@77-AC7F-EE58");
	KURL URL2 = URL;

	KTCPEndPoint EndPoint = URL;
	KTCPEndPoint EndPoint2 = EndPoint;

	KResource Resource("/this/is/a/path?with=query&parms=last");
	KResource Resource2 = Resource;
	KResource Resource3("/this/is/a/path?with=query&parms=later");

	CHECK       ( URL == URL2 );
	CHECK_FALSE ( URL != URL2 );
	CHECK_FALSE ( URL  < URL2 );
	CHECK_FALSE ( URL  > URL2 );
	CHECK       ( URL == "http://third.second-level.com/path/TomatoJuice/2B66-3D@77-AC7F-EE58"_ksv );

	CHECK       ( EndPoint == EndPoint2 );
	CHECK_FALSE ( EndPoint != EndPoint2 );
	CHECK_FALSE ( EndPoint  < EndPoint2 );
	CHECK_FALSE ( EndPoint  > EndPoint2 );

	CHECK       ( Resource == Resource2 );
	CHECK_FALSE ( Resource != Resource2 );
	CHECK_FALSE ( Resource  < Resource2 );
	CHECK_FALSE ( Resource  > Resource2 );
	CHECK       ( Resource  < Resource3 );
	CHECK_FALSE ( Resource == Resource3 );
	CHECK       ( Resource != Resource3 );
	CHECK_FALSE ( Resource  > Resource3 );

	CHECK ( URL.Protocol  == URL2.Protocol );
	CHECK ( URL.Protocol.Decoded()  == "http://" );
	CHECK ( URL.Protocol  == "http://"     );
	CHECK ( URL.Protocol  == "http://"_ks  );
	CHECK ( URL.Protocol  == "http://"_ksv );
	CHECK ( URL.Protocol  == "http://"_ksz );
	CHECK ( "http://"     == URL.Protocol );
	CHECK ( "http://"_ks  == URL.Protocol );
	CHECK ( "http://"_ksv == URL.Protocol );
	CHECK ( "http://"_ksz == URL.Protocol );
	CHECK ( URL.Protocol  != "https://" );
	CHECK ( URL.Protocol  == url::KProtocol("http://") );

//	CHECK ( URL.Protocol  == std::string("http://") );      // we cannot offer this implicit comparison
//	CHECK ( URL.Protocol  == std::string_view("http://") ); // we cannot offer this implicit comparison

	CHECK_FALSE ( URL.Protocol  != URL2.Protocol );
	CHECK_FALSE ( URL.Protocol  != "http://" );
	CHECK_FALSE ( URL.Protocol  != "http://"_ksv );
	CHECK_FALSE ( URL.Protocol  != "http://"_ksz );
	CHECK_FALSE ( "http://"     != URL.Protocol );
	CHECK_FALSE ( "http://"_ksv != URL.Protocol );
	CHECK_FALSE ( "http://"_ksz != URL.Protocol );
	CHECK_FALSE ( URL.Protocol  == "https://" );
	CHECK_FALSE ( URL.Protocol  != url::KProtocol("http://") );

	CHECK ( URL.Domain == "third.second-level.com" );
	CHECK ( URL.Domain == "third.second-level.com"_ksv );
	CHECK ( URL.Domain == "third.second-level.com"_ksz );
	CHECK ( URL.Domain == "third.second-level.com"_ks );
	CHECK ( "third.second-level.com"     == URL.Domain );
	CHECK ( "third.second-level.com"_ks  == URL.Domain );
	CHECK ( "third.second-level.com"_ksv == URL.Domain );
	CHECK ( URL.Domain == std::string("third.second-level.com") );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	CHECK ( URL.Domain == std::string_view("third.second-level.com") );
#endif
	CHECK ( URL.Domain == url::KDomain("third.second-level.com") );

	CHECK_FALSE ( URL.Domain != "third.second-level.com" );
	CHECK_FALSE ( URL.Domain != "third.second-level.com"_ksv );
	CHECK_FALSE ( URL.Domain != "third.second-level.com"_ksz );
	CHECK_FALSE ( URL.Domain != "third.second-level.com"_ks );
	CHECK_FALSE ( "third.second-level.com"     != URL.Domain );
	CHECK_FALSE ( "third.second-level.com"_ks  != URL.Domain );
	CHECK_FALSE ( "third.second-level.com"_ksv != URL.Domain );
	CHECK_FALSE ( URL.Domain != std::string("third.second-level.com") );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	CHECK_FALSE ( URL.Domain != std::string_view("third.second-level.com") );
#endif
	CHECK_FALSE ( URL.Domain != url::KDomain("third.second-level.com") );

	CHECK ( URL.Domain < url::KDomain("zzz.third-level.com") );
	CHECK ( URL.Domain <= url::KDomain("zzz.third-level.com") );
	CHECK ( url::KDomain("zzz.third-level.com") > URL.Domain );

	CHECK ( URL.Path == "/path/TomatoJuice/2B66-3D@77-AC7F-EE58" );
	CHECK ( URL.Path == url::KPath("/path/TomatoJuice/2B66-3D@77-AC7F-EE58") );
	CHECK ( URL.Path != url::KPath("/no/TomatoJuice/2B66-3D@77-AC7F-EE58") );

	URL = "http://third.second-level.com/path/TomatoJuice/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "http://" );
	CHECK ( URL.Domain == "third.second-level.com" );
	CHECK ( URL.Domain != "some.second-level.com" );
	CHECK ( URL.Path == "/path/TomatoJuice/2B66-3D@77-AC7F-EE58" );
	CHECK ( URL.Path != "/no/TomatoJuice/2B66-3D@77-AC7F-EE58" );

	URL = "level.com/path/TomatoJuice/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "" );
	CHECK ( URL.Domain == "level.com" );
	CHECK ( URL.Path == "/path/TomatoJuice/2B66-3D@77-AC7F-EE58" );

	URL = "/path/TomatoJuice/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "" );
	CHECK ( URL.Domain == "" );
	CHECK ( URL.Path == "/path/TomatoJuice/2B66-3D@77-AC7F-EE58" );

	URL = "/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "" );
	CHECK ( URL.Domain == "" );
	CHECK ( URL.Path == "/2B66-3D@77-AC7F-EE58" );

	URL = "2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "" );
	CHECK ( URL.User == "2B66-3D" );
	CHECK ( URL.Domain == "77-AC7F-EE58" );
	CHECK ( URL.Path == "" );

	URL = "http://third.second-level.com/path/T?om=ato#Juice/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "http://" );
	CHECK ( URL.Domain == "third.second-level.com" );
	CHECK ( URL.Path == "/path/T" );
	CHECK ( URL.Query["om"] == "ato" );
	CHECK ( URL.Fragment == "Juice/2B66-3D@77-AC7F-EE58" );

	URL = "http://Tom:Jerry@third.second-level.com/path/TomatoJuice/2B66-3D@77-AC7F-EE58";
	CHECK ( URL.Protocol == "http://" );
	CHECK ( URL.User == "Tom" );
	CHECK ( URL.Password == "Jerry" );
	CHECK ( URL.Domain == "third.second-level.com" );
	CHECK ( URL.Path == "/path/TomatoJuice/2B66-3D@77-AC7F-EE58" );
}

TEST_CASE ("KURLPort")
{
	{
		url::KPort port("123");
		CHECK ( port == 123 );
	}
	{
		url::KPort port(333);
		CHECK ( port == 333 );
	}
	url::KPort port = 123;
	port = 1234;
	port = "12345";
	uint16_t iPort = port;
	CHECK ( iPort == 12345 );
	KString sPort = port.Serialize();
	CHECK ( sPort == "12345" );

	KURL URL = "http://test.com:12345/path/to";
	CHECK ( URL.Port == 12345 );
	URL.Port = 54321;
	CHECK ( URL.Serialize() == "http://test.com:54321/path/to" );
}
