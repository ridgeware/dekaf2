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

	SECTION("SignedRequest 1")
	{
		AWSAuth4::SignedRequest Request(sURL,
										"POST",
										"TranslateText",
										sPostRequest,
										sContentType,
										{},
										{},
										"20230703T115234Z");

		auto Headers = Request.Authorize(sAccessKey,
										 sSecretKey,
										 "us-east-1",
										 "translate");

		CHECK ( Headers.size() == 5 );
		CHECK ( Headers["host"         ] == "translate.us-east-1.amazonaws.com" );
		CHECK ( Headers["content-type" ] == sContentType                        );
		CHECK ( Headers["x-amz-date"   ] == "20230703T115234Z"                  );
		CHECK ( Headers["x-amz-target" ] == "TranslateText"                     );
		CHECK ( Headers["authorization"] ==
			   "AWS4-HMAC-SHA256 Credential=AIHDSFNHCSAPJAKCUSHVF/20230703/us-east-1/translate/aws4_request, "
			   "SignedHeaders=content-type;host;x-amz-date;x-amz-target, "
			   "Signature=53e58bf1bec7f31e2018bc052a916a4a495c3143e0a9af6fcc7e03822051e362" );
	}

	SECTION("SignedRequest 2")
	{
		AWSAuth4::SignedRequest Request(sURL,
										"POST",
										"TranslateText",
										sPostRequest,
										{},
										{},
										{},
										"20230703T115234Z");

		auto Headers = Request.Authorize(sAccessKey,
										 sSecretKey);

		CHECK ( Headers.size() == 5 );
		CHECK ( Headers["host"         ] == "translate.us-east-1.amazonaws.com" );
		CHECK ( Headers["content-type" ] == sContentType                        );
		CHECK ( Headers["x-amz-date"   ] == "20230703T115234Z"                  );
		CHECK ( Headers["x-amz-target" ] == "TranslateText"                     );
		CHECK ( Headers["authorization"] ==
			   "AWS4-HMAC-SHA256 Credential=AIHDSFNHCSAPJAKCUSHVF/20230703/us-east-1/translate/aws4_request, "
			   "SignedHeaders=content-type;host;x-amz-date;x-amz-target, "
			   "Signature=53e58bf1bec7f31e2018bc052a916a4a495c3143e0a9af6fcc7e03822051e362" );
	}

	SECTION("SignedRequest 3")
	{
		AWSAuth4::SignedRequest Request(sURL,
										"POST",
										"TranslateText",
										sPostRequest,
										{},
										"goog",
										{},
										"20230703T115234Z");

		auto Headers = Request.Authorize(sAccessKey,
										 sSecretKey);

		CHECK ( Headers.size() == 5 );
		CHECK ( Headers["host"         ] == "translate.us-east-1.amazonaws.com" );
		CHECK ( Headers["content-type" ] == "application/x-goog-json-1.1"       );
		CHECK ( Headers["x-goog-date"  ] == "20230703T115234Z"                  );
		CHECK ( Headers["x-goog-target"] == "TranslateText"                     );
		CHECK ( Headers["authorization"] ==
			   "GOOG4-HMAC-SHA256 Credential=AIHDSFNHCSAPJAKCUSHVF/20230703/us-east-1/translate/goog4_request, "
			   "SignedHeaders=content-type;host;x-goog-date;x-goog-target, "
			   "Signature=70d14cd2d0ec0422013d564f5d3170b29854e91dbd3a79b7981602c64c7b576b" );
	}
}
