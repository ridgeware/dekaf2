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
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kwebclient.h>
#include <dekaf2/kxml.h>
#include <dekaf2/kthreadpool.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
kurl::kurl ()
//-----------------------------------------------------------------------------
{
	KInit()
		.SetName(s_sProjectName)
		.SetMultiThreading(false)
		.SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI
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
		.Help("do not verify SSL/TLS certificates")
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
		.Option("q,quiet")
		.Help("quiet operation")
	([&]()
	{
		BuildMRQ.Flags |= Flags::QUIET;
	});

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
	([&]()
	{
		BuildMRQ.Method = KHTTPMethod::HEAD;
	});

	m_CLI
		.Option("get")
		.Help("issue a GET request, even if there is request data")
	([&]()
	{
		BuildMRQ.Method = KHTTPMethod::GET;
	});

	m_CLI
		.Option("D,data <content>", "request body")
		.Help("add literal request body, or with @ take contents of file")
	([&](KStringViewZ sArg)
	{
		if (sArg.StartsWith("@"))
		{
			sArg.TrimLeft('@');

			if (!kAppendAll (sArg, BuildMRQ.sRequestBody))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
			}
		}
		else
		{
			BuildMRQ.sRequestBody += sArg;
		}

		if (BuildMRQ.Method.empty())
		{
			BuildMRQ.Method = KHTTPMethod::POST;
		}
	});

	m_CLI
		.Option("H,header <key:value>", "request header with key:value")
		.Help("add HTTP header to the request (can be used multiple times)")
	([&](KStringViewZ sHeader)
	{
		auto Pair = kSplitToPair (sHeader, ":");
		BuildMRQ.Headers.insert ({Pair.first, Pair.second});
	});

	m_CLI
		.Option("A,user-agent <useragent>", "user agent string")
		.Help("set user agent header for the request")
	([&](KStringViewZ sUserAgent)
	{
		if (BuildMRQ.Headers.insert ({"User-Agent", sUserAgent}).second == false)
		{
			throw KOptions::Error("user agent header already set");
		}
	});

	m_CLI
		.Option("force-close")
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
		.Option("repeat <n>").MinArgs(1).MaxArgs(1).Type(dekaf2::KOptions::ArgTypes::Unsigned)
		.Help("repeat request <n> times")
	([&](KStringViewZ sArg)
	{
		m_Config.iRepeat = sArg.UInt64();
		kDebug(2, "repeat count set to {}", sArg.UInt64());
	});

	m_CLI
		.Option("Z,parallel <n>").MinArgs(1).MaxArgs(1).Type(dekaf2::KOptions::ArgTypes::Unsigned)
		.Help("send <n> parallel requests")
	([&](KStringViewZ sArg)
	 {
		m_Config.iParallel = sArg.UInt64();
		kDebug(2, "parallel count set to {}", m_Config.iParallel);
	});

	m_CLI
		.Option("m,mime <MIME>", "request MIME type")
		.Help("set MIME type for the request")
	([&](KStringViewZ sMIME)
	{
		BuildMRQ.sRequestMIME = sMIME;
	});

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
		.Option("compression <method>", "requested compression")
		.Help("set accepted compressions/encodings, or - for no compression")
	([&](KStringViewZ sCompression)
	{
		BuildMRQ.sRequestCompression = sCompression;
	});

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	m_CLI
		.Option("unix-socket <socket path>", "path to unix socket")
		.Type(KOptions::ArgTypes::Socket)
		.Help("use <socket path> to connect to unix domain socket. Use 'localhost' as domain name in the URL.")
	([&](KStringViewZ sSocket)
	{
		BuildMRQ.sUnixSocket = sSocket;
	});
#endif

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
void kurl::ServerQuery (std::size_t iRepeat)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP;

	for (;;)
	{
		auto RQ = m_RequestList.GetNextRequest();

		if (!RQ)
		{
			if (iRepeat-- == 1)
			{
				break;
			}
			m_RequestList.ResetRequest();
			continue;
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

		HTTP.VerifyCerts(!(RQ->Config.Flags & Flags::INSECURE_CERTS));

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

		KURL SocketURL;

		if (!RQ->Config.sUnixSocket.empty())
		{
			SocketURL.Protocol = url::KProtocol::UNIX;
			SocketURL.Path.get() = RQ->Config.sUnixSocket;
		}

		auto sResponse = HTTP.HttpRequest2Host(SocketURL, RQ->URL, RQ->Config.Method, RQ->Config.sRequestBody, RQ->Config.sRequestMIME);

		if (RQ->Config.Flags & Flags::REQUEST_HEADERS)
		{
			HTTP.Request.KHTTPRequestHeaders::Serialize(KErr, "> ");
			// actually curl prints both request and response headers on stderr with -v
			HTTP.Response.KHTTPResponseHeaders::Serialize(KErr, "< ");
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

				if (json.Parse(sResponse))
				{
					json.Serialize(Out, true);
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
int kurl::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		auto iRetVal = m_CLI.Parse(argc, argv, KOut);

		if (iRetVal	|| m_CLI.Terminate())
		{
			// either error or completed
			return iRetVal;
		}
	}

	m_RequestList.Add(std::move(BuildMRQ));
	m_RequestList.CreateRequestList();

	if (m_RequestList.empty())
	{
		throw KException ("missing URL");
	}

	if (m_Config.iParallel == 1)
	{
		ServerQuery (m_Config.iRepeat.shared().get());
	}
	else
	{
		KThreadPool Threads(m_Config.iParallel);

		Threads.pause(true);

		for (auto i = m_Config.iParallel; i > 0; --i)
		{
			Threads.push(&kurl::ServerQuery, this, 1);
		}

		kDebug(2, "have {} queries for {} threads", *m_Config.iRepeat.shared(), Threads.size() );
		
		Threads.pause(false);
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

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ kurl::s_sProjectName;
constexpr KStringViewZ kurl::s_sProjectVersion;
#endif
