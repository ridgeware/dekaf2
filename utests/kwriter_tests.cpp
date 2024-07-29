#include "catch.hpp"

#include <dekaf2/kstream.h>
#include <dekaf2/ktcpclient.h>
#include <dekaf2/kunixstream.h>
#include <dekaf2/ktlsstream.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/ksystem.h>


using namespace dekaf2;

namespace {
KTempDir TempDir;
}

class KMyServer : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;

protected:

	// a simple echo
	KString Request(KStringRef& sLine, Parameters& parameters) override
	{
		sLine += "\n";
		return sLine;
	}

};


#ifdef DEKAF2_HAS_FULL_CPP_17
constexpr
#endif
KStringView sCert = R"(-----BEGIN CERTIFICATE-----
MIIEpDCCAowCCQCjjdpjiU244jANBgkqhkiG9w0BAQsFADAUMRIwEAYDVQQDDAls
b2NhbGhvc3QwHhcNMTgxMDMwMTMwNjA4WhcNMTkxMDMwMTMwNjA4WjAUMRIwEAYD
VQQDDAlsb2NhbGhvc3QwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCl
Sy5eUHyAF14TbT713mYYozxG7Nj6twX2CbmrLkdpBPRgW/G35HPjiKlwr4D49I6t
EjZyMF7hdH5GdXofGAlpT89emiMm5epZAoWEsqTpFpfJjbEf9a+SGzTrhRxlMtbJ
FQyBIcN86bLmowAMIMfA1onCG/mQ5L5iO5s1KbNezH+vwhOlAn12/1ZkNQZwY8jO
3BWC5k2A0TSt0lPr+ZFzFcwJYKziOLKMVbiS4nB/37hI9AruczKppd1OWLqTTM+9
Eb13GzhCY1NDF/v80SVIZQt+P0KurZA/QMuTula/O4nYxZsjlRexuiDEJ1BHMr/t
aV6Sf9kzeh35ntNSf2xf5geITYqwjHRgwXaINYSbYTBg65nWpXybW33meecVeQvD
0HtXsdnV2N+asx4W3Onu6swI4OTmTDQ8hSxqOXrf8ba8vIXlDLoqjbzNIKcALXRM
cvw2T56s9G6xXKuRQAKou8edARLMDq2qjOTxo7Uy83EqkE2ZMWKx/PpbCG+gu9Gd
YKlvr+Wb1T4vK6AvmCXiqgNxwpav2tf9Cji76QlHjx0RJQYvvBNsOlgO8cjtG+6u
vRcI/obs3zjxIy7ZBHqTP86aDnUgbT90pAn8JVSP3Sd67ZVoJEsU0+eQYUoV7dGI
J5uRMnHj4VNJvjq/5S4/YeX6sTGbXWIR11GX63VKhQIDAQABMA0GCSqGSIb3DQEB
CwUAA4ICAQAFcxiEg7IK5s8Kvcwn6lFoOK0vu4AQ7J42LQz+ncgP+iNMXmZoJ6GX
08ZlG5TxWa7RhpfzOBzwn9XVJL3D/mu4in2Nipu7gdl1PNgZ01hFg+T+TnlrqU9P
Qz0u/PBy4pQgkAZyr5NA7nY8QYD6SQRgFZb8orN3gGjhWEfKQnyWDrfey3bixTPA
klF6PD6dHVaU3xa+NBZUC1kpob/lPdEI6AU/aNw4zguPve5czpmwWxRZdPD9lsSU
bDpq3XIPbnWOod3lq3akr6qr8gsfQaPs50QqpGmsrIf4XlSpG9KwBD+3VjlMaINS
vEav+f1pqfUls+MwQqRv6ThM8cevNUsqIUqmFxRfZ+SHNbyIebRvlhPDNuTWj3ZZ
Xd3x0udeekxz2R9+suvFRknHzlTMZ+TidFoTrcC+C50QWfDSwmCYDxT5WLyJXG9D
pEF3pzDMLgbIh+ChTKYZXXU1QYlNW2zzLe6vV7y6K7x9+GzdQsgQZv+I3QMd6hIH
hn7dm5rlcyD0iB4Xv7lHqwh62+b3sA6c38JgtHQduNJmU8p/dMAsYz99F9yw3v6H
QHvjFPcNWBZ32BrEzUeYmLaEGRNj22IWDYm1nu2x0pgkjtou9+HZgZTXPb078+8t
CBw53UppWW98e2rXKWXTtU6rEL1ctGz135WgBrqmkJ58n2pjd4jLDQ==
-----END CERTIFICATE-----
)";

#ifdef DEKAF2_HAS_FULL_CPP_17
constexpr 
#endif
KStringView sKey = R"(-----BEGIN PRIVATE KEY-----
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
ifGH2FR4yO/361j9D9X2h9uBgconDA==
-----END PRIVATE KEY-----
)";

