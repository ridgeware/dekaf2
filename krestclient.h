/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2019, Ridgeware, Inc.
 //
 // +-------------------------------------------------------------------------+
 // | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
 // |/+---------------------------------------------------------------------+/|
 // |/|                                                                     |/|
 // |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
 // |/|                                                                     |/|
 // |\|   OPEN SOURCE LICENSE                                               |\|
 // |/|                                                                     |/|
 // |\|   Permission is hereby granted, free of charge, to any person       |\|
 // |/|   obtaining a copy of this software and associated                  |/|
 // |\|   documentation files (the "Software"), to deal in the              |\|
 // |/|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |\|
 // |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
 // |\|   and to permit persons to whom the Software is furnished to        |\|
 // |/|   do so, subject to the following conditions:                       |/|
 // |\|                                                                     |\|
 // |/|   The above copyright notice and this permission notice shall       |/|
 // |\|   be included in all copies or substantial portions of the          |\|
 // |/|   Software.                                                         |/|
 // |\|                                                                     |\|
 // |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
 // |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */

#pragma once

#include "kstring.h"
#include "kstringview.h"
#include "kwebclient.h"
#include "kjson.h"
#include "khttperror.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// REST client implementation with string input/output
class KRestClient : protected KWebClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self = KRestClient;
	using base = KWebClient;

	/// Default ctor - call SetURL() before any request
	KRestClient     ();
	/// Construct with URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	KRestClient     (KURL URL, bool bVerifyCerts = false);

	/// Set URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	self& SetURL    (KURL URL, bool bVerifyCerts = false);

	/// Get the API URL, const version
	const KURL& GetURL() const      { return m_URL;                                              }

	/// Get the API URL, non-const version
	KURL& URL()                     { return m_URL;                                              }

	/// Register an error code object and switch off exceptions - this setting is only valid
	/// for the next request
	self& SetError  (KHTTPError& ec);

	/// Send the REST request including body to the target and return the response.
	/// Throws or sets error object for non-200 responses.
	KString Request (KStringView sBody, KMIME mime);
	/// Send the REST request including a multipart form body to the target and return the response.
	/// Throws or sets error object for non-200 responses.
	KString Request (const KMIMEMultiPart& MultiPart);
	/// Send the REST request without body to the target and return the response.
	/// Throws or sets error object for non-200 responses.
	KString Request ()              { return Request(KStringView{}, KMIME{});                    }

	/// Send the REST request including body to the target. Expects unstructured data
	/// written to OutStream as response.
	/// Throws or sets error object for non-200 responses.
	bool Request (KOutStream& OutStream, KStringView sBody, KMIME mime);
	/// Send the REST request including a multipart form body to the target. Expects unstructured data
	/// written to OutStream as response.
	/// Throws or sets error object for non-200 responses.
	bool Request (KOutStream& OutStream, const KMIMEMultiPart& MultiPart);
	/// Send the REST request without body to the target and return the response.
	/// Throws or sets error object for non-200 responses.

	/// Set the 'Verb' (HTTP method) for the next request - can also be done implicitly
	/// through one of the Get/Post/Put/Patch/Delete methods
	self& Verb      (KHTTPMethod sVerb);
	/// Set the Path for the next request - will be appended to a base path set at the constructor
	self& Path      (KString sPath);
	/// Set the Query part for the next request
	self& SetQuery  (url::KQuery Query);
	/// Add a Query part to existing queries
	self& AddQuery  (url::KQuery Query);
	/// Add a name/value query part to existing queries
	self& AddQuery  (KString sName, KString sValue);
	/// Add (overwrite) a request header to existing headers
	self& AddHeader(KStringView sName, KStringView sValue);

	/// Set a Get method with path to call
	self& Get       (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::GET    );  }
	/// Set a Head method with path to call
	self& Head      (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::HEAD   );  }
	/// Set a Post method with path to call
	self& Post      (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::POST   );  }
	/// Set a Put method with path to call
	self& Put       (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::PUT    );  }
	/// Set a Patch method with path to call
	self& Patch     (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::PATCH  );  }
	/// Set a Delete method with path to call
	self& Delete    (KString sPath) { return Path(std::move(sPath)).Verb(KHTTPMethod::DELETE );  }

	/// clear all state except the ctor parameters - will be called automatically after a
	/// Request() and before the next setup of a request
	void clear ();

	using base::Good;
	using base::HttpSuccess;
	using base::HttpFailure;
	using base::GetStatusCode;
	using base::Error;
	using base::Authentication;
	using base::BasicAuthentication;
	using base::DigestAuthentication;
	using base::ClearAuthentication;
	using base::SetTimeout;
	using base::SetProxy;
	using base::AutoConfigureProxy;
	using base::AllowRedirects;
	using base::AllowCompression;
	using base::VerifyCerts;
	using base::GetVerifyCerts;
	using base::AllowQueryToWWWFormConversion;
	using base::AcceptCookies;

