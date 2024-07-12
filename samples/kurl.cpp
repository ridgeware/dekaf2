/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2022, Ridgeware, Inc.
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

#include "kurl.h"
#include <dekaf2/kdefinitions.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kwebclient.h>
#include <dekaf2/kxml.h>
#include <dekaf2/kencode.h>
#include <dekaf2/kawsauth.h>
#include <dekaf2/kparallel.h>
#include <dekaf2/kmodifyingstreambuf.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kfilesystem.h>

using namespace DEKAF2_NAMESPACE_NAME;

//-----------------------------------------------------------------------------
kurl::kurl ()
//-----------------------------------------------------------------------------
{
	KInit(true) // we want a signal handler thread
		.SetName(s_sProjectName)
		.SetMultiThreading(false)
		.SetOnlyShowCallerOnJsonError();

	m_CLI
		.Throw()
		.SetBriefDescription("dekaf2 based HTTP query tool")
		.SetAdditionalArgDescription("<URL>");

	m_CLI
		.Option(":,next")
		.Help("open next config section")
	([&]()
	{
		m_RequestList.Add(std::move(BuildMRQ));
		BuildMRQ.clear();
	});

	m_CLI
		.Option("i,include")
		.Help("show http response headers in output")
	([&]()
	{
		BuildMRQ.Flags |= Flags::RESPONSE_HEADERS;
	});

	m_CLI
		.Option("k,insecure")
		.Help("do not verify TLS certificates")
	([&]()
	{
		BuildMRQ.Flags |= Flags::INSECURE_CERTS;
	});

	m_CLI
		.Option("p,pretty")
		.Help("pretty print XML or JSON data")
	([&]()
	{
		BuildMRQ.Flags |= Flags::PRETTY;
	});

	m_CLI
		.Option("v,verbose")
		.Help("verbose status output")
	([&]()
	{
		BuildMRQ.Flags |= (Flags::VERBOSE | Flags::REQUEST_HEADERS | Flags::RESPONSE_HEADERS);
	});

	m_CLI
		.Option("K,config")
		.Type(KOptions::ArgTypes::File)
		.Help("specify file to read kurl arguments from")
	([&](KStringViewZ sArg)
	{
		// load defaults first
		LoadConfig();
		m_CLI.ParseFile(sArg);
	});

	m_CLI
		.Option("q,disable")
		.Help("do not read kurlrc")
		.Set(m_bLoadDefaultConfig, false);

	m_CLI
		.Option("s,silent")
		.Help("quiet (silent) operation")
	([&]()
	{
		BuildMRQ.Flags |= Flags::QUIET;
	});

	m_CLI
		.Option("u,user <USER:PASS>", "username and password")
		.Help("set username and password for the request")
	([&](KStringViewZ sUserPass)
	{
		auto Pieces = sUserPass.Split(":");
		if (Pieces.size() != 2)
		{
			throw KOptions::Error("user option must have username and password concatenated by a colon");
		}
		BuildMRQ.sUsername = Pieces[0];
		BuildMRQ.sPassword = Pieces[1];
	});

	m_CLI
		.Option("aws-sigv4 <provider1[:provider2\n            [:region[:service]]]>")
		.Help("set authorization with AWS signature type 4 (requires ID and key set with -u,user). "
			  "provider1 = aws,goog,osc,.., "
			  "provider2 = amz,goog,osc, "
			  "region = e.g. us-west-1, "
			  "service = e.g. translate. "
			  "region and service will be deduced from the URL if missing.")
		.Set(BuildMRQ.sAWSProvider);

	m_CLI
		.Option("basic")
		.Hidden();
//		.Help("force basic authentication for username/password set with -u, --user (default anyway)");

	m_CLI
		.Option("V,version")
		.Help("show version information")
		.Final()
	([&]()
	{
		ShowVersion();
	});

	m_CLI
		.Option("X,request <method>", "request method")
		.Help(kFormat("set request method ({}), default = GET", KHTTPMethod::REQUEST_METHODS))
	([&](KStringViewZ sMethod)
	{
		BuildMRQ.Method = sMethod.ToUpperASCII();

		if (!BuildMRQ.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	m_CLI
		.Option("I,head")
		.Help("issue a HEAD request")
		.Set(BuildMRQ.Method, KHTTPMethod(KHTTPMethod::HEAD));

	m_CLI
		.Option("G,get")
		.Help("issue a GET request, even if there is request data")
		.Set(BuildMRQ.Method, KHTTPMethod(KHTTPMethod::GET));

	m_CLI
		.Option("d,data,data-ascii <content>", "request body")
		.Help("add content as HTML form, or with prefixed @ take contents of file")
	([&](KStringViewZ sArg)
	{
		AddRequestData(sArg, true, true);
	});

	m_CLI
		.Option("data-binary <content>", "request body")
		.Help("add unconverted, binary content, or with prefixed @ take contents of file")
	([&](KStringViewZ sArg)
	{
		AddRequestData(sArg, false, true);
	});

	m_CLI
		.Option("data-raw <content>", "request body")
		.Help("add content as HTML form, do not treat prefixed @ as file")
	([&](KStringViewZ sArg)
	{
		AddRequestData(sArg, true, false);
	});

	m_CLI
		.Option("H,header <key:value>", "request header with key:value")
		.Help("add HTTP header to the request (can be used multiple times)"
			  " or load a file with @filename with one header per line")
	([&](KStringViewZ sHeader)
	{
		if (sHeader.front() == '@')
		{
			auto sHeaderFile = sHeader;
			sHeaderFile.TrimLeft('@');

			KString sHeaders;

			if (!kAppendAll (sHeaderFile, sHeaders))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sHeaderFile));
			}

			for (auto sHeader : sHeaders.Split("\n"))
			{
				auto Pair = kSplitToPair (sHeader, ":");
				if (!Pair.first.empty())
				{
					BuildMRQ.Headers.insert ({Pair.first, Pair.second});
				}
			}
		}
		else
		{
			auto Pair = kSplitToPair (sHeader, ":");
			if (!Pair.first.empty())
			{
				// use a pre-C++17-safe insert_or_assign
				auto p = BuildMRQ.Headers.insert ({Pair.first, Pair.second});
				if (!p.second)
				{
					p.first->second = Pair.second;
				}
			}
		}
	});

	m_CLI
		.Option("referer,referrer <referrer>", "referrer URL")
		.Help("set refer(r)er header for the request")
	([&](KStringViewZ sReferer)
	{
		if (BuildMRQ.Headers.insert ({KHTTPHeader(KHTTPHeader::REFERER).Serialize(), sReferer}).second == false)
		{
			throw KOptions::Error("referer header already set");
		}
	});

	m_CLI
		.Option("A,user-agent <useragent>", "user agent string")
		.Help("set user agent header for the request")
	([&](KStringViewZ sUserAgent)
	{
		if (BuildMRQ.Headers.insert ({KHTTPHeader(KHTTPHeader::USER_AGENT).Serialize(), sUserAgent}).second == false)
		{
			throw KOptions::Error("user agent header already set");
		}
	});

	m_CLI
		.Option("close")
		.Help("set Connection: Close header, and disconnect after RX")
	([&]()
	{
		if (BuildMRQ.Headers.insert ({"Connection", "close"}).second == false)
		{
			throw KOptions::Error("Connection header already set");
		}
		BuildMRQ.Flags |= Flags::CONNECTION_CLOSE;
	});

	m_CLI
		.Option("progress <Wheel|Bar>")
		.MinArgs(0).MaxArgs(1)
		.Help("show progress either as spinning wheel (default), or as a progress bar")
	([&](KOptions::ArgList& Commands)
	{
		if (Commands.empty())
		{
			m_Progress.SetType(Progress::None);
		}
		else if (Commands.pop().ToLower() == "wheel")
		{
			m_Progress.SetType(Progress::Wheel);
		}
		else
		{
			m_Progress.SetType(Progress::Bar);
		}
	});

	m_CLI
		.Option("stats")
		.Help("show statistics")
		.Set(m_Config.bShowStats, true);

	m_CLI
		.Option("timeout <n>")
		.MinArgs(1).MaxArgs(1)
		.Type(KOptions::ArgTypes::Unsigned)
		.Range(0, 1000000)
		.Help("timeout in seconds, default 5")
	([&](KStringViewZ sArg)
	{
		BuildMRQ.iSecondsTimeout = sArg.UInt64();
		m_Config.bShowStats = true;
	});

	m_CLI
		.Option("repeat <n>")
		.MinArgs(1).MaxArgs(1)
		.Type(KOptions::ArgTypes::Unsigned)
		.Help("repeat request <n> times (and show statistics)")
	([&](KStringViewZ sArg)
	{
		m_Config.iRepeat    = sArg.UInt64();
		m_Config.bShowStats = true;
	});

	m_CLI
		.Option("Z,parallel <n>")
		.MinArgs(1).MaxArgs(1)
		.Range(1, 10000)
		.Type(KOptions::ArgTypes::Unsigned)
		.Help("send <n> parallel requests")
		.Set(m_Config.iParallel);

	m_CLI
		.Option("m,mime <MIME>", "request MIME type")
		.Help("set MIME type for the request")
		.Set(BuildMRQ.sRequestMIME);

	m_CLI
		.Option("o,output <file>", "output file name")
		.Help("set output file name for the request")
	([&](KStringViewZ sFile)
	{
		BuildMRQ.AddOutput(sFile);
	});

	m_CLI
		.Option("O")
		.Help("sets output file name to last path part of the URL")
	([&]()
	 {
		BuildMRQ.AddOutput(sOutputToLastPathComponent);
	});

	m_CLI
		.Option("compressed")
		.Help("requests compressed response (with default compressors - this is the default with kurl")
		// empty method string means default compressors
		.Set(BuildMRQ.sRequestCompression, "");

	m_CLI
		.Option("compression <method>", "requested compression")
		.Help(kFormat("set accepted compressions/encodings ({}), or - for no compression",
					  KHTTPCompression::GetImplementedCompressors()))
		.Set(BuildMRQ.sRequestCompression);

	m_CLI
		.Option("no-compressed,uncompressed")
		.Help("do not request a compressed response")
		.Set(BuildMRQ.sRequestCompression, "-");

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	m_CLI
		.Option("unix-socket <socket path>", "path to unix socket")
		.Type(KOptions::ArgTypes::Socket)
		.Help("use <socket path> to connect to unix domain socket. Use 'localhost' as domain name in the URL.")
		.Set(BuildMRQ.sUnixSocket);
#endif

	m_CLI
		.Option("url <URL>", "requested URL")
		.Help("set requested URL")
	([&](KStringViewZ sURL)
	{
		KURL URL(sURL);

		if (URL.Protocol == url::KProtocol::UNDEFINED)
		{
			kDebug(2, "no protocol specified - assuming HTTP");
			URL.Protocol = url::KProtocol::HTTP;
		}

		BuildMRQ.AddURL(std::move(URL));
	});

	m_CLI
		.Option("sim <file>", "input file name")
		.Help("input <file> used as server response")
		.Type(KOptions::ArgTypes::File)
	([&](KStringViewZ sFile)
	{
		BuildMRQ.AddURL(kFormat("file://{}", kNormalizePath(sFile)));
	});

	m_CLI
		.Option("http1,http1.1")
		.Help("force http/1.1 protocol, do not allow upgrade to http/2")
	([&]()
	{
		if (BuildMRQ.Flags & FORCE_HTTP_2)
		{
			throw KOptions::Error("--http1 and --http2 options are mutually exclusive");
		}
		BuildMRQ.Flags |= Flags::FORCE_HTTP_1;
	});

	m_CLI
		.Option("http2")
		.Help("force http/2 protocol, do not allow downgrade to http/1.1")
	([&]()
	{
		if (BuildMRQ.Flags & FORCE_HTTP_1)
		{
			throw KOptions::Error("--http1 and --http2 options are mutually exclusive");
		}
		BuildMRQ.Flags |= Flags::FORCE_HTTP_2;
	});

	m_CLI
		.UnknownCommand([&](KOptions::ArgList& Commands)
	{
		while (!Commands.empty())
		{
			KURL URL(Commands.pop());

			if (URL.Protocol == url::KProtocol::UNDEFINED)
			{
				kDebug(2, "no protocol specified - assuming HTTP");
				URL.Protocol = url::KProtocol::HTTP;
			}

			BuildMRQ.AddURL(std::move(URL));
		}
	});

} // ctor

