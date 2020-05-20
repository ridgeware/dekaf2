#include "catch.hpp"

#include <dekaf2/kmime.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/koutstringstream.h>
#include <dekaf2/kjson.h>
#include <vector>

using namespace dekaf2;

KString Normalized(KStringView sInput)
{
	KString sOut { sInput };
	sOut.Replace("\r\n", "\n");
	return sOut;
}

TEST_CASE("KMIME")
{
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

	SECTION("KMIMEMultiPart")
	{
		KMIMEMultiPartFormData Parts;
		Parts += KMIMEFormData("TheName", "TheValue");
		Parts += KMIMEFile("This is file1 Русский\nWith line2\n", "file1.txt", KMIME::TEXT_UTF8);
		Parts += KMIMEFile("This is file2 Русский\nwith line2\n", "file2.jpg");
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

		KHTTPHeaders Headers;
		sFormData = Normalized(Parts.Serialize(&Headers));
		KString sHeaders;
		KOutStringStream oss(sHeaders);
		Headers.Serialize(oss);
		KString sExpectedHeaders = Normalized((R"(Content-Type: multipart/form-data; boundary="----=_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]----"

)"));
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
		sHeaders .ReplaceRegex("_KMIME_Part_[0-9]+_[0-9]+\\.[0-9]+-", "_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]-");
		sFormData.ReplaceRegex("_KMIME_Part_[0-9]+_[0-9]+\\.[0-9]+-", "_KMIME_Part_[SEQ]_[RANDOM1].[RANDOM2]-");
		CHECK ( sHeaders  == sExpectedHeaders );
		CHECK ( sFormData == sExpected2 );


	}
}
