#include "kawsauth.h"
#include "khmac.h"
#include "kmessagedigest.h"
#include "kstringutils.h"
#include "kurlencode.h"

DEKAF2_NAMESPACE_BEGIN

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
							 KString     sContentType,
							 KStringView sProvider,
							 const HTTPHeaders& AdditionalSignedHeaders,
							 KString     sDateTime)
//---------------------------------------------------------------------------
{
	m_sDateTime = std::move(sDateTime);
	// and produce a date only string
	m_sDate = m_sDateTime.substr(0, 8);

	m_sHost = URL.Domain.Serialize();
	m_sHost.Trim();

	if (sProvider.empty())
	{
		// empty is not allowed!
		sProvider = "amz";
	}
	KString sProviderLowercase = sProvider.ToLowerASCII();
	KString sProviderTitlecase = sProviderLowercase;
	// see above: sProvider is never empty
	sProviderTitlecase[0] = KASCII::kToUpper(sProviderTitlecase[0]);

	if (sContentType.empty())
	{
		sContentType = kFormat("application/x-{}-json-1.1", sProviderLowercase);
	}

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

	if (!sContentType.empty() &&
		(Method == KHTTPMethod::POST || Method == KHTTPMethod::PUT || Method == KHTTPMethod::PATCH))
	{
		LowerCaseHeaders.emplace("content-type", sContentType);
		m_AddedHeaders  .emplace("Content-Type", sContentType);
	}
	LowerCaseHeaders.emplace("host", m_sHost);
	LowerCaseHeaders.emplace(kFormat("x-{}-date", sProviderLowercase), m_sDateTime);

	m_AddedHeaders  .emplace("Host", m_sHost);
	m_AddedHeaders  .emplace(kFormat("X-{}-Date", sProviderTitlecase), m_sDateTime);

	if (!sTarget.empty())
	{
		LowerCaseHeaders.emplace(kFormat("x-{}-target", sProviderLowercase), sTarget);
		m_AddedHeaders  .emplace(kFormat("X-{}-Target", sProviderTitlecase), sTarget);
	}

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
											KStringView sService,
											KStringView sProvider)
//---------------------------------------------------------------------------
{
	if (sRegion.empty() || sService.empty())
	{
		auto Parts = m_sHost.Split(".");

		if (Parts.size() < 4)
		{
			kDebug(1, "invalid hostname: {}", m_sHost);
		}
		else
		{
			if (sRegion.empty())
			{
				sRegion = Parts[1];
			}

			if (sService.empty())
			{
				sService = Parts[0];
			}
		}
	}

	if (sProvider.empty())
	{
		// empty is not allowed!
		sProvider = "aws";
	}

	KString sCredentialScope = GetDate();
	sCredentialScope += '/';
	sCredentialScope += sRegion;
	sCredentialScope += '/';
	sCredentialScope += sService;
	sCredentialScope += '/';
	sCredentialScope += sProvider.ToLowerASCII();
	sCredentialScope += "4_request";

	KString sAlgorithm = sProvider.ToUpperASCII();
	sAlgorithm += "4-HMAC-SHA256";

	KString sStringToSign = sAlgorithm;
	sStringToSign += '\n';
	sStringToSign += GetDateTime();
	sStringToSign += '\n';
	sStringToSign += sCredentialScope;
	sStringToSign += '\n';
	sStringToSign += GetDigest();

	auto sSigningKey = SignatureKey(sSecretKey, GetDate(), sRegion, sService);
	auto sSignature  = SHA256HMACHexDigest(sSigningKey, sStringToSign);

	KString sAuthHeader = sAlgorithm;
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

DEKAF2_NAMESPACE_END