//-----------------------------------------------------------------------------
void kurl::AddRequestData(KStringViewZ sArg, bool bEncodeAsForm, bool bTakeFile)
//-----------------------------------------------------------------------------
{
	KString sData;

	if (bTakeFile && sArg.StartsWith("@"))
	{
		sArg.TrimLeft('@');

		if (!kReadAll(sArg, sData))
		{
			throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
		}
	}
	else
	{
		if (bEncodeAsForm)
		{
			// TODO
			sData = sArg;
		}
		else
		{
			sData = sArg;
		}
	}

	if (!BuildMRQ.sRequestBody.empty() && !sData.empty())
	{
		BuildMRQ.sRequestBody += '&';
	}

	BuildMRQ.sRequestBody += sData;

	if (BuildMRQ.Method.empty())
	{
		BuildMRQ.Method = KHTTPMethod::POST;
	}

	if (!BuildMRQ.Headers.contains(KHTTPHeader(KHTTPHeader::CONTENT_TYPE).Serialize()))
	{
		BuildMRQ.Headers.insert({KHTTPHeader(KHTTPHeader::CONTENT_TYPE).Serialize(), KMIME::WWW_FORM_URLENCODED});
	}

}; // AddRequestData

//-----------------------------------------------------------------------------
void kurl::LoadConfig ()
//-----------------------------------------------------------------------------
{
	if (!m_bLoadDefaultConfig) return;
	static bool s_bIsLoaded = false;
	if (s_bIsLoaded) return;
	s_bIsLoaded = true;

	// check for default config file:
	// KURL_HOME/.kurlrc
	// HOME/.kurlrc
	// CURL_HOME/.curlrc
	// HOME/.curlrc

	auto CheckForConfig = [&](KStringView sDir, KStringView sFile) -> bool
	{
		if (!sDir.empty())
		{
			auto sPathname = kFormat("{}{}{}", sDir, kDirSep, sFile);
			if (kFileExists(sPathname))
			{
				kDebug(2, "loading config from: {}", sPathname);
				m_CLI.ParseFile(sPathname);
				return true;
			}
		}
		return false;
	};

	if (CheckForConfig(kGetEnv("KURL_HOME"), ".kurlrc")) return;
	if (CheckForConfig(kGetHome()          , ".kurlrc")) return;
//	if (CheckForConfig(kGetEnv("CURL_HOME"), ".curlrc")) return;
//	if (CheckForConfig(kGetHome()          , ".curlrc")) return;

} // LoadConfig

