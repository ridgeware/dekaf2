#pragma once

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include "khttp_method.h"
#include "ktime.h"
#include <map>

DEKAF2_NAMESPACE_BEGIN

namespace AWSAuth4 {

using HTTPHeaders = std::map<KString, KString>;

//-----------------------------------------------------------------------------
/// generate an AWS signature key for a day, region, service, and provider
KString SignatureKey(KStringView sKey, 
					 KStringView sDateStamp,
					 KStringView sRegion, 
					 KStringView sService,
					 KStringView sProvider = "aws");
//-----------------------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Authorizing a service request to an AWS service with personal access token
/// and secret key
class SignedRequest
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Create a signature over what AWS calls a canonical request - a mix of
	/// headers, path, query, and payload, hashed and HMACed.
	/// @param URL full URL including path and query (if any), like "https://translate.us-east-1.amazonaws.com"
	/// @param Method the HTTP method, like KHTTPMethod::GET or KHTTPMethod::POST
	/// @param sTarget the API's method, e.g. TranslateDocument. Can also be left empty, e.g. for S3 access.
	/// @param sPayload a string_view of the payload (body content), to compute hashes..
	/// @param sContentType the content type is required for translate requests.
	/// This implementation defaults it to amz-json (for the empty string)
	/// @param sProvider component of the x-{provider}-date etc. headers. defaults to "amz", others are "goog", "osc"
	/// @param AdditionalSignedHeaders should contain all the headers that you would like
	/// to sign. Defaulted to empty list.
	/// @param sDateTime timestamp for which the request should be signed, defaults to current time
	SignedRequest(
		const KURL& URL,
		KHTTPMethod Method,            // e.g. "GET"
		KStringView sTarget,           // e.g. TranslateDocument
		KStringView sPayload,          // e.g. ""
		KString     sContentType = {}, // application/x-{sProvider}-json-1.1 will be inserted for the empy string
		KStringView sProvider = "amz", // amz for AWS
		const HTTPHeaders& AdditionalSignedHeaders = {},
		KString sDateTime = kNow().to_string("%Y%m%dT%H%M%SZ"));

	/// use an access key and a shared secret to generate the headers that have to
	/// be added for a request in a certain region and for a certain service. Add
	/// the returned HTTP headers to your HTTP client's request.
	/// @param sAccessKey an AWS access key
	/// @param sSecretKey an AWS secret key
	/// @param sRegion an AWS service region, like us-east-1 (this must also be
	/// part of the host name scheme used in the constructor, and will be isolated from the host
	/// name if omitted here)
	/// @param sService an AWS service, like "translate" (this will be isolated from the host
	/// name if omitted here)
	/// @param sProvider aws, goog, osc, .. defaults to aws
	/// @return the set of HTTP headers that have to be added to a client request
	/// for authorization. This includes the Authorization header, but also AWS
	/// internal headers like X-Amz-Target.
	const HTTPHeaders& Authorize(
		KStringView sAccessKey,
		KStringView sSecretKey,
		KStringView sRegion   = {},   // e.g. "us-east-1", will be isolated from URL if empty
		KStringView sService  = {},   // e.g. "translate", will be isolated from URL if empty
		KStringView sProvider = {});  // e.g. "aws"

	/// internal helper, exposes the computed SHA256 digest for the canonical
	/// request
	const KString& GetDigest() const { return m_sCanonicalRequestDigest; }

	/// internal helper, exposes the list of headers cryptographically signed
	const KString& GetSignedHeaders() const { return m_sSignedHeaders; }

	/// internal helper, exposes the datetime stamp used in the signing and
	/// authorization requests
	const KString& GetDateTime() const { return m_sDateTime; }

	/// internal helper, exposes the date stamp used in the signing and
	/// authorization requests
	const KString& GetDate() const { return m_sDate; }

	/// internal helper, exposes the host name used in the signing and
	/// authorization requests
	const KString& GetHost() const { return m_sHost; }

	/// helper, exposes the generated authentication http headers. Also returned
	/// from the Authorize() method
	const HTTPHeaders& GetAddedHeaders() const { return m_AddedHeaders; }

//------
private:
//------

	KString m_sCanonicalRequestDigest;
	KString m_sSignedHeaders;
	KString m_sDateTime;
	KString m_sDate;
	KString m_sHost;
	KString m_sProviderLowercase;
	HTTPHeaders m_AddedHeaders;

}; // SignedRequest

}  // namespace AWSAuth4

DEKAF2_NAMESPACE_END