TEST_CASE("KWriter") {

	kSetCWD("/tmp");

	SECTION("Short write to file")
	{
		KString sLarge;
		sLarge += "01234567890123456789012345678901234567890123456789012345678901234567890123456789";

		KString sFile(kFormat("{}/short.txt", TempDir.Name()));
		KOutFile stream(sFile);
		stream.Write(sLarge);
		CHECK( stream.Good() == true );
		stream.close();

		CHECK ( kFileSize(sFile) == sLarge.size() );
	}

	SECTION("Large write to file")
	{
		KString sLarge;
		sLarge.reserve(1*1024*1024);
		for (size_t ct = 0; ct < 1*1024*1024 / 80; ++ct)
		{
			sLarge += "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
		}

		KString sFile(kFormat("{}large.txt", TempDir.Name()));
		KOutFile stream(sFile);
		stream.Write(sLarge);
		CHECK( stream.Good() == true );
		stream.close();

		CHECK ( kFileSize(sFile) == sLarge.size() );
	}

#ifndef DEKAF2_IS_WINDOWS

	SECTION("Short write to TCP socket")
	{
		KString sLarge;
		sLarge = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";

		KMyServer Server(43234, false, 5);
		Server.Start(chrono::seconds(5), false);

		KTCPClient stream("localhost:43234", chrono::seconds(5));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}

	SECTION("Large write to TCP socket")
	{
		KString sLarge;
		sLarge.reserve(1*1024*1024);
		for (size_t ct = 0; ct < 1*1024*1024 / 80; ++ct)
		{
			sLarge += "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
		}

		KMyServer Server(43235, false, 5);
		Server.Start(chrono::seconds(5), false);

		KTCPClient stream("localhost:43235", chrono::seconds(5));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}
#endif

#ifndef DEKAF2_IS_WINDOWS
	SECTION("Short write to Unix socket")
	{
		KString sLarge;
		sLarge = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";


		KString sSocket;
#ifdef DEKAF2_IS_OSX
		// OSX has a very narrow limit on unix socket file name length,
		// and a very long temp folder name, so we use just /tmp ..
		kSystem("rm -f /tmp/short.socket");
		sSocket = "/tmp/short.socket";
#else
		sSocket = kFormat("{}/short.socket", TempDir.Name());
#endif
		KMyServer Server(sSocket, 5);
		Server.Start(chrono::seconds(5), false);

		KUnixStream stream(sSocket, chrono::seconds(5));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}

	SECTION("Large write to Unix socket")
	{
		KString sLarge;
		sLarge.reserve(1*1024*1024);
		for (size_t ct = 0; ct < 1*1024*1024 / 80; ++ct)
		{
			sLarge += "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
		}

		KString sSocket;
#ifdef DEKAF2_IS_OSX
		// OSX has a very narrow limit on unix socket file name length,
		// and a very long temp folder name, so we use just /tmp ..
		kSystem("rm -f /tmp/large.socket");
		sSocket = "/tmp/large.socket";
#else
		sSocket = kFormat("{}/large.socket", TempDir.Name());
#endif
		KMyServer Server(sSocket, 5);
		Server.Start(chrono::seconds(5), false);

		KUnixStream stream(sSocket, chrono::seconds(5));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}
#endif

#ifndef DEKAF2_IS_WINDOWS
	SECTION("short write to TLS socket")
	{
		KString sLarge;
		sLarge = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";

		KMyServer Server(43236, true, 5);
		Server.SetTLSCertificates(sCert, sKey);
		Server.Start(chrono::seconds(10), false);

		KTLSContext TLSContext(false);
		KTLSClient stream(TLSContext, "localhost:43236", KStreamOptions(KStreamOptions::None, chrono::seconds(10)));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
//			kSleep(5);
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}

	SECTION("Large write to TLS socket")
	{
		KString sLarge;
		sLarge.reserve(1*1024*1024);
		for (size_t ct = 0; ct < 1*1024*1024 / 80; ++ct)
		{
			sLarge += "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
		}

		KMyServer Server(43237, true, 5);
		Server.SetTLSCertificates(sCert, sKey);
		Server.Start(chrono::seconds(5), false);

		KTLSContext TLSContext(false);
		KTLSClient stream(TLSContext, "localhost:43237", KStreamOptions(KStreamOptions::None, chrono::seconds(5)));
		stream.Write(sLarge);
		stream.Write('\n');
		stream.Flush();
		CHECK( stream.KOutStream::Good() == true );

		if (stream.KOutStream::Good())
		{
			KString sRx;
			CHECK ( stream.ReadLine(sRx) == true );
			CHECK ( sRx == sLarge );
		}
	}
#endif

	SECTION("move")
	{
		static_assert(std::is_move_constructible<std::ofstream>::value, "std::ofstream has to be move constructible");

		KString sFile = kFormat("{}{}move-tests", TempDir.Name(), kDirSep);
		KString sLine = "one line of text\n";

		{
			std::ofstream F1(sFile);
			CHECK ( F1.is_open() );
			CHECK ( F1.good() );
			F1.write(sLine.data(), sLine.size());
			CHECK ( F1.good() );
			auto F2 = std::move(F1);
			CHECK ( F2.good() );
			F2.write(sLine.data(), sLine.size());
			CHECK ( F2.good() );
			F2.close();
			CHECK ( kReadAll(sFile) == "one line of text\none line of text\n" );
			CHECK ( kRemoveFile(sFile) );

			KOutStream outstream(F2);
			KOutStream outstream2 = std::move(outstream);
			KOutStream& outstream3 = outstream2;
		}
		{
			KWriter<std::ofstream> File(sFile);
			CHECK ( File.is_open() );
			CHECK ( File.Good() );
			File.Write(sLine);
			CHECK ( File.Good() );
			auto File2 = std::move(File);
			CHECK ( File2.Good() );
			File2.Write(sLine);
			CHECK ( File2.Good() );
			File2.close();
			CHECK ( kReadAll(sFile) == "one line of text\none line of text\n" );
			CHECK ( kRemoveFile(sFile) );
		}
		{
			KOutFile OutFile(sFile);
			CHECK ( OutFile.is_open() );
			CHECK ( OutFile.Good() );
			OutFile.Write(sLine);
			CHECK ( OutFile.Good() );
			auto OutFile2 = std::move(OutFile);
			CHECK ( OutFile2.is_open() );
			CHECK ( OutFile2.Good() );
			OutFile2.Write(sLine);
			CHECK ( OutFile2.Good() );
			OutFile2.close();
			CHECK ( kReadAll(sFile) == "one line of text\none line of text\n" );
			CHECK ( kRemoveFile(sFile) );
		}
	}

	SECTION("Seek")
	{
		KString sFile = kFormat("{}{}seek-tests", TempDir.Name(), kDirSep);
		kRemoveFile(sFile);
		KOutFile File(sFile);
		KString sBuffer;

		SECTION("standalone")
		{
			CHECK ( kGetWritePosition(File)     == 0     );
			CHECK ( kSetWritePosition(File, 30) == true  );
			CHECK ( kGetWritePosition(File)     == 30    );
			CHECK ( kWrite(File, "01234567890123456789", 20) == 20 );
			CHECK ( kGetWritePosition(File)     == 50    );
			CHECK ( kRewind(File, 5)            == true  );
			CHECK ( kGetWritePosition(File)     == 45    );
			CHECK ( kForward(File, 5)           == true  );
			CHECK ( kGetWritePosition(File)     == 50    );
			CHECK ( kSetWritePosition(File, 5)  == true  );
			CHECK ( kGetWritePosition(File)     == 5     );
			CHECK ( kWrite(File, "01234567890123456789", 20) == 20 );
			CHECK ( kGetWritePosition(File)     == 25    );
			CHECK ( kRewind(File)               == true  );
			CHECK ( kGetWritePosition(File)     == 0     );
			CHECK ( kForward(File)              == true  );
			CHECK ( kGetWritePosition(File)     == 50    );
			File.close();
			sBuffer = kReadAll(sFile);
			CHECK ( sBuffer == "\0\0\0\0\0" "01234567890123456789\0\0\0\0\0" "01234567890123456789"_ks );
		}

		SECTION("in class")
		{
			CHECK ( File.GetWritePosition()     == 0     );
			CHECK ( File.SetWritePosition(30)   == true  );
			CHECK ( File.GetWritePosition()     == 30    );
			File.Write("01234567890123456789");
			CHECK ( File.GetWritePosition()     == 50    );
			CHECK ( File.Rewind(5)              == true  );
			CHECK ( File.GetWritePosition()     == 45    );
			CHECK ( File.Forward(5)             == true  );
			CHECK ( File.GetWritePosition()     == 50    );
			CHECK ( File.SetWritePosition(5)    == true  );
			CHECK ( File.GetWritePosition()     == 5     );
			File.Write("01234567890123456789");
			CHECK ( File.GetWritePosition()     == 25    );
			CHECK ( File.Rewind()               == true  );
			CHECK ( File.GetWritePosition()     == 0     );
			CHECK ( File.Forward()              == true  );
			CHECK ( File.GetWritePosition()     == 50    );
			File.close();
			sBuffer = kReadAll(sFile);
			CHECK ( sBuffer == "\0\0\0\0\0" "01234567890123456789\0\0\0\0\0" "01234567890123456789"_ks );
		}
	}
}