//-----------------------------------------------------------------------------
void kurl::ServerQuery ()
//-----------------------------------------------------------------------------
{
	KWebClient HTTP;

	for (;;)
	{
		auto RQ = m_RequestList.GetNextRequest();

		if (!RQ)
		{
			break;
		}

		KOutFile OutFile;

		// prepare the output
		if (!RQ->sOutput.empty())
		{
			OutFile.open(RQ->sOutput);
		}

		KOutStream Out(!RQ->sOutput.empty() ? OutFile : std::cout);

		// write all line endings that _we_ provoke (with .WriteLine()) in canonical form
		Out.SetWriterEndOfLine("\r\n");

		{
			TLSOptions options = (RQ->URL.Protocol == url::KProtocol::HTTPS) 
									? TLSOptions::DefaultsForHTTP
									: TLSOptions::None;

			if (RQ->Config.Flags & Flags::FORCE_HTTP_2)
			{
				options = TLSOptions::RequestHTTP2;
			}
			else if (RQ->Config.Flags & Flags::FORCE_HTTP_1)
			{
				options = TLSOptions::None;
			}

			if ((RQ->Config.Flags & Flags::INSECURE_CERTS) == 0)
			{
				options |= TLSOptions::VerifyCert;
			}

			HTTP.SetTLSOptions(options);
		}
		
		HTTP.SetTimeout(RQ->Config.iSecondsTimeout);

		if (RQ->Config.sRequestCompression == "-")
		{
			HTTP.RequestCompression(false);
		}
		else
		{
			HTTP.RequestCompression(true, RQ->Config.sRequestCompression);
		}

		for (const auto& Header : RQ->Config.Headers)
		{
			HTTP.AddHeader (Header.first, Header.second);
		}

		if (HTTP.Request.Headers.Contains(KHTTPHeader::USER_AGENT) == false)
		{
			HTTP.AddHeader (KHTTPHeader::USER_AGENT, kFormat("{}/{}", s_sProjectName, s_sProjectVersion));
		}

		KURL SocketURL;

		if (!RQ->Config.sUnixSocket.empty())
		{
			SocketURL.Protocol = url::KProtocol::UNIX;
			SocketURL.Path.get() = RQ->Config.sUnixSocket;
		}

		KStopTime tDuration;

		KString sResponse;

		if (RQ->URL.Protocol != url::KProtocol::FILE)
		{
			sResponse = HTTP.HttpRequest2Host(SocketURL, RQ->URL, RQ->Config.Method, RQ->Config.sRequestBody, RQ->Config.sRequestMIME);
		}
		else
		{
			if (!RQ->URL.Domain.get().empty() && !(RQ->URL.Domain.get() == "localhost" || RQ->URL.Domain.get() == "127.0.0.1"))
			{
				throw KError(kFormat("cannot read non-local file: {}", RQ->URL.Domain.get()));
			}
			KStringViewZ sPath = RQ->URL.Path.get();
			sResponse = kReadAll(sPath);
			HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::CreateByInspection(sPath).Serialize());
		}

		m_Results.Add(HTTP.Response.iStatusCode, tDuration.elapsed());

		m_Progress.ProgressOne();

		if (HTTP.Response.iStatusCode == 0)
		{
			// no connection
			Out.FormatLine("{}: {}", RQ->URL, HTTP.Error());
			continue;
		}

		if (RQ->Config.Flags & Flags::REQUEST_HEADERS)
		{
			KModifyingOutputStreamBuf Modifier(KErr.ostream());
			Modifier.Replace("", "> ");
			HTTP.Request.KHTTPRequestHeaders::Serialize(KErr);
			// actually curl prints both request and response headers on stderr with -v
			Modifier.Replace("", "< ");
			HTTP.Response.KHTTPResponseHeaders::Serialize(KErr);
		}

		if (RQ->Config.Flags & Flags::RESPONSE_HEADERS)
		{
			HTTP.Response.KHTTPResponseHeaders::Serialize(Out);
		}

		bool bPrinted = false;

		if (RQ->Config.Flags & Flags::PRETTY)
		{
			if (HTTP.Response.ContentType() == KMIME::JSON)
			{
				KJSON json;

#if !DEKAF2_KJSON2_IS_DISABLED
				if (json.Parse(sResponse))
#else
				KString sError;
				if (kjson::Parse(json, sResponse, sError))
#endif
				{
#if !DEKAF2_KJSON2_IS_DISABLED
					json.Serialize(Out, true);
#else
					Out.ostream() << std::setw(1) << std::setfill('\t') << json;
#endif
					Out.Write('\n');
					bPrinted = true;
				}
			}
			else if (HTTP.Response.ContentType() == KMIME::XML)
			{
				KXML XML;

				if (XML.Parse(sResponse))
				{
					XML.Serialize(Out);
					bPrinted = true;
				}
			}
		}

		if (!bPrinted)
		{
			Out.Write (sResponse);
		}

		if (RQ->Config.Flags & Flags::CONNECTION_CLOSE)
		{
			HTTP.Disconnect();
		}
	}

} // ServerQuery

