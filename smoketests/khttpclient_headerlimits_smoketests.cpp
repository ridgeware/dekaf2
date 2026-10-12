#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP2 || (DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_NGTCP2)

#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <algorithm>

using namespace dekaf2;

// Tests the response header limits of the HTTP/2 and HTTP/3 client against an
// independent HPACK and QPACK implementation: nginx runs as reverse proxy in a
// container, in front of a backend in this process that sends the scripted
// responses over HTTP/1.1. Start the proxy with
//
//   smoketests/proxy/start-proxy.sh
//
// The tests skip themselves when no proxy responds. Environment overrides:
//
//   DEKAF2_TEST_PROXY_HOST   - host of the proxy       (default 127.0.0.1)
//   DEKAF2_TEST_PROXY_PORT   - port of the proxy       (default 8443)
//   DEKAF2_TEST_BACKEND_PORT - port of the backend     (default 7660)

namespace {

//-----------------------------------------------------------------------------
// builds the response for a path of the form /<case>/<n>:
//
//   /plain         a response without additional fields
//   /fields/<n>    n additional fields
//   /field/<n>     one additional field of n bytes, name plus value
//   /section/<n>   additional fields of together n bytes, counted as the header
//                  limits count them: name plus value plus 32 for each field
//   /interim/<n>   n 103 Early Hints responses before the final response
//   /nolength/<n>  a body of n bytes without Content-Length, ended by closing
//                  the connection
KString BuildResponse(KStringView sPath)
//-----------------------------------------------------------------------------
{
	sPath.remove_prefix('/');
	auto iPos  = sPath.find('/');
	auto sCase = sPath.substr(0, iPos);
	std::size_t iCount = (iPos == KStringView::npos) ? 0 : sPath.substr(iPos + 1).UInt64();

	KString sResponse;
	KString sFields;

	if (sCase == "nolength")
	{
		sResponse  = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n";
		sResponse.append(iCount, 'a');
		return sResponse;
	}

	if (sCase == "fields")
	{
		for (std::size_t i = 0; i < iCount; ++i)
		{
			sFields += kFormat("x-f-{}: 1\r\n", i);
		}
	}
	else if (sCase == "field")
	{
		KStringView sName = "x-big";
		sFields += sName;
		sFields += ": ";
		sFields.append(iCount - sName.size(), 'a');
		sFields += "\r\n";
	}
	else if (sCase == "section")
	{
		// fields of at most 30000 bytes each, the bytes spread evenly over them
		constexpr std::size_t iNameSize  = 6; // x-s-NN
		constexpr std::size_t iOverhead  = iNameSize + 32;
		constexpr std::size_t iMaxValue  = 30000;
		std::size_t iFields = (iCount + iOverhead + iMaxValue - 1) / (iOverhead + iMaxValue);
		std::size_t iValues = iCount - iFields * iOverhead;

		for (std::size_t i = 0; i < iFields; ++i)
		{
			sFields += kFormat("x-s-{:02}: ", i);
			sFields.append(iValues / iFields + (i < iValues % iFields ? 1 : 0), 'a');
			sFields += "\r\n";
		}
	}
	else if (sCase == "interim")
	{
		for (std::size_t i = 0; i < iCount; ++i)
		{
			sResponse += "HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload\r\n\r\n";
		}
	}

	sResponse += "HTTP/1.1 200 OK\r\n";
	sResponse += sFields;
	sResponse += "Content-Length: 2\r\nConnection: close\r\n\r\nOK";

	return sResponse;

} // BuildResponse

// the backend behind the proxy: answers each request with the response that its
// path selects, and closes the connection
class KHeaderBackend : public KTCPServer
{

public:

	KHeaderBackend(uint16_t iPort)
	: KTCPServer(iPort, false, 20)
	{
	}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		stream->SetReaderRightTrim("\r\n");

		KString sRequestLine;

		if (!stream->ReadLine(sRequestLine))
		{
			return;
		}

		KString sLine;

		// the requests of these tests have no body
		while (stream->ReadLine(sLine) && !sLine.empty())
		{
		}

