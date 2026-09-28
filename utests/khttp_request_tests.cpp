#include "catch.hpp"

#include <dekaf2/http/protocol/khttp_request.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

using namespace dekaf2;

TEST_CASE("KHTTPRequest")
{

	SECTION("KInHTTPRequestLine")
	{
		KInHTTPRequestLine RL;
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		auto Words = RL.Parse("GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 3);
		CHECK (Words[0]        == "GET");
		CHECK (Words[1]        == "/This/is/a/test?with=parameters");
		CHECK (Words[2]        == "HTTP/1.1");
		CHECK (RL.IsValid()	   == true);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "GET");
		CHECK (RL.GetResource()== "/This/is/a/test?with=parameters");
		CHECK (RL.GetPath()    == "/This/is/a/test");
		CHECK (RL.GetQuery()   == "with=parameters");
		CHECK (RL.GetVersion() == "HTTP/1.1");

		Words = RL.Parse(" GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == " GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET  /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET  /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parameters  HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters  HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parametersHTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parametersHTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parameters FTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters FTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		KString sRequest;
		sRequest.assign(256, 'G');
		sRequest += "GET /This/is/a/test?with=parameters HTTP/1.1";
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == sRequest);
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		sRequest = "GET ";
		sRequest.append(KInHTTPRequestLine::MAX_REQUESTLINELENGTH, '/');
		sRequest += "/This/is/a/test?with=parameters HTTP/1.1";
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		sRequest = "GET /This/is/a/test?with=parameters HTTP/1.1";
		sRequest.append(256, '1');
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == sRequest);
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");
	}

}

TEST_CASE("KHTTPRequestHeaders::AcceptedCompressors")
{
	auto Accepted = [](KStringView sAcceptEncoding, KStringView sUserAgent, KHTTPVersion Version, bool bChunked)
	{
		KHTTPRequestHeaders Request;
		Request.SetHTTPVersion(Version);
		Request.Headers.Set(KHTTPHeader::ACCEPT_ENCODING, sAcceptEncoding);

		if (!sUserAgent.empty())
		{
			Request.Headers.Set(KHTTPHeader::USER_AGENT, sUserAgent);
		}

		return Request.AcceptedCompressors(bChunked);
	};

	SECTION("HTTP/1.0 has no chunking")
	{
		CHECK ( Accepted("gzip, deflate", "", KHTTPVersion::http10, true ) == 0 );
		CHECK ( Accepted("gzip, deflate", "", KHTTPVersion::http10, false) == (KHTTPCompression::GZIP | KHTTPCompression::ZLIB) );
		CHECK ( Accepted("gzip, deflate", "", KHTTPVersion::http11, true ) == (KHTTPCompression::GZIP | KHTTPCompression::ZLIB) );

		KHTTPRequestHeaders Request;
		Request.SetHTTPVersion(KHTTPVersion::http10);
		Request.Headers.Set(KHTTPHeader::ACCEPT_ENCODING, "gzip");
		CHECK ( Request.SupportedCompression() == "" );
		Request.SetHTTPVersion(KHTTPVersion::http11);
		CHECK ( Request.SupportedCompression() == "gzip" );
		Request.Headers.Set(KHTTPHeader::ACCEPT_ENCODING, "gzip;q=0, deflate");
		CHECK ( Request.SupportedCompression() == "deflate" );
	}

#if defined(DEKAF2_HAS_LIBZSTD) && defined(DEKAF2_HAS_LIBBROTLI)
	SECTION("Safari zstd exclusion")
	{
		constexpr KStringView sAcceptEncoding = "gzip, deflate, br, zstd";
		constexpr KStringView sSafari26 = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.3.1 Safari/605.1.15";
		constexpr KStringView sSafari27 = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/27.0 Safari/605.1.15";
		constexpr KStringView sCriOS    = "Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/140.0.7339.101 Mobile/15E148 Safari/604.1";
		constexpr KStringView sChrome   = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";
		constexpr KStringView sFirefox  = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:143.0) Gecko/20100101 Firefox/143.0";

		auto HasZstd = [&](KStringView sUserAgent, KHTTPVersion Version, bool bChunked)
		{
			return (Accepted(sAcceptEncoding, sUserAgent, Version, bChunked) & KHTTPCompression::ZSTD) != 0;
		};

		// Safari 26 cannot decode chunked zstd responses on HTTP/1.1
		CHECK_FALSE ( HasZstd(sSafari26, KHTTPVersion::http11, true ) );
		CHECK       ( HasZstd(sSafari26, KHTTPVersion::http11, false) );
		CHECK       ( HasZstd(sSafari26, KHTTPVersion::http2 , true ) );
		CHECK       ( (Accepted(sAcceptEncoding, sSafari26, KHTTPVersion::http11, true) & KHTTPCompression::BROTLI) != 0 );
		// Safari 27 can
		CHECK       ( HasZstd(sSafari27, KHTTPVersion::http11, true ) );
		// without a Version/ token the Safari version is not known
		CHECK_FALSE ( HasZstd(sCriOS   , KHTTPVersion::http11, true ) );
		CHECK       ( HasZstd(sCriOS   , KHTTPVersion::http11, false) );
		// other browsers are not affected
		CHECK       ( HasZstd(sChrome  , KHTTPVersion::http11, true ) );
		CHECK       ( HasZstd(sFirefox , KHTTPVersion::http11, true ) );
		CHECK       ( HasZstd(""       , KHTTPVersion::http11, true ) );

		KHTTPRequestHeaders Request;
		Request.SetHTTPVersion(KHTTPVersion::http11);
		Request.Headers.Set(KHTTPHeader::ACCEPT_ENCODING, sAcceptEncoding);
		Request.Headers.Set(KHTTPHeader::USER_AGENT, sSafari26);
		CHECK ( Request.SupportedCompression() == "br" );
		Request.Headers.Set(KHTTPHeader::USER_AGENT, sSafari27);
		CHECK ( Request.SupportedCompression() == "zstd" );
	}
#endif
}

TEST_CASE("KInHTTPRequestLine::GetQuery")
{
	// request line without a query string: GetQuery() must return an empty view,
	// not an out-of-bounds view from an unsigned length underflow (reachable e.g.
	// through the %q access-log directive on any query-less request).
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path HTTP/1.1");
		auto q = RL.GetQuery();
		CHECK ( q.empty() );
		CHECK ( q.size() == 0 );
		CHECK ( RL.GetPath() == "/path" );
	}

	// with a query string it is returned without the leading '?'
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path?x=1&y=2 HTTP/1.1");
		CHECK ( RL.GetQuery() == "x=1&y=2" );
		CHECK ( RL.GetPath()  == "/path" );
	}

	// present but empty query ("path?")
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path? HTTP/1.1");
		auto q = RL.GetQuery();
		CHECK ( q.empty() );
		CHECK ( q.size() == 0 );
		CHECK ( RL.GetPath() == "/path" );
	}

	// root path, no query
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET / HTTP/1.1");
		CHECK ( RL.GetQuery().empty() );
		CHECK ( RL.GetPath() == "/" );
	}
}