//-----------------------------------------------------------------------------
void kurl::ShowVersion ()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);
	KOut.FormatLine(":: {}", Dekaf::GetVersionInformation());

} // ShowVersion

//-----------------------------------------------------------------------------
void kurl::ShowStats (KDuration tTotal, std::size_t iTotalRequests)
//-----------------------------------------------------------------------------
{
	KErr.WriteLine();
	KErr.FormatLine("{:=<50}", "");
	KErr.FormatLine("running with {} threads in parallel", m_Config.iParallel);
	KErr.FormatLine("total {} for {} requests, {:.0f} req/sec",
					tTotal,
					iTotalRequests,
					iTotalRequests / (tTotal.microseconds().count() / 1000000.0));
	KErr.WriteLine();
	KErr.WriteLine(m_Results.Print());
	KErr.FormatLine("{:=<50}", "");

} // ShowStats

//-----------------------------------------------------------------------------
int kurl::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	{
		auto iRetVal = m_CLI.Parse(argc, argv, KOut);

		if (iRetVal	|| m_CLI.Terminate())
		{
			// either error or completed
			return iRetVal;
		}

		// load default config if not already done..
		LoadConfig();
	}

	m_RequestList.Add(std::move(BuildMRQ));
	m_RequestList.CreateRequestList(m_Config.iRepeat);

	if (m_RequestList.empty())
	{
		throw KException ("missing URL");
	}

	auto iTotalRequests = m_Config.iRepeat * m_RequestList.size();

	kDebug(2, "have {} queries for {} threads",
		   iTotalRequests,
		   m_Config.iParallel );

	m_Progress.Start(iTotalRequests);

	KStopTime tStopTotal;

	if (m_Config.iParallel == 1)
	{
		ServerQuery ();
	}
	else
	{
		KRunThreads Threads(m_Config.iParallel);
		Threads.Create(&kurl::ServerQuery, this);
	}

	m_Progress.Finish();

	if (m_Config.bShowStats)
	{
		ShowStats(tStopTotal.elapsed(), iTotalRequests);
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return kurl().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", kurl::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", kurl::s_sProjectName, ex.what());
	}
	return 1;

} // main

