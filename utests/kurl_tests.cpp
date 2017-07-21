#include <map>
#include <tuple>

#include "dekaf.h"
#include "catch.hpp"
#include "kurl.h"

using namespace dekaf2;
using std::get;
using std::map;
using std::tuple;

TEST_CASE ("KURL")
{
	//            hint    final   flag
	//            get<0>  get<1>  get<2> get<3>
	typedef tuple<size_t, size_t, bool,  KString> parm_t;
	typedef map<KString, parm_t> test_t;
	// get<3> is unused but could be used if catch.hpp has output support.
	// It is used to identify qualities of URL analyzed by KURL and friends.

	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// These are examples of valid URLs
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

	// simple URLs
	test_t URL_valid;

	URL_valid["a://b.c"] =
	    parm_t (0, 7, true, "minimum valid URL");
	URL_valid["http://google.com"] =
	    parm_t (0, 17, true, "minimum sensible URL");
	URL_valid["http://subsub.sub.go.co.jp"] =
	    parm_t (0, 26, true, "valid URL with .co.");

	// user:pass in simple URLs
	URL_valid["https://jlettvin@mail.google.com"] =
	    parm_t (0, 32, true, "user@host");
	URL_valid["https://user:password@mail.google.com"] =
	    parm_t (0, 37, true, "user:pass@host");
	URL_valid["opaquelocktoken://user:password@point.domain.pizza"] =
	    parm_t (0, 50, true, "unusual scheme");

	// query string in simple URL
	URL_valid["http://foo.bar?hello=world"] =
	    parm_t (0, 26, true, "query on simple URL");

	// fragment in simple URL
	URL_valid["http://sushi.bar#uni"] =
	    parm_t (0, 20, true, "fragment on simple URL");

	// scheme/user/pass/domain/query/fragment
	URL_valid["https://user:password@what.ever.com?please=stop#now"] =
	    parm_t (0, 51, true, "all URL elements in use");
	// TODO This next one fails.  Fix it.
	//URL_valid["a://b.c:1/d?e=f#g"] =
	    //parm_t (0, 17, true, "minimum valid fully populated URL");


	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// These are examples of valid URLs
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

	test_t URL_invalid;
	URL_invalid["file:///home/jlettvin/.bashrc"] =
	    parm_t (0, 0, false, "slash before domain");

	// TODO fix this one too.
	//URL_invalid["http://home/jlettvin/.bashrc"] =
	    //parm_t (0, 0, false, "no TLD (Top Level Domain)");

	URL_invalid["I Can Has Cheezburger?"] =
	    parm_t (0, 0, false, "Garbage text");

	URL_invalid["\x01\x02\x03\x04\xa0\xa1\xa2\xfd\xfe\xff"] =
	    parm_t (0, 0, false, "Non-ASCII");


	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	// Data for tests designed early
	//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

	    KString sProto{"https://"};
		KString sUser{"jlettvin:password@"};
		KString sDomain{"jlettvin.github.com:80"};
		KString sPath{"/foo/bar"};
		KString sQuery{"?baz=beef"};
		KString sFragment{"#fun"};

		KString source{sProto+sUser+sDomain}; //+sPath+sQuery+sFragment};

		SECTION ("KProto simple test")
		{
			    KString target;
				KString expect("https://");
				size_t hint{0};

				KProto kproto (source, hint);

				kproto.serialize (target);

				CHECK (target == expect);
				CHECK (hint == 8);
		}

		SECTION ("KProto/KUserInfo/KDomain simple test")
		{
			    KString target;
				KString& expect{source};
				size_t hint{0};

				KProto    kproto  (source, hint);
				KUserInfo kuser   (source, hint);
				KDomain   kdomain (source, hint);

				kproto .serialize (target);
				kuser  .serialize (target);
				kdomain.serialize (target);

				CHECK (target == expect);
				CHECK (hint == expect.size());
		}

		SECTION ("KProto/KDomain hint offset")
		{
			    KString target;
				KString& expect{sDomain};
				size_t hint{0};

				KDomain kdomain (sDomain, hint);

				kdomain.serialize (target);

				CHECK (target == expect);
				//CHECK (hint == 41);
		}

		//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		// SOLO unit tests
		//||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

		SECTION ("KProto solo unit (pass https://)")
		{
			KString solo ("https://");
			KString expect{solo};
			KString target;
			size_t hint{0};

			KProto kproto (solo, hint);

			bool ret = kproto.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == target.size());
		}

		SECTION ("KProto solo unit (fail missing slash)")
		{
			KString solo ("https:/");
			KString expect{""};
			KString target{""};
			size_t hint{0};

			KProto kproto (solo, hint);

			bool ret = kproto.serialize (target);

			CHECK (ret == false);
			CHECK (target == expect);
			CHECK (hint == 0);
		}

		SECTION ("KProto solo unit (fail missing ://)")
		{
			KString solo ("https");
			KString expect{""};
			KString target{""};
			size_t hint{0};

			KProto kproto (solo, hint);

			bool ret = kproto.serialize (target);

			CHECK (ret == false);
			CHECK (target == expect);
			CHECK (hint == 0);
		}

		SECTION ("KProto solo unit (pass mailto:)")
		{
			KString solo ("mailto:");
			KString expect{"mailto:"};
			KString target{""};
			size_t hint{0};

			KProto kproto (solo, hint);

			bool ret = kproto.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == expect.size());
		}

		SECTION ("KUserInfo solo unit (pass foo:bar@)")
		{
			KString solo ("foo:bar@");
			KString expect{"foo:bar@"};
			KString target{""};
			size_t hint{0};

			KUserInfo kuserinfo (solo, hint);

			bool ret = kuserinfo.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == expect.size());
		}

		SECTION ("KUserInfo solo unit (pass http://foo:bar@) at offset 7")
		{
			KString solo ("http://foo:bar@");
			KString expect{"foo:bar@"};
			KString target{""};
			size_t hint{7};

			KUserInfo kuserinfo (solo, hint);

			bool ret = kuserinfo.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == solo.size());
		}

		SECTION ("KUserInfo solo unit (pass http://foo.bar@) at offset 7")
		{
			KString solo ("http://foo.bar@");
			KString expect{"foo.bar@"};
			KString target{""};
			size_t hint{7};

			KUserInfo kuserinfo (solo, hint);

			bool ret = kuserinfo.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == solo.size());
		}

		SECTION ("KUserInfo solo unit (fail http://google.com) at offset 7")
		{
			KString solo ("http://google.com");
			KString expect{""};
			KString target{""};
			size_t hint{7};

			KUserInfo kuserinfo (solo, hint);

			bool ret = kuserinfo.serialize (target);

			CHECK (ret == true);
			CHECK (target == expect);
			CHECK (hint == 7);
		}

		SECTION ("KURL bulk valid tests")
		{
			test_t::iterator it;
			for (it = URL_valid.begin(); it != URL_valid.end(); ++it)
			{
				const KString& source = it->first;
				parm_t&  parameter = it->second;
				KString  expect{source};
				KString  target{""};
				size_t   hint{get<0>(parameter)};
				size_t   done{get<1>(parameter)};
				bool     want{get<2>(parameter)};

				KURL kurl (source, hint);
				bool have{kurl.serialize (target)};

				if (want != have || target != expect || done != hint)
				{
					CHECK (source == expect);
				}
				CHECK (want   == have  );
				CHECK (target == expect);
				CHECK (done   == hint  );
			}
		}

		SECTION ("KURL bulk invalid tests")
		{
			test_t::iterator it;
			for (it = URL_invalid.begin(); it != URL_invalid.end(); ++it)
			{
				const KString& source = it->first;
				parm_t&  parameter = it->second;
				KString  expect{""};
				KString  target{""};
				size_t   hint		{get<0>(parameter)};
				size_t   done		{get<1>(parameter)};
				bool     want		{get<2>(parameter)};
				KString  feature	{get<3>(parameter)};

				KURL kurl (source, hint);
				bool have{kurl.serialize (target)};

				if (want != have || target != expect || done != hint)
				{
					CHECK (source == expect);
				}
				CHECK (want   == have  );
				CHECK (target == expect);
				CHECK (done   == hint  );
			}
		}

}
