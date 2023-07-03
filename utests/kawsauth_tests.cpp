#include "catch.hpp"
#include <dekaf2/kawsauth.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kencode.h>


using namespace dekaf2;

TEST_CASE("KAWSAuth")
{
	static constexpr KStringView sAccessKey   = "AIHDSFNHCSAPJAKCUSHVF";
	static constexpr KStringView sSecretKey   = "odjfgKJHFAEsdgkdfjg+skKJJkjsgKGTFCls";
	static constexpr KStringView sURL         = "https://translate.us-east-1.amazonaws.com";
	static constexpr KStringView sContentType = "application/x-amz-json-1.1";
	static constexpr KStringView sPostRequest = R"({
  "SourceLanguageCode": "en",
  "TargetLanguageCode": "de",
  "Text": "This is a translation request in English."
 })";

	SECTION("SignatureKey")
	{
		auto sSignatureKey = AWSAuth4::SignatureKey(sSecretKey, "20230703", "us-east-1", "translate");
		CHECK ( KEncode::Hex(sSignatureKey) == "49bfc97c7ee5978260c3884c8dc25c9909740ec6d9fbf6d2638d1f492db9e7e6" );
	}

	SECTION("SignedRequest")
	{
		AWSAuth4::SignedRequest Request(sURL,
										"POST",
										"TranslateText",
										sPostRequest,
										sContentType,
										{},
										"20230703T115234Z");

		auto Headers = Request.Authorize(sAccessKey,
										 sSecretKey,
										 "us-east-1",
										 "translate");

		CHECK ( Headers.size() == 5 );
		CHECK ( Headers["Host"         ] == "translate.us-east-1.amazonaws.com" );
		CHECK ( Headers["Content-Type" ] == sContentType                        );
		CHECK ( Headers["X-Amz-Date"   ] == "20230703T115234Z"                  );
		CHECK ( Headers["X-Amz-Target" ] == "TranslateText"                     );
		CHECK ( Headers["Authorization"] ==
			   "AWS4-HMAC-SHA256 Credential=AIHDSFNHCSAPJAKCUSHVF/20230703/us-east-1/translate/aws4_request, "
			   "SignedHeaders=content-type;host;x-amz-date;x-amz-target, "
			   "Signature=53e58bf1bec7f31e2018bc052a916a4a495c3143e0a9af6fcc7e03822051e362" );
	}
}