//----------
protected:
//----------

	/// Send the REST request including an eventual body to the target and write the response to OutStream.
	/// Does not throw
	bool NoExceptRequest (KOutStream& OutStream, KStringView sBody, KMIME mime) noexcept;
	/// Throws the error if no error object is set, otherwise sets the error object and
	/// returns false
	bool ThrowOrReturn (KHTTPError&& ec, bool bRetval = false);
	/// Calls clear once after a Request() to reset all state and setup except from ctor
	void ResetAfterRequest();

	KURL m_URL;
	KHTTPMethod m_sVerb;
	KString m_sPath;
	url::KQuery m_Query;
	KHTTPError* m_ec { nullptr };
	bool m_bNeedReset { false };

}; // KRestClient

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// REST client implementation with JSON input/output
class KJsonRestClient : public KRestClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ErrorCallback = std::function<KString(const KJSON&)>;
	using self = KJsonRestClient;
	using base = KRestClient;

	static KString DefaultErrorCallback(const KJSON& jResponse, KStringView sErrorProperties = "message,messages,error,errors");

	/// Default ctor - call SetURL() before any request
	KJsonRestClient () = default;
	/// Construct with URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	/// The ErrorCallback will be called on non-200 responses with valid JSON response and
	/// should be used to identify the error text in the JSON.
	KJsonRestClient (KURL URL, bool bVerifyCerts = false, ErrorCallback ecb = nullptr)
	: KRestClient(std::move(URL), bVerifyCerts)
	, m_ErrorCallback(std::move(ecb))
	{
	}

	/// Set URL to connect to, including basic REST path and basic query parms.
	/// The individual request path will be added to the basic path, same for query parms.
	self& SetURL    (KURL URL, bool bVerifyCerts = false)
	                         { base::SetURL(std::move(URL), bVerifyCerts); return *this; }

	/// The ErrorCallback will be called on non-200 responses with valid JSON response and
	/// should be used to identify the error text in the JSON.
	self& SetErrorCallback (ErrorCallback ecb)
	{
		m_ErrorCallback = std::move(ecb);
		return *this;
	}

	/// Register an error code object and switch off exceptions - this setting is only valid
	/// for the next request
	self& SetError (KHTTPError& ec)    { base::SetError(ec);               return *this; }

	/// Send the REST request including an eventual JSON body to the target and return the response.
	/// Throws or sets error object for non-200 responses.
	KJSON Request  (const KJSON& json = KJSON{}, KMIME Mime = KMIME::JSON);
	/// Send the REST request including a multipart form body to the target and return the response.
	/// Throws or sets error object for non-200 responses.
	KJSON Request (const KMIMEMultiPart& MultiPart);

	/// Send the REST request including an eventual JSON body to the target. Expects unstructured data
	/// written to OutStream as response.
	/// Throws or sets error object for non-200 responses.
	bool Request (KOutStream& OutStream, const KJSON& json = KJSON{}, KMIME Mime = KMIME::JSON);
	/// Send the REST request including a multipart form body to the target. Expects unstructured data
	/// written to OutStream as response.
	/// Throws or sets error object for non-200 responses.
	bool Request (KOutStream& OutStream, const KMIMEMultiPart& MultiPart);

	/// Set the 'Verb' (HTTP method) for the next request - can also be done implicitly
	/// through one of the Get/Post/Put/Patch/Delete methods
	self& Verb     (KString sVerb)     { base::Verb(std::move(sVerb));     return *this; }
	/// Set the Path for the next request - will be appended to a base path set at the constructor
	self& Path     (KString sPath)     { base::Path(std::move(sPath));     return *this; }
	/// Set the Query part for the next request
	self& SetQuery (url::KQuery Query) { base::SetQuery(std::move(Query)); return *this; }
	/// Add a Query part to existing queries
	self& AddQuery (url::KQuery Query) { base::AddQuery(std::move(Query)); return *this; }
	/// Add a name/value query part to existing queries
	self& AddQuery (KString sName, KString sValue)
	                                   { base::AddQuery(std::move(sName), std::move(sValue));
										   return *this; }
	/// Add (overwrite) a request header to existing headers
	self& AddHeader (KStringView sName, KStringView sValue)
									   { base::AddHeader(sName, sValue); return *this;   }

	/// Set a Get method with path to call
	self& Get      (KString sPath)     { base::Get(std::move(sPath));      return *this; }
	/// Set a Head method with path to call
	self& Head     (KString sPath)     { base::Head(std::move(sPath));     return *this; }
	/// Set a Post method with path to call
	self& Post     (KString sPath)     { base::Post(std::move(sPath));     return *this; }
	/// Set a Put method with path to call
	self& Put      (KString sPath)     { base::Put(std::move(sPath));      return *this; }
	/// Set a Patch method with path to call
	self& Patch    (KString sPath)     { base::Patch(std::move(sPath));    return *this; }
	/// Set a Delete method with path to call
	self& Delete   (KString sPath)     { base::Delete(std::move(sPath));   return *this; }

//----------
protected:
//----------

	/// Throws the error if no error object is set, otherwise sets the error object and
	/// returns the retval
	KJSON ThrowOrReturn (KHTTPError&& ec, KJSON&& retval = KJSON{});

	KJSON RequestAndParseResponse(KStringView sRequest, KMIME Mime);

//----------
private:
//----------

	ErrorCallback m_ErrorCallback;

}; // KJsonRestClient

} // end of namespace dekaf2
