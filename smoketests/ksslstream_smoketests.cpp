#include "catch.hpp"

#include <dekaf2/ksslclient.h>
#include <dekaf2/ktcpclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

// TODO move these tests into smoketests once kcurl has been merged

TEST_CASE("KTCPCLient")
{
	KTCPClient stream;
	stream.expires_from_now(boost::posix_time::seconds(5));
	stream.connect("www.google.com", "http");

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

TEST_CASE("KSSLClient")
{

	KSSLClient stream;
	CHECK ( stream.connect("www.google.com", "https", true) == true );
	stream.Timeout(1);

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

