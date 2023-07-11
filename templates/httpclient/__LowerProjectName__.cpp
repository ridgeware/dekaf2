
#include "__LowerProjectName__.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kwebclient.h>

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

	Options.
		Option("M,mime <mime>", "mime type, default = application/json")
	([&](KStringViewZ sMimeType)
	{
		m_Config.MimeType = sMimeType;
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
	kPrintLine (":: {} v{}", s_sProjectName, s_sProjectVersion);

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
