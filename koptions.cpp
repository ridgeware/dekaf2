/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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
 //
 */

#include "koptions.h"
#include "kstream.h"
#include "klog.h"
#include "ksplit.h"
#include "kfilesystem.h"
#include "kstringutils.h"
#include "kcgistream.h"
#include "kurl.h"
#include "khttp_header.h"

namespace dekaf2 {

//---------------------------------------------------------------------------
KOptions::CLIParms::Arg_t::Arg_t(KStringViewZ sArg_)
//---------------------------------------------------------------------------
	: sArg(sArg_)
	, bConsumed { false }
	, iDashes { 0 }
{
	if (sArg.front() == '-')
	{
		if (sArg.size() > 1 && !KASCII::kIsDigit(sArg[1]))
		{
			if (sArg[1] == '-')
			{
				if (sArg.size() > 2)
				{
					sArg.remove_prefix(2);
					iDashes = 2;
				}
				// double dash, leave alone
			}
			else
			{
				sArg.remove_prefix(1);
				iDashes = 1;
			}
		}
		// single dash or negative number, leave alone
	}

} // Arg_t ctor

//---------------------------------------------------------------------------
KStringViewZ KOptions::CLIParms::Arg_t::Dashes() const
//---------------------------------------------------------------------------
{
	KStringViewZ sReturn { s_sDoubleDash };
	sReturn.remove_prefix(2 - iDashes);

	return sReturn;

} // Dashes

//---------------------------------------------------------------------------
void KOptions::CLIParms::Create(int argc, char** argv)
//---------------------------------------------------------------------------
{
	m_ArgVec.clear();
	m_ArgVec.reserve(argc);

	for (int ii = 0; ii < argc; ++ii)
	{
		m_ArgVec.push_back(KStringViewZ(*argv++));
	}

	if (!m_ArgVec.empty())
	{
		m_sProgramName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CParms ctor

//---------------------------------------------------------------------------
void KOptions::CLIParms::Create(const std::vector<KStringViewZ>& parms)
//---------------------------------------------------------------------------
{
	m_ArgVec.clear();
	m_ArgVec.reserve(parms.size());

	for (auto it : parms)
	{
		m_ArgVec.push_back(it);
	}

	if (!m_ArgVec.empty())
	{
		m_sProgramName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CParms ctor

//---------------------------------------------------------------------------
void KOptions::CLIParms::Create(const std::vector<KString>& parms)
//---------------------------------------------------------------------------
{
	m_ArgVec.clear();
	m_ArgVec.reserve(parms.size());

	for (auto& it : parms)
	{
		KStringViewZ zz = it;
		m_ArgVec.push_back(zz);
	}

	if (!m_ArgVec.empty())
	{
		m_sProgramName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CParms ctor

//---------------------------------------------------------------------------
KOptions::KOptions(bool bEmptyParmsIsError, KStringView sCliDebugTo/*=KLog::STDOUT*/, bool bThrow/*=false*/)
//---------------------------------------------------------------------------
	: m_sCliDebugTo(sCliDebugTo)
	, m_bEmptyParmsIsError(bEmptyParmsIsError)
	, m_bThrow(bThrow)
{
} // KOptions ctor

//---------------------------------------------------------------------------
void KOptions::Help(KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_sHelp)
	{
		for (size_t iCount = 0; iCount < m_iHelpSize; ++iCount)
		{
			out.WriteLine(m_sHelp[iCount]);
		}
	}
	else
	{
		// check if we have a help option registered
		auto cbi = m_Options.find("help");
		if (cbi == m_Options.end())
		{
			DEKAF2_THROW (Error("no help registered"));
		}
		// yes - call the function with empty args
		ArgList Args;
		cbi->second.func(Args);
	}

} // Help

//---------------------------------------------------------------------------
int KOptions::Evaluate(KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_CLIParms.size() < 2)
	{
		if (m_bEmptyParmsIsError)
		{
			Help(out);
			return -1;
		}
	}

	size_t iUnconsumed {0};

	for (auto& it : m_CLIParms)
	{
		if (!it.bConsumed)
		{
			++iUnconsumed;
		}
	}

	if (iUnconsumed)
	{
		out.FormatLine("have {} excess argument{}:", iUnconsumed, iUnconsumed == 1 ? "" : "s");

		for (auto& it : m_CLIParms)
		{
			if (!it.bConsumed)
			{
				out.Write(it.Dashes());
				out.WriteLine(it.sArg);
			}
		}

		return 1;
	}

	return 0;

} // Evaluate

//---------------------------------------------------------------------------
void KOptions::RegisterUnknownOption(CallbackN Function)
//---------------------------------------------------------------------------
{
	m_UnknownOption.func = std::move(Function);
}

//---------------------------------------------------------------------------
void KOptions::RegisterUnknownCommand(CallbackN Function)
//---------------------------------------------------------------------------
{
	m_UnknownCommand.func = std::move(Function);
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOptions, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	for (auto sOption : sOptions.Split())
	{
		// we cannot move Function here as it may get stored multiple times
		m_Options.insert({sOption, { iMinArgs, sMissingParms, Function }});
	}
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommands, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	for (auto sCommand : sCommands.Split())
	{
		// we cannot move Function here as it may get stored multiple times
		m_Commands.insert({sCommand, { iMinArgs, sMissingParms, Function }});
	}
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterOption(sOption, 0, "", [Function](ArgList& unused)
	{
		Function();
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterCommand(sCommand, 0, "", [Function](ArgList& unused)
	{
		Function();
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	RegisterOption(sOption, 1, sMissingParm, [Function](ArgList& args)
	{
		Function(args.pop());
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	RegisterCommand(sCommand, 1, sMissingParm, [Function](ArgList& args)
	{
		Function(args.pop());
	});
}

//---------------------------------------------------------------------------
bool KOptions::IsCGIEnvironment() const
//---------------------------------------------------------------------------
{
	return !kGetEnv(KCGIInStream::REQUEST_METHOD).empty();
}

//---------------------------------------------------------------------------
int KOptions::ParseCGI(KStringViewZ sProgramName, KOutStream& out)
//---------------------------------------------------------------------------
{
	url::KQuery Query;

	Query.Parse(kGetEnv(KCGIInStream::QUERY_STRING));

	std::vector<KStringViewZ> QueryArgs;

	QueryArgs.push_back(sProgramName);

	for (const auto& it : Query.get())
	{
		QueryArgs.emplace_back(it.first);
		if (!it.second.empty())
		{
			QueryArgs.emplace_back(it.second);
		}
	}

	m_CLIParms.Create(QueryArgs);

	kDebug(2, "parsed {} arguments from CGI query string", QueryArgs.size() - 1);

	return Execute(out);

} // ParseCGI

//---------------------------------------------------------------------------
int KOptions::Parse(int argc, char** argv, KOutStream& out)
//---------------------------------------------------------------------------
{
	m_CLIParms.Create(argc, argv);

	return Execute(out);

} // Parse

//---------------------------------------------------------------------------
int KOptions::Parse(KStringView sAltCLI, KOutStream& out)
//---------------------------------------------------------------------------
{
	kDebug (1, sAltCLI);

	std::vector<KString> parms;
	kSplit (parms, sAltCLI, /*delim=*/" ", /*trim=*/" \t\r\n\b", /*escape=*/0, /*bCombineDelimiters=*/true);
	m_CLIParms.Create(parms);
	return Execute(out);

} // Parse

//---------------------------------------------------------------------------
int KOptions::Execute(KOutStream& out)
//---------------------------------------------------------------------------
{
	CLIParms::iterator lastCommand;
	KString sError;

	DEKAF2_TRY
	{

		if (!m_CLIParms.empty())
		{
			// using explicit iterators so that options can move the loop forward more than one step
			for (auto it = m_CLIParms.begin() + 1; it != m_CLIParms.end(); ++it)
			{
				lastCommand = it;
				ArgList Args;
				CallbackParams* Callback { nullptr };
				bool bIsUnknown { false };

				auto& Store = it->IsOption() ? m_Options : m_Commands;
				m_sCurrentArg = it->sArg;
				auto cbi = Store.find(KStringView(it->sArg));
				if (DEKAF2_UNLIKELY(cbi == Store.end()))
				{
					// check if we have a handler for an unknown arg
					if (it->IsOption())
					{
						if (m_UnknownOption.func)
						{
							Callback = &m_UnknownOption;
							// we pass the current Arg as the first arg of Args,
							// but we need to take care to not take it into account
							// when we readjust the remaining args after calling the
							// callback
							Args.push_front(it->sArg);
							bIsUnknown = true;
						}
					}
					else
					{
						if (m_UnknownCommand.func)
						{
							Callback = &m_UnknownCommand;
							// we pass the current Arg as the first arg of Args,
							// but we need to take care to not take it into account
							// when we readjust the remaining args after calling the
							// callback
							Args.push_front(it->sArg);
							bIsUnknown = true;
						}
					}
				}
				else
				{
					Callback = &cbi->second;
				}

				if (Callback)
				{
					it->bConsumed = true;

					// isolate parms until next command and add them to the ArgList
					for (auto it2 = it + 1; it2 != m_CLIParms.end() && !it2->IsOption(); ++it2)
					{
						Args.push_front(it2->sArg);
					}

					if (Callback->iMinArgs > Args.size())
					{
						if (!Callback->sMissingParms.empty())
						{
							DEKAF2_THROW(MissingParameterError(Callback->sMissingParms));
						}
						DEKAF2_THROW(MissingParameterError(kFormat("{} arguments required, but only {} found", Callback->iMinArgs, Args.size())));
					}

					// keep record of the initial args count
					auto iOldSize = Args.size();

					// finally call the callback
					Callback->func(Args);

					if (iOldSize < Args.size())
					{
						DEKAF2_THROW(WrongParameterError("callback manipulated parameter count"));
					}

					if (bIsUnknown)
					{
						// adjust arg count
						--iOldSize;
					}

					// advance arg iter by count of consumed args
					while (iOldSize-- > Args.size())
					{
						(++it)->bConsumed = true;
					}
				}
				else if (it->IsOption())
				{
					// argument was not evaluated
					if (it->sArg == "help")
					{
						Help(out);
						it->bConsumed = true;
						return -1;
					}

					if (it->sArg.In("d,dd,ddd"))
					{
						it->bConsumed = true;
						KLog::getInstance().SetLevel (static_cast<int>(it->sArg.size()));
						KLog::getInstance().SetDebugLog (m_sCliDebugTo);
						KLog::getInstance().KeepCLIMode (true);
						kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
					}
					else if (it->sArg == "d0")
					{
						it->bConsumed = true;
						KLog::getInstance().SetLevel (0);
						KLog::getInstance().SetDebugLog (m_sCliDebugTo);
						KLog::getInstance().KeepCLIMode (true);
					}
					else if (it->sArg.In("dgrep,dgrepv"))
					{
						it->bConsumed = true;

						bool bIsInverted = it->sArg == "dgrepv";

						// check if we have a followup argument (the grep string)
						if (++it == m_CLIParms.end())
						{
							DEKAF2_THROW(MissingParameterError("need argument (grep expression)"));
						}
						it->bConsumed = true;

						// if no -d option has been applied yet switch to -ddd
						if (KLog::getInstance().GetLevel() <= 0)
						{
							KLog::getInstance().SetLevel (3);
							kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
						}
						KLog::getInstance().SetDebugLog (m_sCliDebugTo);
						kDebug (1, "debug {} set to: '{}'", bIsInverted	? "egrep -v" : "egrep", it->sArg);
						KLog::getInstance().LogWithGrepExpression(true, bIsInverted, it->sArg);
						KLog::getInstance().KeepCLIMode (true);
					}
				}
			}
		}
		
		return Evaluate(out);
	}

	DEKAF2_CATCH (const MissingParameterError& error)
	{
		sError.Format("{}: missing parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what());
	}

	DEKAF2_CATCH (const WrongParameterError& error)
	{
		sError.Format("{}: wrong parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what());
	}

	DEKAF2_CATCH (const BadOptionError& error)
	{
		sError.Format("{}: {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what());
	}

	DEKAF2_CATCH (const Error& error)
	{
		sError = error.what();
	}

	DEKAF2_CATCH (const NoError& error)
	{
#ifdef _MSC_VER
		error.what();
#endif
	}

	if (m_bThrow)
	{
		 throw KException(sError);
	}
	else
	{
		out.WriteLine(sError);
	}

	return 1;

} // Execute


//---------------------------------------------------------------------------
KStringViewZ KOptions::GetProgramPath() const
//---------------------------------------------------------------------------
{
	return m_CLIParms.m_sProgramName;

} // GetProgramPath

//---------------------------------------------------------------------------
KStringView KOptions::GetProgramName() const
//---------------------------------------------------------------------------
{
	return kBasename(GetProgramPath());

} // GetProgramName



#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
	constexpr KStringViewZ KOptions::CLIParms::Arg_t::s_sDoubleDash;
#endif

} // end of namespace dekaf2
