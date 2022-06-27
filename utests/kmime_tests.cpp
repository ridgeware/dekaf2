#include "catch.hpp"

#include <dekaf2/kmime.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/koutstringstream.h>
#include <dekaf2/kjson.h>
#include <vector>

using namespace dekaf2;

namespace {

#ifndef DEKAF2_HAS_CPP_14
KString Normalized(KStringView sInput)
{
	KString sOut { sInput };
	sOut.Replace("\r\n", "\n");
	return sOut;
}
#endif

void testMime(const KMIME& mime)
{
}

} // end of anonymous namespace

TEST_CASE("KMIME")
{
	SECTION("assignment")
	{
		KMIME a;
		a = "abc";
		a = "abc"_ks;
		a = "abc"_ksv;
		a = "abc"_ksz;
		a = std::string("abc");

		testMime("abc"_ks);
		testMime("abc"_ksv);
		testMime("abc"_ksz);
	}

	SECTION("by extension")
	{
		KMIME a;

		CHECK( a.ByExtension("test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("/folder/test.dir/test.txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension("txt") );
		CHECK( a == KMIME::TEXT_UTF8 );

		CHECK( a.ByExtension(".html") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension(".HTML") );
		CHECK( a == KMIME::HTML_UTF8 );

		CHECK( a.ByExtension("css") );
		CHECK( a == KMIME::CSS );

	}

	SECTION("parts")
	{
		KMIME mime="application/vnd.api+json";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json"        );
		CHECK ( mime.Parameter() == ""            );

		mime="application/vnd.api+json+xml";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == ""            );

		mime="application/vnd.api+json+xml;  type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime="application/vnd.second.api+json+xml;  type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd.second"  );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime="application/vnd.api+json+xml;type=any";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "api"         );
		CHECK ( mime.Tree()      == "vnd"         );
		CHECK ( mime.Suffix()    == "json+xml"    );
		CHECK ( mime.Parameter() == "type=any"    );

		mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "document"    );
		CHECK ( mime.Tree()      == "vnd.openxmlformats-officedocument.wordprocessingml" );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == ""            );

		mime = "image/svg+xml";

		CHECK ( mime.Type()      == "image"       );
		CHECK ( mime.SubType()   == "svg"         );
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == "xml"         );
		CHECK ( mime.Parameter() == ""            );

		mime = "text/html; charset=UTF-8";

		CHECK ( mime.Type()      == "text"        );
		CHECK ( mime.SubType()   == "html"        );
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == "charset=UTF-8" );

		mime = "application/x-www-form-urlencoded";

		CHECK ( mime.Type()      == "application" );
		CHECK ( mime.SubType()   == "x-www-form-urlencoded");
		CHECK ( mime.Tree()      == ""            );
		CHECK ( mime.Suffix()    == ""            );
		CHECK ( mime.Parameter() == ""            );

		mime = "text/sgml.html+xml; charset=UTF-8";

		CHECK ( mime.Type()      == "text"        );
		CHECK ( mime.SubType()   == "html"        );
		CHECK ( mime.Tree()      == "sgml"        );
		CHECK ( mime.Suffix()    == "xml"         );
		CHECK ( mime.Parameter() == "charset=UTF-8" );
	}

// C++11 has problems destroying the KMIMEMultiPartFormData ..
#ifndef DEKAF2_HAS_CPP_14
	SECTION("KMIMEMultiPart")
	{
		KMIMEMultiPartFormData Parts;
		Parts += KMIMEText("TheName", "TheValue");
		Parts += KMIMEFile("", "This is file1 Русский\nWith line2\n", "file1.txt", KMIME::TEXT_UTF8);
		Parts += KMIMEFile("", "This is file2 Русский\nwith line2\n", "file2.jpg");
		Parts += KMIMEPart(KJSON
						   {
							{ "message", "important"               },
							{ "parts"  , { "one", "two", "three" } }
						   }.dump(),
						   KMIME::JSON);
		// serialize for SMTP
		auto sFormData = Normalized(Parts.Serialize());
		sFormData.ReplaceRegex("_KMIME_Part_[0-9]+_[0-9]+\\.[0-9]+-", "_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]-");
		KString sExpected1 = Normalized(R"(Content-Type: multipart/form-data;
 boundary="----=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----"

------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable
Content-Disposition: form-data; name="TheName"

TheValue
------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: quoted-printable
Content-Disposition: form-data; filename="file1.txt"

This is file1 =D0=A0=D1=83=D1=81=D1=81=D0=BA=D0=B8=D0=B9
With line2

------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: image/jpeg
Content-Transfer-Encoding: base64
Content-Disposition: form-data; filename="file2.jpg"

VGhpcyBpcyBmaWxlMiDQoNGD0YHRgdC60LjQuQp3aXRoIGxpbmUyCg==
------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: application/json
Content-Transfer-Encoding: base64
Content-Disposition: inline

eyJtZXNzYWdlIjoiaW1wb3J0YW50IiwicGFydHMiOlsib25lIiwidHdvIiwidGhyZWUiXX0=
------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]------
)");
		CHECK ( sFormData == sExpected1 );

		sFormData = Normalized(Parts.Serialize(true));
		KString sContentType = Parts.ContentType();
		KString sExpectedContentType = Normalized((R"(multipart/form-data; boundary=----=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----)"));
		KString sExpected2 = Normalized(R"(------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: text/plain; charset=UTF-8
Content-Disposition: form-data; name="TheName"

TheValue
------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: text/plain; charset=UTF-8
Content-Disposition: form-data; filename="file1.txt"

This is file1 Русский
With line2

------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: image/jpeg
Content-Disposition: form-data; filename="file2.jpg"

This is file2 Русский
with line2

------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----
Content-Type: application/json
Content-Disposition: inline

{"message":"important","parts":["one","two","three"]}
------=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]------
)");
		sContentType.ReplaceRegex("_KMIME_Part_[0-9]+_[0-9]+\\.[0-9]+-", "_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]-");
		sFormData   .ReplaceRegex("_KMIME_Part_[0-9]+_[0-9]+\\.[0-9]+-", "_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]-");
		CHECK ( sContentType == sExpectedContentType );
		CHECK ( sFormData    == sExpected2 );
	}
#endif // (CPP 11)
}
