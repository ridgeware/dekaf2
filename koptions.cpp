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
#include <iostream>

namespace dekaf2 {

//---------------------------------------------------------------------------
KOptions::CParms::Arg_t::Arg_t(KStringView sArg_)
//---------------------------------------------------------------------------
	: sArg(sArg_)
	, bConsumed {false}
{
	if (sArg.front() == '-')
	{
		if (sArg.size() > 1)
		{
			if (sArg[1] == '-')
			{
				if (sArg.size() > 2)
				{
					sArg.remove_prefix(2);
					bIsSingleDashed = false;
					bIsDoubleDashed = true;
				}
				// double dash, leave alone
			}
			else
			{
				sArg.remove_prefix(1);
				bIsSingleDashed = true;
				bIsDoubleDashed = false;
			}
		}
		// single dash, leave alone
	}
}

//---------------------------------------------------------------------------
KOptions::CParms::CParms(int argc, char** argv)
//---------------------------------------------------------------------------
{
	Args.reserve(argc);

	for (int ii = 0; ii < argc; ++ii)
	{
		Args.push_back(KStringView(*argv++));
	}

	if (!Args.empty())
	{
		sProgramName = Args.front().sArg;
		Args.front().bConsumed = true;
	}

}

//---------------------------------------------------------------------------
KOptions::CParms::~CParms()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
std::unique_ptr<KOptions::CParms> KOptions::CreateParms(int argc, char** argv)
//---------------------------------------------------------------------------
{
	return std::make_unique<CParms>(argc, argv);
}

//---------------------------------------------------------------------------
KOptions::KOptions(int& retval, bool bEmptyParmsIsError, KStringView sCliDebugTo/*=KLog::STDOUT*/)
//---------------------------------------------------------------------------
	: m_retval(&retval)
	, m_bEmptyParmsIsError(bEmptyParmsIsError)
	, m_sCliDebugTo(sCliDebugTo)
{
}

//---------------------------------------------------------------------------
KOptions::~KOptions()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
void KOptions::Help()
//---------------------------------------------------------------------------
{
	for (size_t ct = 0; ct < m_sHelpSize; ++ct)
	{
		std::cout.write(m_sHelp[ct].data(), m_sHelp[ct].size());
		std::cout.write("\n", 1);
	}
}

//---------------------------------------------------------------------------
bool KOptions::Evaluate(KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_Parms->empty() || m_Parms->size() == 1)
	{
		if (m_bEmptyParmsIsError)
		{
			Help();
			return false;
		}
	}

	size_t iUnconsumed {0};
	for (auto& it : *m_Parms)
	{
		if (!it.bConsumed)
		{
			++iUnconsumed;
		}
	}

	if (iUnconsumed)
	{
		out.FormatLine("have {} excess arguments:", iUnconsumed);
		for (auto& it : *m_Parms)
		{
			if (!it.bConsumed)
			{
				if (it.bIsSingleDashed)
				{
					out.Write('-');
				}
				else if (it.bIsDoubleDashed)
				{
					out.Write("--");
				}
				out.WriteLine(it.sArg);
			}
		}

		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
const KOptions::Command_t* KOptions::FindCommand(const CommandStore& Store, KStringView sCommand)
//---------------------------------------------------------------------------
{
	if (!sCommand.empty())
	{
		for (auto& it : Store)
		{
			if (it.sCmd == sCommand)
			{
				return &it.func;
			}
		}
	}

	return nullptr;

} // FindCommand

//---------------------------------------------------------------------------
bool KOptions::Options(int argc, char** argv, KOutStream& out)
//---------------------------------------------------------------------------
{
	m_Parms = CreateParms(argc, argv);

	CParms::iterator lastCommand;

	try
	{

		// using explicit iterators so that children can move the loop forward more than one step
		for (auto it = m_Parms->begin() + 1; it != m_Parms->end(); ++it)
		{
			lastCommand = it;

			if (it->IsOption())
			{
				auto Cmd = FindCommand(m_Options, it->sArg);
				if (Cmd)
				{
					it->bConsumed = true;
					// isolate parms until next command
					ArgList Args;
					auto it2 = it + 1;
					for (; it2 != m_Parms->end() && !it2->IsOption(); ++it2)
					{
						Args.push_back(it2->sArg);
					}

					auto iConsumed = Cmd->operator()(Args);
					if (iConsumed > Args.size())
					{
						// error
						break;
					}
					// advance arg iter
					while (iConsumed-- > 0)
					{
						(++it)->bConsumed = true;
					}
				}
				else
				{
					// children did not evaluate argument
					if (it->sArg == "help")
					{
						Help();
						it->bConsumed = true;
						return false;
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
			else
			{
				auto Cmd = FindCommand(m_Commands, it->sArg);
				if (Cmd)
				{
					it->bConsumed = true;
					ArgList Args;
					Cmd->operator()(Args);
				}
			}
		}

		if (!Evaluate(out))
		{
			SetRetval(1);
			return false;
		}

		return true;
	}

	catch (const MissingParameterError& error)
	{
		out.FormatLine("missing parameter after {} : {}", lastCommand->sArg, error.what());
	}

	catch (const WrongParameterError& error)
	{
		out.FormatLine("wrong parameter after {} : {}", lastCommand->sArg, error.what());
	}

	catch (const std::exception error)
	{
		out.FormatLine("exception at {} : {}", lastCommand->sArg, error.what());
	}

	catch (...)
	{
		out.FormatLine("unknown exception at {}", lastCommand->sArg);
	}

	SetRetval(1);

	return false;

}

//---------------------------------------------------------------------------
void KOptions::SetRetval(int iVal)
//---------------------------------------------------------------------------
{
	if (m_retval)
	{
		if (!*m_retval)
		{
			*m_retval = iVal;
		}
	}
}

} // end of namespace dekaf2
