#include <dekaf2/dekaf2.h>
#include <dekaf2/krest.h>
#include <dekaf2/koptions.h>
#include <dekaf2/ktcpserver.h>

using namespace dekaf2;

// a small but complete "Hello World" REST echo responder, and a minimalized
// echo responder based on KTCPServer

static constexpr KStringView g_sHelp[]
{
	"",
	"REST server benchmark",
	"",
	"  usage: restserver <options> run",
	"",
	"where options are:",
	"",
	"  -p <port number>           :: port to bind server to  (8888)",
	"  -c <concurrency>           :: concurrent server threads (20)",
	"  -t <timeout>               :: request timeout in seconds (1)",
	"  -k <max keepalive rounds>  :: max keepalive rounds     (100)",
	"  --simple                   :: SIMPLE server instead of REST",
	"  --klog <header>            :: set klog debug header (for REST)",
	""
};


class SimpleServer : public KTCPServer
{

public:

	SimpleServer(KREST::Options& Options)
	: KTCPServer(Options.iPort, false, Options.iMaxConnections)
	{
	}

	void Session(KStream& stream, KStringView sRemoteEndPoint, int iSocketFd) override final
	{
		stream.SetReaderRightTrim("\r\n");

		KString sLine;

		while (stream.ReadLine(sLine))
		{
			if (sLine.empty())
			{
				// end of headers
				stream.Write(sResponse).Flush();
			}
		}
	}

private:

	static constexpr KStringView sResponse {
		"HTTP/1.1 200 OK\r\n"
		"Connection: keep-alive\r\n"
		"Server: dekaf2 bench server\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 26\r\n"
		"\r\n"
		"{\"message\":\"hello world\"}\n"
	};

};

int main(int argc, char** argv)
{
	kInit();
	// create Options
	KREST::Options Options;
	Options.Type = KREST::HTTP;
	Options.iPort = 8888;
	Options.iMaxConnections = 20;
	Options.iTimeout = 1;
	Options.iMaxKeepaliveRounds = 100;

	bool bSimpleServer { false };

	// create CLI parser
	KOptions CLI(false);

	CLI.RegisterHelp(g_sHelp);

	CLI.Option("p", "port number")
	.Type(KOptions::Integer).Range(1, 65535)
	([&](KStringViewZ sArg)
	{
		Options.iPort = sArg.UInt16();
	});

	CLI.Option("c", "concurency").Type(KOptions::Integer)
	([&](KStringViewZ sArg)
	{
		Options.iMaxConnections = sArg.UInt16();
	});

	CLI.Option("t", "timeout").Type(KOptions::Integer)
	([&](KStringViewZ sArg)
	{
		Options.iTimeout = sArg.UInt16();
	});

	CLI.Option("k", "max keepalive rounds").Type(KOptions::Integer)
	([&](KStringViewZ sArg)
	{
		Options.iMaxKeepaliveRounds = sArg.UInt16();
	});

	CLI.Option("simple")([&]()
	{
		bSimpleServer = true;
	});

	CLI.Option("klog", "klog header")
	([&](KStringViewZ sArg)
	{
		Options.sKLogHeader = sArg;
	});

	bool bRun { false };

	CLI.Command("run")([&]()
	{
		bRun = true;
	});

	// parse CLI args
	CLI.Parse(argc, argv, KErr);

	if (!bRun)
	{
		CLI.Help(KErr);
		return 1;
	}

	if (bSimpleServer)
	{
		KOut.FormatLine("starting SIMPLE server on port {}", Options.iPort);

		SimpleServer Server(Options);
		return Server.Start(Options.iTimeout);
	}
	else
	{
		// create a REST router
		KRESTRoutes Routes;

		// add our rest route to it: we listen for all requests
		Routes.AddRoute({ KHTTPMethod(), false, "/*", [&](KRESTServer& http)
		{
			http.SetRawOutput("{\"message\":\"hello world\"}\n");
//			http.json.tx["message"] = "hello world";
		}});

		KOut.FormatLine("starting REST server on port {}", Options.iPort);

		// and start the REST server
		return KREST().Execute(Options, Routes);
	}

}


