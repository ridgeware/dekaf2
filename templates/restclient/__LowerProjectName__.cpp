
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/rest/framework/krestclient.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

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
void __ProjectName__::ServerQuery ()
//-----------------------------------------------------------------------------
{
	KJsonRestClient HTTP (m_Config.URL);

	HTTP.SetVerifyCerts (m_Config.bVerifyCerts);
	HTTP.SetTimeout (m_Config.Timeout);

	for (const auto& Header : m_Config.Headers)
	{
		HTTP.AddHeader (Header.first, Header.second);
	}

	// returns JSON, may throw
	auto sResponse = HTTP.Verb(m_Config.Method)
	                     .Request(m_Config.jBody);

	auto sOutput = m_Config.bPretty ? sResponse.dump(1, '\t') : sResponse.dump();

	if (m_Config.sOutFile.empty())
	{
		kWriteLine (sOutput);
	}
	else if (!kWriteFile (m_Config.sOutFile, sOutput))
	{
		throw KException(kFormat("cannot write to {}", m_Config.sOutFile));
	}

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
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription       ("dekaf2 __ProjectType__ template")
			// .SetHelpSeparator          ("::")             // the column separator between option and help text
			// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
			// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
			.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
			//.SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
		;

		KStringViewZ              sMethod      = Options("X,request <method> : set request method (default = GET)", "GET");
		KStringViewZ              sData        = Options("D,data [@]<data>   : add literal request body, or with @ take contents of file", "");
		std::vector<KStringViewZ> Headers      = Options("H,header <header>  : add HTTP header to the request (can be used multiple times)", std::vector<KStringViewZ>{}).MaxArgs(1);
		m_Config.bVerifyCerts                  =!Options("k,insecure         : do not verify the TLS certificate of the server", false);
		m_Config.Timeout                       = chrono::seconds(Options("timeout <seconds> : connect and read timeout (default 15)", 15).Range(1, 3600).UInt16());
		m_Config.sOutFile                      = Options("o,output <file>    : write the response to <file> instead of stdout", "");
		m_Config.bPretty                       = Options("pretty             : pretty print the JSON response", false);
		bool                      bShowVersion = Options("version            : show version information", false);

		// ---- insert project specific options here ----

		// the URL is the only positional argument
		auto URLs = Options.GetUnknownCommands();

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return 1;
		}

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}

		if (URLs.size() > 1)
		{
			throw KException("multiple URLs");
		}

		if (!URLs.empty())
		{
			m_Config.URL = URLs.front();
		}

		m_Config.Method = sMethod.ToUpperASCII();

		if (!m_Config.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KException(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}

		if (!sData.empty())
		{
			KString sJSON;

			if (sData.starts_with('@'))
			{
				sData.remove_prefix(1);

				if (!kReadAll (sData, sJSON))
				{
					throw KException(kFormat("invalid filename: {}", sData));
				}
			}
			else
			{
				sJSON = sData;
			}

			KString sError;
			if (!kjson::Parse(m_Config.jBody, sJSON, sError))
			{
				throw KException("input is not valid JSON");
			}
		}

		for (auto sHeader : Headers)
		{
			auto Pair = kSplitToPair (sHeader, ":");
			m_Config.Headers.insert ({Pair.first, Pair.second});
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
