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
		if (sArg.size() > 1)
		{
			// this equivalent for sArg[1] is necessary to satisfy gcc 6.3
			if (sArg.operator[](1) == '-')
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
		// single dash, leave alone
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
KOptions::KOptions(bool bEmptyParmsIsError, KStringView sCliDebugTo/*=KLog::STDOUT*/)
//---------------------------------------------------------------------------
	: m_sCliDebugTo(sCliDebugTo)
	, m_bEmptyParmsIsError(bEmptyParmsIsError)
{
} // KOptions ctor

//---------------------------------------------------------------------------
void KOptions::Help(KOutStream& out)
//---------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(m_sHelp == nullptr))
	{
		DEKAF2_THROW (Error("no help registered"));
	}

	for (size_t iCount = 0; iCount < m_iHelpSize; ++iCount)
	{
		out.WriteLine(m_sHelp[iCount]);
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
	m_UnknownOption.func = Function;
}

//---------------------------------------------------------------------------
void KOptions::RegisterUnknownCommand(CallbackN Function)
//---------------------------------------------------------------------------
{
	m_UnknownCommand.func = Function;
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOptions, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	std::vector<KStringView> Options;
	kSplit(Options, sOptions);
	for (auto sOption : Options)
	{
		m_Options.insert({sOption, {iMinArgs, sMissingParms, Function}});
	}
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommands, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	std::vector<KStringView> Commands;
	kSplit(Commands, sCommands);
	for (auto sCommand : Commands)
	{
		m_Commands.insert({sCommand, {iMinArgs, sMissingParms, Function}});
	}
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterOption(sOption, 0, "", [Function](ArgList&)
	{
		Function();
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterCommand(sCommand, 0, "", [Function](ArgList&)
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
int KOptions::Parse(int argc, char** argv, KOutStream& out)
//---------------------------------------------------------------------------
{
	m_CLIParms.Create(argc, argv);

	CLIParms::iterator lastCommand;

	DEKAF2_TRY
	{

		if (!m_CLIParms.empty())
		{
			// using explicit iterators so that options can move the loop forward more than one step
			for (auto it = m_CLIParms.begin() + 1; it != m_CLIParms.end(); ++it)
			{
				lastCommand = it;
				ArgList Args;
				CallbackParams* CBP { nullptr };
				bool bIsUnknown { false };

				auto& Store = it->IsOption() ? m_Options : m_Commands;
				auto cbi = Store.find(it->sArg);
				if (DEKAF2_UNLIKELY(cbi == Store.end()))
				{
					// check if we have a handler for an unknown arg
					if (it->IsOption())
					{
						if (m_UnknownOption.func)
						{
							CBP = &m_UnknownOption;
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
							CBP = &m_UnknownCommand;
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
					CBP = &cbi->second;
				}

				if (CBP)
				{
					it->bConsumed = true;
					// isolate parms until next command and add them to the ArgList
					auto it2 = it + 1;
					for (; it2 != m_CLIParms.end() && !it2->IsOption(); ++it2)
					{
						Args.push_front(it2->sArg);
					}

					if (CBP->iMinArgs > Args.size())
					{
						if (!CBP->sMissingParms.empty())
						{
							DEKAF2_THROW(MissingParameterError(CBP->sMissingParms.c_str()));
						}
						else
						{
							DEKAF2_THROW(MissingParameterError(kFormat("{} arguments required, but only {} found", CBP->iMinArgs, Args.size())));
						}
					}

					// keep record of the initial args count
					auto iOldSize = Args.size();

					// finally call the callback
					CBP->func(Args);

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
					else if (it->sArg.In("d,dd,ddd"))
					{
						it->bConsumed = true;
						KLog().SetLevel (it->sArg.size());
						KLog().SetDebugLog (m_sCliDebugTo);
						kDebug (1, "debug level set to: {}", KLog().GetLevel());
					}
				}
			}
		}
		
		return Evaluate(out);
	}

	DEKAF2_CATCH (const MissingParameterError& error)
	{
		out.FormatLine("{}: missing parameter after {}{}: {}", kBasename(m_CLIParms.m_sProgramName), lastCommand->Dashes(), lastCommand->sArg, error.what());
	}

	DEKAF2_CATCH (const WrongParameterError& error)
	{
		out.FormatLine("{}: wrong parameter after {}{}: {}", kBasename(m_CLIParms.m_sProgramName), lastCommand->Dashes(), lastCommand->sArg, error.what());
	}

	DEKAF2_CATCH (const Error& error)
	{
		out.WriteLine(error.what());
	}

	DEKAF2_CATCH (const NoError& error)
	{
	}

	DEKAF2_CATCH (const KException& error)
	{
		out.WriteLine(error.what());
	}

	return 1;

} // Parse


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
	constexpr KStringViewZ KOptions::CLIParms::Arg_t::sDoubleDash;
#endif

} // end of namespace dekaf2
