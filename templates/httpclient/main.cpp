
#include "__LowerProjectName__.h"
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kwebclient.h>

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"__LowerProjectName__ -- dekaf2 __ProjectType__ template",
	"",
	"usage: __LowerProjectName__ [<options>] <URL>",
	"",
	"where <options> are:",
	"   -help                  :: this text",
	"   -version,rev,revision  :: show version information",
	"   -d[d[d]]               :: different levels of debug messages",
	"   -X, --request <method> :: set request method of simulated request (default = GET)",
	"   -D, --data [@]<data>   :: add literal request body, or with @ take contents of file",
	"   -M, --mime <mimetype>  :: set mime type for request body (default = application/json)",
	"   -H, --header <header>  :: add HTTP header to the request (can be used multiple times)",
	""
};

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	m_CLI.RegisterHelp(g_Help);

	m_CLI.RegisterUnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (!m_Config.URL.empty() || Commands.size() > 1)
		{
			throw KOptions::Error("multiple URLs");
		}
		m_Config.URL = Commands.pop();
	});

	m_CLI.RegisterOption("version,rev,revision", [&]()
	{
		ShowVersion();
		m_Config.bTerminate = true;
	});

	m_CLI.RegisterOption("X,request", "request_method", [&](KStringViewZ sMethod)
	{
		m_Config.Method = sMethod.ToUpperASCII();

		if (!m_Config.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	m_CLI.RegisterOption("D,data", "request_body", [&](KStringViewZ sArg)
	{
		if (sArg.StartsWith("@"))
		{
			sArg.TrimLeft('@');

			if (!kReadAll (sArg, m_Config.sBody))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
			}
		}
		else
		{
			m_Config.sBody = sArg;
		}
	});

	m_CLI.RegisterOption("M,mime", "mime type", [&](KStringViewZ sMimeType)
	{
		m_Config.MimeType = sMimeType;
	});

	m_CLI.RegisterOption("H,header", "header with key:value", [&](KStringViewZ sHeader)
	{
		auto Pair = kSplitToPair (sHeader, ":");
		m_Config.Headers.insert ({Pair.first, Pair.second});
	});

} // ctor

//-----------------------------------------------------------------------------
void __ProjectName__::ServerQuery ()
//-----------------------------------------------------------------------------
{
	KWebClient HTTP;

	for (const auto& Header : m_Config.Headers)
	{
		HTTP.AddHeader (Header.first, Header.second);
	}

	// does not throw, returns string (page)
	auto sResponse = HTTP.HttpRequest (m_Config.URL, m_Config.Method, m_Config.sBody, m_Config.MimeType);

	// check success
	if (!HTTP.HttpSuccess())
	{
		KErr.WriteLine (HTTP.Error());
	}
	else
	{
		KOut.WriteLine (sResponse);
	}

} // ServerQuery

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion ()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine (":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto iResult = m_CLI.Parse (argc, argv, KOut);

	if (iResult || m_Config.bTerminate)
	{
		return iResult;
	}

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
		return __ProjectName__().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}
	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