//-----------------------------------------------------------------------------
void kurl::MultiRequest::BuildAuthenticationHeader()
//-----------------------------------------------------------------------------
{
	if (!sUsername.empty())
	{
		if (sAWSProvider.empty())
		{
			// basic authentication
			if (Headers.emplace (
				KHTTPHeader(KHTTPHeader::AUTHORIZATION).Serialize(),
				kFormat("Basic {}", KEnc::Base64(kFormat("{}:{}", sUsername, sPassword)))
			).second == false)
			{
				throw KError("basic auth: authentication header already set");
			}
		}
		else
		{
			// AWS Auth v4
			if (URLs.size() != 1)
			{
				throw KError("cannot query multiple URLs with aws-sigv4 option");
			}

			auto Pieces = sAWSProvider.Split(":");

			switch (Pieces.size())
			{
				case 0: 
					Pieces = { "aws", "amz" };
					break;

				case 1: 
					if (Pieces[0] == "aws") 
					{
						Pieces.push_back("amz");
					}
					else
					{
						Pieces.push_back(Pieces[0]);
					}
					break;

				default:
					break;
			}

			while (Pieces.size() < 4)
			{
				Pieces.push_back("");
			}

			AWSAuth4::SignedRequest AWS(URLs.front().first,
										Method,
										"",
										sRequestBody,
										sRequestMIME.Serialize(),
										Pieces[1]);

			auto AHeaders = AWS.Authorize(sUsername,
										  sPassword,
										  Pieces[2],
										  Pieces[3],
										  Pieces[0]);

			for (const auto& Header : AHeaders)
			{
				Headers.emplace(Header);
			}
		}
	}

} // MultiRequest::BuildAuthenticationHeader