		// GET /fields/500 HTTP/1.1
		auto Words = sRequestLine.Split(" ");

		stream->Write(BuildResponse(Words.size() > 1 ? Words[1] : KStringView{})).Flush();
	}

}; // KHeaderBackend

// the fields that arrive with every response: those the proxy adds (date,
// server ..), the content length, and the :status pseudo header field
struct Baseline
{
	std::size_t iFields { 0 };
	std::size_t iSize   { 0 };
};

//-----------------------------------------------------------------------------
// counts the fields of a response as the header limits count them
Baseline CountFields(const KHTTPResponseHeaders& Response)
//-----------------------------------------------------------------------------
{
	// :status and its three digits
	Baseline Base { 1, 7 + 3 + 32 };

	for (const auto& Header : Response.Headers)
	{
		++Base.iFields;
		Base.iSize += Header.first.Serialize().size() + Header.second.size() + 32;
	}

	return Base;

} // CountFields

//-----------------------------------------------------------------------------
void RunHeaderLimitTests(KHTTPVersion Version, KStreamOptions::Options Option)
//-----------------------------------------------------------------------------
{
	auto sProxyHost   = kGetEnv("DEKAF2_TEST_PROXY_HOST", "127.0.0.1");
	auto iProxyPort   = KString(kGetEnv("DEKAF2_TEST_PROXY_PORT"  , "8443")).UInt16();
	auto iBackendPort = KString(kGetEnv("DEKAF2_TEST_BACKEND_PORT", "7660")).UInt16();
	auto sProxy       = kFormat("https://{}:{}", sProxyHost, iProxyPort);

	KHeaderBackend Backend(iBackendPort);
	Backend.Start(chrono::seconds(2), false);

	auto Client = [&](const KHTTPHeaders::Limits& Limits = KHTTPHeaders::DefaultResponseLimits)
	{
		auto HTTP = std::make_unique<KWebClient>();
		HTTP->SetStreamOptions(KHTTPStreamOptions(Option));
		HTTP->SetVerifyCerts(false);
		HTTP->AllowCompression(false);
		HTTP->SetTimeout(chrono::seconds(10));
		HTTP->SetResponseHeaderLimits(Limits);
		return HTTP;
	};

	auto HTTP = Client();
	auto sBody = HTTP->Get(kFormat("{}/plain", sProxy));

	if (HTTP->GetStatusCode() == 0)
	{
		WARN("no test proxy at " << sProxy << ", start smoketests/proxy/start-proxy.sh: " << HTTP->Error());
		return;
	}

	INFO ( Version.Serialize() );

	// the proxy has to reach the backend
	REQUIRE ( HTTP->GetStatusCode() == 200 );
	CHECK   ( sBody == "OK" );
	REQUIRE ( HTTP->Response.GetHTTPVersion() == Version );

	auto Base = CountFields(HTTP->Response);

	auto Get = [&](KWebClient& HTTP, KStringView sCase, std::size_t iCount) -> bool
	{
		auto sResult = HTTP.Get(kFormat("{}/{}/{}", sProxy, sCase, iCount));
		return HTTP.HttpSuccess() && sResult == "OK";
	};

	auto IsRejected = [](KWebClient& HTTP, KHTTPHeaders::ParseError Error) -> bool
	{
		return HTTP.HttpSuccess() == false
		    && HTTP.GetTransportError() == KHTTPClient::TransportError::HeaderLimit
		    && HTTP.Response.GetParseError() == Error;
	};

	const auto& Limits = KHTTPHeaders::DefaultResponseLimits;

	// requests the response of a case with a new client, and checks if it is
	// accepted, or rejected for the given reason
	auto Check = [&](KStringView sCase, std::size_t iCount, KHTTPHeaders::ParseError Rejected,
	                 const KHTTPHeaders::Limits& ClientLimits = KHTTPHeaders::DefaultResponseLimits)
	{
		auto HTTP = Client(ClientLimits);
		auto bOK  = Get(*HTTP, sCase, iCount);

		INFO ( kFormat("/{}/{}: {}", sCase, iCount, HTTP->Error()) );

		if (Rejected == KHTTPHeaders::NoError)
		{
			CHECK ( bOK );
		}
		else
		{
			CHECK ( bOK == false );
			CHECK ( IsRejected(*HTTP, Rejected) );
		}
	};

	// a field of the limit is accepted, one byte more is rejected
	Check("field"  , Limits.iMaxFieldLength                   , KHTTPHeaders::NoError);
	Check("field"  , Limits.iMaxFieldLength + 1               , KHTTPHeaders::FieldTooLong);

	{
		// far above the limit, the HPACK or QPACK decoder may reject the field
		// before the limit is checked - then the connection fails
		auto HTTP = Client();
		auto bOK  = Get(*HTTP, "field", 200000);
		INFO  ( HTTP->Error() );
		CHECK ( bOK == false );
		CHECK ( HTTP->GetTransportError() != KHTTPClient::TransportError::None );
		// and it fails right away, not by the timeout
		CHECK ( HTTP->GetTransportError() != KHTTPClient::TransportError::Timeout );
	}

	// the number of fields
	Check("fields" , Limits.iMaxFieldCount - Base.iFields     , KHTTPHeaders::NoError);
	Check("fields" , Limits.iMaxFieldCount - Base.iFields + 1 , KHTTPHeaders::TooManyFields);

	// the size of the section
	Check("section", Limits.iMaxSectionSize - Base.iSize      , KHTTPHeaders::NoError);
	Check("section", Limits.iMaxSectionSize - Base.iSize + 1  , KHTTPHeaders::SectionTooLarge);

	// interim responses
	Check("interim", detail::iMaxInterimResponses             , KHTTPHeaders::NoError);
	Check("interim", detail::iMaxInterimResponses + 1         , KHTTPHeaders::TooManyInterimResponses);

	{
		// after a rejected response the next request of the same client succeeds -
		// the rejection comes long before the end of these fields, so the rest of
		// the header block still arrives after the stream was reset
		auto HTTP = Client();
		auto bOK  = Get(*HTTP, "fields", Limits.iMaxFieldCount * 4);
		CHECK ( bOK == false );
		bOK = Get(*HTTP, "plain", 0);
		INFO  ( HTTP->Error() );
		CHECK ( bOK );
	}

	// a response without Content-Length ends with the stream - also when the end
	// arrives together with the headers
	for (std::size_t iSize : { 0, 5 })
	{
		auto HTTP    = Client();
		auto sResult = HTTP->Get(kFormat("{}/nolength/{}", sProxy, iSize));
		INFO  ( kFormat("/nolength/{}: {}", iSize, HTTP->Error()) );
		CHECK ( HTTP->HttpSuccess() );
		CHECK ( sResult == KString(iSize, 'a') );
	}

	// raised limits accept the responses that were rejected
	KHTTPHeaders::Limits Raised { Limits.iMaxFieldLength + 1, Limits.iMaxFieldCount + 1, Limits.iMaxSectionSize * 2 };

	Check("field"  , Limits.iMaxFieldLength + 1               , KHTTPHeaders::NoError, Raised);
	Check("fields" , Limits.iMaxFieldCount - Base.iFields + 1 , KHTTPHeaders::NoError, Raised);
	Check("section", Limits.iMaxSectionSize - Base.iSize + 1  , KHTTPHeaders::NoError, Raised);

} // RunHeaderLimitTests

} // end of anonymous namespace

TEST_CASE("KHTTPClient response header limits over a proxy")
{
#if DEKAF2_HAS_NGHTTP2
	SECTION("HTTP/2")
	{
		RunHeaderLimitTests(KHTTPVersion::http2, KStreamOptions::RequestHTTP2);
	}
#endif

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_NGTCP2
	SECTION("HTTP/3")
	{
		RunHeaderLimitTests(KHTTPVersion::http3, KStreamOptions::RequestHTTP3);
	}
#endif
}

#endif
