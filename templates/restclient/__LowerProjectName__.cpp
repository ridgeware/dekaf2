
#include "__LowerProjectName__.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/krestclient.h>

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
	"   -H, --header <header>  :: add HTTP header to the request (can be used multiple times)",
	""
};

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	KInit().SetName(s_sProjectName).SetMultiThreading().SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

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
		KString sJSON;

		if (sArg.StartsWith("@"))
		{
			sArg.TrimLeft('@');

			if (!kReadAll (sArg, sJSON))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
			}
		}
		else
		{
			sJSON = sArg;
		}

		KString sError;
		if (!kjson::Parse(m_Config.jBody, sJSON, sError))
		{
			throw KException("input is not valid JSON");
		}
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
	KJsonRestClient HTTP (m_Config.URL);

	for (const auto& Header : m_Config.Headers)
	{
		HTTP.AddHeader (Header.first, Header.second);
	}

	// returns JSON, may throw
	auto sResponse = HTTP.Verb(m_Config.Method)
	                     .Request(m_Config.jBody);

	KOut.WriteLine (sResponse.dump());

} // ServerQuery

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion ()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
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
		return __ProjectName__().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
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
