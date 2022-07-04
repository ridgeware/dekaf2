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

using namespace dekaf2;

//-----------------------------------------------------------------------------
kurl::kurl ()
//-----------------------------------------------------------------------------
{
	KInit().SetName(s_sProjectName).SetMultiThreading().SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI
		.SetBriefDescription("dekaf2 based HTTP query tool")
		.SetAdditionalArgDescription("<URL>");

	m_CLI
		.Option("i,include")
		.Help("show http response headers in output")
	([&]()
	{
		m_Config.Flags |= Flags::RESPONSE_HEADERS;
	});

	m_CLI
		.Option("k,insecure")
		.Help("do not verify SSL/TLS certificates")
	([&]()
	{
		m_Config.Flags |= Flags::INSECURE_CERTS;
	});

	m_CLI
		.Option("p,pretty")
		.Help("pretty print XML or JSON data")
	([&]()
	{
		m_Config.Flags |= Flags::PRETTY;
	});

	m_CLI
		.Option("v,verbose")
		.Help("verbose status output")
	([&]()
	{
		m_Config.Flags |= (Flags::VERBOSE | Flags::REQUEST_HEADERS | Flags::RESPONSE_HEADERS);
	});

	m_CLI
		.Option("q,quiet")
		.Help("quiet operation")
	([&]()
	{
		m_Config.Flags |= Flags::QUIET;
	});

	m_CLI
		.Option("V,version")
		.Help("show version information")
	([&]()
	{
		ShowVersion();
		m_Config.bTerminate = true;
	});

	m_CLI
		.Option("X,request <method>", "request method")
		.Help(kFormat("set request method ({}) of simulated request (default = GET)", KHTTPMethod::REQUEST_METHODS))
	([&](KStringViewZ sMethod)
	{
		m_Config.Method = sMethod.ToUpperASCII();

		if (!m_Config.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	m_CLI
		.Option("D,data <content>", "request body")
		.Help("add literal request body, or with @ take contents of file")
	([&](KStringViewZ sArg)
	{
		if (sArg.StartsWith("@"))
		{
			sArg.TrimLeft('@');

			if (!kReadAll (sArg, m_Config.sRequestBody))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
			}
		}
		else
		{
			m_Config.sRequestBody = sArg;
		}
	});

	m_CLI
		.Option("H,header <key:value>", "request header with key:value")
		.Help("add HTTP header to the request (can be used multiple times)")
	([&](KStringViewZ sHeader)
	{
		auto Pair = kSplitToPair (sHeader, ":");
		m_Config.Headers.insert ({Pair.first, Pair.second});
	});

	m_CLI
		.Option("m,mime <MIME>", "request MIME type")
		.Help("set MIME type for the request")
	([&](KStringViewZ sMIME)
	{
		m_Config.sRequestMIME = sMIME;
	});

	m_CLI
		.Option("compression <method>", "request compression")
		.Help("set accepted compressions/encodings")
	([&](KStringViewZ sCompression)
	{
		m_Config.sRequestCompression = sCompression;
	});

	m_CLI
		.UnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (!m_Config.URL.empty() || Commands.size() > 1)
		{
			throw KOptions::Error("multiple URLs");
		}
		m_Config.URL = Commands.pop();

		if (m_Config.URL.Protocol == url::KProtocol::UNDEFINED)
		{
			kDebug(2, "no protocol specified - assuming HTTP");
			m_Config.URL.Protocol = url::KProtocol::HTTP;
		}
	});

} // ctor

//-----------------------------------------------------------------------------
void kurl::ServerQuery ()
//-----------------------------------------------------------------------------
{
	KWebClient HTTP (!(m_Config.Flags & Flags::INSECURE_CERTS));

	if (m_Config.sRequestCompression == "-")
	{
		HTTP.RequestCompression(false);
	}
	else
	{
		HTTP.RequestCompression(true, m_Config.sRequestCompression);
	}

	for (const auto& Header : m_Config.Headers)
	{
		HTTP.AddHeader (Header.first, Header.second);
	}

	auto sResponse = HTTP.HttpRequest(m_Config.URL, m_Config.Method, m_Config.sRequestBody, m_Config.sRequestMIME);

	if (m_Config.Flags & Flags::REQUEST_HEADERS)
	{
		HTTP.Request.KHTTPRequestHeaders::Serialize(KOut, "> ");
	}

	if (m_Config.Flags & Flags::RESPONSE_HEADERS)
	{
		HTTP.Response.KHTTPResponseHeaders::Serialize(KOut, "< ");
	}

	bool bPrinted = false;

	if (m_Config.Flags & Flags::PRETTY)
	{
		if (HTTP.Response.ContentType() == KMIME::JSON)
		{
			auto json = kjson::Parse(sResponse);

			if (!json.is_null())
			{
				KOut.WriteLine(json.dump(1, '\t'));
				bPrinted = true;
			}
		}
		else if (HTTP.Response.ContentType() == KMIME::XML)
		{
			KXML XML;

			if (XML.Parse(sResponse))
			{
				XML.Serialize(KOut);
				bPrinted = true;
			}
		}
	}

	if (!bPrinted)
	{
		KOut.WriteLine (sResponse);
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

		if (iRetVal	|| m_Config.bTerminate)
		{
			// either error or completed
			return iRetVal;
		}
	}

	// ---- insert project code here ----

	if (m_Config.URL.empty())
	{
		throw KException ("missing URL");
	}

	ServerQuery ();

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
