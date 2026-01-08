#include "catch.hpp"

#include <dekaf2/ktlsstream.h>
#include <dekaf2/ktcpclient.h>
#include <dekaf2/kwebclient.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <iostream>

using namespace dekaf2;

TEST_CASE("KTCPCLient")
{
	KTCPClient stream;
	stream.Connect("www.google.com:80", chrono::seconds(5));

	stream.SetWriterEndOfLine("\r\n");
	stream.SetReaderRightTrim("\r\n");
	CHECK ( stream.good() );
	stream.WriteLine("GET / HTTP/1.1");
	CHECK ( stream.good() );
	stream.WriteLine("Host: www.google.com");
	CHECK ( stream.good() );
	stream.WriteLine("");
	CHECK ( stream.good() );
	stream.flush();
	CHECK ( stream.good() );
	KString str;
	CHECK ( stream.ReadLine(str) == true );
	CHECK ( stream.good() );

}

TEST_CASE("KTLSClient")
{

	KTLSClient stream;
	CHECK ( stream.Connect("www.google.com:443", KStreamOptions::None) == true );
	stream.SetTimeout(chrono::seconds(1));

	stream.SetReaderRightTrim("\r\n");
	stream.SetWriterEndOfLine("\r\n");
	CHECK ( stream.good() );
	stream.WriteLine("GET / HTTP/1.1");
	CHECK ( stream.good() );
	stream.WriteLine("Host: www.google.com");
	CHECK ( stream.good() );
	stream.WriteLine("");
	CHECK ( stream.good() );
	stream.flush();
	CHECK ( stream.good() );

	KString str;
	// this is to test the timeout functionality
	CHECK ( stream.ReadRemaining(str) == true );
	CHECK ( stream.good() == false );

}

TEST_CASE("KTLSClient 2")
{
	KString sURL {"https://www.google.com"};
	KWebClient HTTP;
	HTTP.SetTimeout(chrono::seconds(1));
	KString sResponse = HTTP.Get (sURL);
	CHECK ( !sResponse.empty() );
	CHECK ( HTTP.Response.iStatusCode == 200 );
}

TEST_CASE("KTLSClient 3")
{
	KString sURL {"https://www.dfggooglesdkjfhsjuhkjlgrsisfugkhvij.com"};
	KWebClient HTTP;
	HTTP.SetTimeout(chrono::seconds(1));
	KString sResponse = HTTP.Get (sURL);
	CHECK ( sResponse.empty() );
	CHECK ( HTTP.Response.iStatusCode != 200 );
#ifdef DEKAF2_IS_WINDOWS
	CHECK ( HTTP.Error().contains("No such host is known" ));
#else
	CHECK ( HTTP.Error().contains("Host not found (authoritative)") );
#endif
}

TEST_CASE("KTLSClient 4")
{
	// we use github as a lickmus test to see if our TLS stack
	// supports TLS 1.2/1.3 (as github removed support for older
	// versions)
	KString sURL {"https://github.com"};
	KWebClient HTTP;
	HTTP.SetTimeout(chrono::seconds(1));
	KString sResponse = HTTP.Get (sURL);
	INFO  ( "Verify TLS 1.2/1.3 support" );
	CHECK ( !sResponse.empty() );
	CHECK ( HTTP.Response.iStatusCode == 200 );
}
