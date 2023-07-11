
#include "__LowerProjectName__.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/krestclient.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	KInit()
		.SetName(s_sProjectName)
		.SetMultiThreading()
		.SetOnlyShowCallerOnJsonError();
}

//-----------------------------------------------------------------------------
void __ProjectName__::SetupOptions (KOptions& Options)
//-----------------------------------------------------------------------------
{
	Options
		.Throw()
		.SetBriefDescription       ("__LowerProjectName__ -- dekaf2 __ProjectType__ template")
		// .SetHelpSeparator          ("::")             // the column separator between option and help text
		// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
		// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
		.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
		//.SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
	;

	Options
		.UnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (!m_Config.URL.empty() || Commands.size() > 1)
		{
			throw KOptions::Error("multiple URLs");
		}
		m_Config.URL = Commands.pop();
	});

	Options
		.Option("X,request <method>", "request_method")
		.ToUpper()
		.Help("set request method (default = GET)")
	([&](KStringViewZ sMethod)
	{
		m_Config.Method = sMethod.ToUpperASCII();

		if (!m_Config.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	Options
		.Option("D,data [@]<data>", "request_body")
		.Help("add literal request body, or with @ take contents of file")
	([&](KStringViewZ sArg)
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

	Options
		.Option("H,header <header>", "header with key:value")
		.Help("add HTTP header to the request (can be used multiple times)")
	([&](KStringViewZ sHeader)
	{
		auto Pair = kSplitToPair (sHeader, ":");
		m_Config.Headers.insert ({Pair.first, Pair.second});
	});

	Options
		.Option("version")
		.Help("show version information")
		.Stop()
	([&]()
	{
		ShowVersion();
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

	kPrintLine (sResponse.dump());

} // ServerQuery

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion ()
//-----------------------------------------------------------------------------
{
	kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		KOptions Options(true);
		SetupOptions(Options);

		auto iRetVal = Options.Parse(argc, argv, KOut);

		if (Options.Terminate() || iRetVal)
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
