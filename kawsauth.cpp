#include "kawsauth.h"
#include "khmac.h"
#include "kmessagedigest.h"
#include "kstringutils.h"
#include "kurlencode.h"

namespace dekaf2 {

namespace {

//---------------------------------------------------------------------------
KString SHA256HexDigest(KStringView sBuffer)
//---------------------------------------------------------------------------
{
	return KMessageDigest(KMessageDigest::SHA256, sBuffer).HexDigest();
}

//---------------------------------------------------------------------------
KString SHA256HMACDigest(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	return KHMAC(KHMAC::SHA256, sKey, sMessage).Digest();
}

//---------------------------------------------------------------------------
KString SHA256HMACHexDigest(KStringView sKey, KStringView sMessage)
//---------------------------------------------------------------------------
{
	return KHMAC(KHMAC::SHA256, sKey, sMessage).HexDigest();
}

} // end of anonymous namespace

namespace AWSAuth4 {

//---------------------------------------------------------------------------
KString SignatureKey(KStringView sKey, KStringView sDateStamp,
					 KStringView sRegion, KStringView sService)
//---------------------------------------------------------------------------
{
	KString sInit{"AWS4"};
	sInit += sKey;
	auto skDate    = SHA256HMACDigest(sInit,    sDateStamp);
	auto skRegion  = SHA256HMACDigest(skDate,   sRegion   );
	auto skService = SHA256HMACDigest(skRegion, sService  );
	return SHA256HMACDigest(skService, "aws4_request");

} // SignatureKey

//---------------------------------------------------------------------------
SignedRequest::SignedRequest(const KURL& URL,
							 KHTTPMethod Method,       // e.g. "GET"
							 KStringView sTarget,      // e.g. TranslateDocument
							 KStringView sPayload,     // e.g. ""
							 KStringView sContentType,
							 const HTTPHeaders& AdditionalSignedHeaders,
							 KString     sDateTime)
//---------------------------------------------------------------------------
{
	m_sDateTime = std::move(sDateTime);
	// and produce a date only string
	m_sDate = m_sDateTime.substr(0, 8);

	m_sHost = URL.Domain.Serialize();
	m_sHost.Trim();

	KString sCanonicalRequest;

	sCanonicalRequest = Method.Serialize();
	sCanonicalRequest += '\n';
	sCanonicalRequest += URL.Path.empty() ? "/" : URL.Path.Serialize();
	sCanonicalRequest += '\n';

	{
		// we cannot simply use the serialization of kurl::KQueryParms,
		// because AWS wants them sorted AFTER percent-encoding, AND
		// wants spaces encoded as %20 and not as +
		std::map<KString, KString> EncodedQuery;
		for (auto& it : URL.Query)
		{
			EncodedQuery.insert(std::map<KString, KString>::value_type{
				kUrlEncode(it.first),
				kUrlEncode(it.second)
			});
		}

		bool bIsFirst = true;
		for (const auto& Query : EncodedQuery)
		{
			if (!bIsFirst)
			{
				sCanonicalRequest += '&';
			}
			else
			{
				bIsFirst = false;
			}
			sCanonicalRequest += Query.first;
			sCanonicalRequest += '=';
			sCanonicalRequest += Query.second;
		}
		sCanonicalRequest += '\n';
	}

	HTTPHeaders LowerCaseHeaders;
	for (auto it = AdditionalSignedHeaders.begin(); it != AdditionalSignedHeaders.end(); ++it)
	{
		// lowercase the header and collapse the value, it must be resorted after
		// encoding..
		auto sKey   = it->first.ToLower().Trim();
		auto sValue = it->second;
		sValue.CollapseAndTrim();
		LowerCaseHeaders.insert(HTTPHeaders::value_type{std::move(sKey), std::move(sValue)});
	}

	LowerCaseHeaders.insert(HTTPHeaders::value_type { "content-type", sContentType });
	LowerCaseHeaders.insert(HTTPHeaders::value_type { "host"        , m_sHost      });
	LowerCaseHeaders.insert(HTTPHeaders::value_type { "x-amz-date"  , m_sDateTime  });
	LowerCaseHeaders.insert(HTTPHeaders::value_type { "x-amz-target", sTarget      });

	m_AddedHeaders  .insert(HTTPHeaders::value_type { "Content-Type", sContentType });
	m_AddedHeaders  .insert(HTTPHeaders::value_type { "Host"        , m_sHost      });
	m_AddedHeaders  .insert(HTTPHeaders::value_type { "X-Amz-Date"  , m_sDateTime  });
	m_AddedHeaders  .insert(HTTPHeaders::value_type { "X-Amz-Target", sTarget      });

	for (const auto& Header : LowerCaseHeaders)
	{
		sCanonicalRequest += Header.first;
		sCanonicalRequest += ':';
		sCanonicalRequest += Header.second;
		sCanonicalRequest += '\n';
	}
	sCanonicalRequest += '\n';

	for (const auto& Header : LowerCaseHeaders)
	{
		if (!m_sSignedHeaders.empty()) m_sSignedHeaders += ';';
		m_sSignedHeaders += Header.first;
	}

	sCanonicalRequest += m_sSignedHeaders;
	sCanonicalRequest += '\n';
	sCanonicalRequest += SHA256HexDigest(sPayload);

	m_sCanonicalRequestDigest = SHA256HexDigest(sCanonicalRequest);

} // ctor

//---------------------------------------------------------------------------
const HTTPHeaders& SignedRequest::Authorize(KStringView sAccessKey,
											KStringView sSecretKey,
											KStringView sRegion,
											KStringView sService)
//---------------------------------------------------------------------------
{
	KString sCredentialScope = GetDate();
	sCredentialScope += '/';
	sCredentialScope += sRegion;
	sCredentialScope += '/';
	sCredentialScope += sService;
	sCredentialScope += '/';
	sCredentialScope += "aws4_request";

	constexpr KStringView sAlgorithm = "AWS4-HMAC-SHA256";

	KString sStringToSign = KString(sAlgorithm);
	sStringToSign += '\n';
	sStringToSign += GetDateTime();
	sStringToSign += '\n';
	sStringToSign += sCredentialScope;
	sStringToSign += '\n';
	sStringToSign += GetDigest();

	auto sSigningKey = SignatureKey(sSecretKey, GetDate(), sRegion, sService);
	auto sSignature  = SHA256HMACHexDigest(sSigningKey, sStringToSign);

	KString sAuthHeader = KString(sAlgorithm);
	sAuthHeader += " Credential=";
	sAuthHeader += sAccessKey;
	sAuthHeader += '/';
	sAuthHeader += sCredentialScope;
	sAuthHeader += ", SignedHeaders=";
	sAuthHeader += GetSignedHeaders();
	sAuthHeader += ", Signature=";
	sAuthHeader += sSignature;

	m_AddedHeaders.insert(HTTPHeaders::value_type { "Authorization", sAuthHeader });

	return m_AddedHeaders;

} // Authorize

}  // namespace AWSAuth4

}  // namespace dekaf2