//-----------------------------------------------------------------------------
void kurl::BuildMultiRequest::BuildPairsOfURLAndOutput()
//-----------------------------------------------------------------------------
{
	KStringView sLastOut;
	auto it = m_BuildOutputs.begin();

	for (auto& URL : m_BuildURLs)
	{
		if (it != m_BuildOutputs.end())
		{
			sLastOut = *it++;
		}
		if (sLastOut == sOutputToLastPathComponent)
		{
			KString sComponent = kBasename(URL.Path.get());
			URLs.emplace_back(std::move(URL), sComponent);
		}
		else
		{
			URLs.emplace_back(std::move(URL), sLastOut);
		}
	}

	m_BuildURLs.clear();
	m_BuildOutputs.clear();

} // BuildMultiRequest::BuildPairsOfURLAndOutput

//-----------------------------------------------------------------------------
void kurl::RequestList::Add(BuildMultiRequest BMRQ)
//-----------------------------------------------------------------------------
{
	BMRQ.BuildPairsOfURLAndOutput();
	BMRQ.BuildAuthenticationHeader();

	if (!BMRQ.URLs.empty())
	{
		m_MultiRequests.push_back(std::move(BMRQ));
	}

} // RequestList::Add

//-----------------------------------------------------------------------------
void kurl::RequestList::CreateRequestList(std::size_t iRepeat)
//-----------------------------------------------------------------------------
{
	m_iRepeat = iRepeat;

	// now create the single request list out of the multi-requests that may have
	// been configured - we keep the MultiRequests as the data store, and only
	// create references to it in the RequestList
	for (auto& MRQ : m_MultiRequests)
	{
		for (auto& URL : MRQ.URLs)
		{
			m_RequestList.emplace_back(URL.first, URL.second, MRQ);
		}
	}
	it.unique().get() = m_RequestList.begin();

} // RequestList::CreateRequestList

//-----------------------------------------------------------------------------
const kurl::SingleRequest* kurl::RequestList::GetNextRequest()
//-----------------------------------------------------------------------------
{
	auto p = it.unique();

	if (*p == m_RequestList.end())
	{
		if (m_iRepeat == 1)
		{
			return nullptr;
		}
		else
		{
			--m_iRepeat;
			*p = m_RequestList.begin();
		}
	}

	return &*(*p)++;

} // RequestList::GetNextRequest

//-----------------------------------------------------------------------------
void kurl::Results::Add(uint16_t iResultCode, KDuration tDuration)
//-----------------------------------------------------------------------------
{
	auto Codes = m_ResultCodes.unique();
	auto it    = Codes->find(iResultCode);

	if (it == Codes->end())
	{
		auto pair = Codes->insert({iResultCode, Result{}});
		it = pair.first;
	}

	it->second.Add(tDuration);

} // Results::Add

//-----------------------------------------------------------------------------
KString kurl::Results::Print()
//-----------------------------------------------------------------------------
{
	auto Codes = m_ResultCodes.shared();

	KString sResult;

	for (const auto& it : *Codes)
	{
		sResult += kFormat("HTTP {}: count {}, avg {}\n",
						   it.first,
						   kFormNumber(it.second.m_tDuration.Rounds()),
						   it.second.m_tDuration.average());
	}

	return sResult;

} // Results::Print

//-----------------------------------------------------------------------------
void kurl::Progress::Start(std::size_t iSize)
//-----------------------------------------------------------------------------
{
	if (m_ProgressType == Type::Bar)
	{
		m_Bar = std::make_unique<KBAR>(iSize,
									   KBAR::DEFAULT_WIDTH,
									   KBAR::SLIDER,
									   KBAR::DEFAULT_DONE_CHAR,
									   KErr);
	}

} // Progress::Start

//-----------------------------------------------------------------------------
void kurl::Progress::ProgressOne()
//-----------------------------------------------------------------------------
{
	switch (m_ProgressType)
	{
		case Type::None:
			break;
		case Type::Wheel:
			PrintNextWheel();
			break;
		case Type::Bar:
			if (m_Bar)
			{
				m_Bar->Move();
			}
			break;
	}

} // Progress::ProgressOne

//-----------------------------------------------------------------------------
void kurl::Progress::Finish()
//-----------------------------------------------------------------------------
{
	switch (m_ProgressType)
	{
		case Type::None:
		case Type::Wheel:
			break;
		case Type::Bar:
			if (m_Bar)
			{
				m_Bar->Finish();
			}
			break;
	}

} // Progress::Finish

//-----------------------------------------------------------------------------
void kurl::Progress::PrintNextWheel()
//-----------------------------------------------------------------------------
{
	static std::mutex mutex;
	std::lock_guard<std::mutex> lock(mutex);

	KErr.Format("\b{:c}", GetNextWheelChar());

} // Progress::PrintNextWheel

//-----------------------------------------------------------------------------
char kurl::Progress::GetNextWheelChar()
//-----------------------------------------------------------------------------
{
	static constexpr std::array<char, 8> m_Wheel = {'/', '-', '\\', '|', '/', '-', '\\', '|'};

	if (m_WheelChar > 6) m_WheelChar = 0;
	else ++m_WheelChar;
	return m_Wheel[m_WheelChar];

} // Progress::GetNextWheelChar


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView  kurl::sOutputToLastPathComponent;
constexpr KStringViewZ kurl::s_sProjectName;
constexpr KStringViewZ kurl::s_sProjectVersion;
#endif
