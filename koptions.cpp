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
#include "dekaf2.h"

namespace dekaf2 {

namespace {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// small RAII helper to make sure we reset an output stream pointer
/// when the reference for it goes out of scope
struct KOutStreamRAII
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	KOutStreamRAII(KOutStream** Var, KOutStream& Stream)
	: m_Old(*Var)
	, m_Var(Var)
	{
		*Var = &Stream;
	}

	~KOutStreamRAII()
	{
		*m_Var = m_Old;
	}

private:

	KOutStream* m_Old;
	KOutStream** m_Var;

}; // KOutStreamRAII

} // end of anonymous namespace

//---------------------------------------------------------------------------
KOptions::OptionalParm::OptionalParm(KOptions& base, KStringView sOption, KStringViewZ sArgDescription, bool bIsCommand)
//---------------------------------------------------------------------------
: CallbackParam(sOption, sArgDescription, bIsCommand ? fIsCommand : fNone)
, m_base(&base)
{
	if (m_iMinArgs == 0 && !m_sMissingArgs.empty())
	{
		m_iMinArgs = 1;
	}
}

//---------------------------------------------------------------------------
KOptions::OptionalParm::~OptionalParm()
//---------------------------------------------------------------------------
{
	m_base->Register(std::move(*this));
}

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::Help(KStringView sHelp)
//---------------------------------------------------------------------------
{
	m_sHelp = sHelp.Trim();
	return *this;

} // Help

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::Range(int64_t iLowerBound, int64_t iUpperBound)
//---------------------------------------------------------------------------
{
	if (iLowerBound > iUpperBound)
	{
		std::swap(iLowerBound, iUpperBound);
	}

	m_iLowerBound  = iLowerBound;
	m_iUpperBound  = iUpperBound;
	m_iFlags      |= fCheckBounds;

	return *this;

} // Range

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::Callback(CallbackN Func)
//---------------------------------------------------------------------------
{
	m_Callback = std::move(Func);
	return *this;
}

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::Callback(Callback1 Func)
//---------------------------------------------------------------------------
{
	Callback([func = std::move(Func)](ArgList& args)
	{
		func(args.pop());
	});

	m_iMinArgs = 1;
	m_iMaxArgs = 1;

	return *this;
}

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::Callback(Callback0 Func)
//---------------------------------------------------------------------------
{
	Callback([func = std::move(Func)](ArgList& unused)
	{
		func();
	});

	m_iMinArgs = 0;
	m_iMaxArgs = 0;

	return *this;
}

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
void KOptions::CLIParms::Create(int argc, char const* const* argv)
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
		m_sProgramPathName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CLIParms::Create

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
		m_sProgramPathName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CLIParms::Create

//---------------------------------------------------------------------------
KStringViewZ KOptions::CLIParms::GetProgramPath() const
//---------------------------------------------------------------------------
{
	return m_sProgramPathName;

} // CLIParms::GetProgramPath

//---------------------------------------------------------------------------
KStringView KOptions::CLIParms::GetProgramName() const
//---------------------------------------------------------------------------
{
	return kBasename(GetProgramPath());

} // CLIParms::GetProgramName


//---------------------------------------------------------------------------
KOptions::KOptions(bool bEmptyParmsIsError, KStringView sCliDebugTo/*=KLog::STDOUT*/, bool bThrow/*=false*/)
//---------------------------------------------------------------------------
	: m_bThrow(bThrow)
	, m_bEmptyParmsIsError(bEmptyParmsIsError)
{
	SetBriefDescription("KOptions based option parsing");
	SetMaxHelpWidth(120);
	SetWrappedHelpIndent(0);

	Option("help")
	.Help("this text")
	([this]()
	{
		auto& out = GetCurrentOutputStream();

		if (m_sHelp)
		{
			for (size_t iCount = 0; iCount < m_iHelpSize; ++iCount)
			{
				out.WriteLine(m_sHelp[iCount]);
			}
		}
		else
		{
			BuildHelp(out);
		}

		// and abort further parsing
		throw NoError {};
	});

	Option("d0,d,dd,ddd")
	.Help("increasing optional stdout debug levels")
	([this,sCliDebugTo]()
	{
		auto sArg = GetCurrentArg();
		auto iLevel = (sArg == "d0") ? 0 : sArg.size();
		KLog::getInstance().SetLevel    (iLevel);
		KLog::getInstance().SetDebugLog (sCliDebugTo);
		KLog::getInstance().KeepCLIMode (true);
		kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
	});

	Option("dgrep,dgrepv <regex>", "grep expression")
	.Help("search (not) for grep expression in debug output")
	([this,sCliDebugTo](KStringViewZ sGrep)
	{
		bool bIsInverted = GetCurrentArg() == "dgrepv";
		
		// if no -d option has been applied yet switch to -ddd
		if (KLog::getInstance().GetLevel() <= 0)
		{
			KLog::getInstance().SetLevel (3);
			kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
		}
		KLog::getInstance().SetDebugLog (sCliDebugTo);
		kDebug (1, "debug {} set to: '{}'", bIsInverted	? "egrep -v" : "egrep", sGrep);
		KLog::getInstance().LogWithGrepExpression(true, bIsInverted, sGrep);
		KLog::getInstance().KeepCLIMode (true);
	});

	Option("ini <filename>", "ini file name")
	.Help("load options from ini file")
	([this](KStringViewZ sIni)
	{
		if (ParseFile(sIni, KOut))
		{
			// error was already displayed - just abort parsing
			throw NoError {};
		}
	});

} // KOptions ctor

namespace {

//---------------------------------------------------------------------------
KStringView WrapOutput(KStringView& sInput, std::size_t iMaxSize)
//---------------------------------------------------------------------------
{
	auto sWrapped = sInput;

	if (sWrapped.size() > iMaxSize)
	{
		sWrapped.erase(iMaxSize);

		if (!(sInput.size() > iMaxSize && KASCII::kIsSpace(sInput[sWrapped.size()])))
		{
			sWrapped.erase(sWrapped.find_last_of(detail::kASCIISpaces));
		}
		else
		{
			// do not erase the last part of sWrapped if it ended in a space,
			// instead only remove trailing spaces
			sWrapped.TrimRight();
		}
	}

	if (sWrapped.empty())
	{
		if (!sInput.empty())
		{
			// this is only possible due to a logic error,
			// make sure we do not loop forever
			kDebugLog(1, "KOptions::WrapOutput() resulted in an empty string");
			sWrapped = sInput;
		}
	}

	sInput.remove_prefix(sWrapped.size());
	sInput.TrimLeft();

	return sWrapped;

} // WrapOutput

} // end of anonymous namespace

//---------------------------------------------------------------------------
void KOptions::BuildHelp(KOutStream& out) const
//---------------------------------------------------------------------------
{
	std::size_t iMaxLen { 0 };

	for (const auto& Callback : m_Callbacks)
	{
		auto iSize = (Callback.m_sNames.front() == '-') ? Callback.m_sNames.size() - 1 : Callback.m_sNames.size();
		iMaxLen = std::max(iMaxLen, iSize);
	}

	out.WriteLine();
	out.Format("{} -- ", GetProgramName());

	{
		auto iIndent = GetProgramName().size() + 4;
		auto sDescription = GetBriefDescription();

		auto sLimited = WrapOutput(sDescription, m_iMaxHelpRowWidth - iIndent);
		out.WriteLine(sLimited);

		iIndent += m_iWrappedHelpIndent;

		while (sDescription.size())
		{
			auto iSpaces = iIndent;
			while (iSpaces--)
			{
				out.Write(' ');
			}

			auto sLimited = WrapOutput(sDescription, m_iMaxHelpRowWidth - iIndent);
			out.WriteLine(sLimited);
		}
	}

	out.WriteLine();
	out.FormatLine("usage: {}{}{}",
				   GetProgramName(),
				   m_bHaveOptions ? " [<options>]" : "",
				   m_bHaveCommands ? " [<actions>]" : "");
	out.WriteLine();

	auto iMaxHelp  = m_iMaxHelpRowWidth - (iMaxLen + m_sSeparator.size() + 5);
	// format wrapped help texts so that they start earlier at the left if
	// argument size is bigger than help size
	bool bOverlapping = iMaxHelp < iMaxLen;
	// kFormat crashes when we insert the spacing parm at the same time with
	// the actual parms, therefore we prepare the format strings before inserting
	// the parms
	auto sFormat   = kFormat("   {{}}{{:<{}}} {{}}{} {{}}", iMaxLen, m_sSeparator);
	auto sOverflow = kFormat("  {{:<{}}}  {{}}",
							 bOverlapping
							 ? 1 + m_iWrappedHelpIndent // we need the + 1 to avoid a total of 0 which would crash kFormat
							 : 2 + iMaxLen + m_sSeparator.size() + m_iWrappedHelpIndent);

	auto Show = [&](bool bCommands)
	{
		for (const auto& Callback : m_Callbacks)
		{
			if (Callback.IsCommand() == bCommands && Callback.m_sNames != "!")
			{
				bool bFirst   = true;
				auto iHelp    = iMaxHelp;
				auto sHelp    = Callback.m_sHelp;

				if (sHelp.empty())
				{
					sHelp     = Callback.m_sMissingArgs;
				}

				while (bFirst || sHelp.size())
				{
					auto sLimited = WrapOutput(sHelp, iHelp);

					if (bFirst)
					{
						bFirst = false;
						out.FormatLine(sFormat,
									   (!Callback.IsCommand() && Callback.m_sNames.front() != '-') ? "-" : "",
									   Callback.m_sNames,
									   bCommands ? " " : "",
									   sLimited);

						if (bOverlapping)
						{
							iHelp = m_iMaxHelpRowWidth - (1 + m_iWrappedHelpIndent);
						}
						else
						{
							iHelp -= m_iWrappedHelpIndent;
						}
					}
					else
					{
						out.FormatLine(sOverflow, "", sLimited);
					}
				}
			}
		}
	};

	if (m_bHaveOptions)
	{
		out.WriteLine("where <options> are:");
		Show(false);
		out.WriteLine();
	}

	if (m_bHaveCommands)
	{
		out.FormatLine("{} <actions> are:", m_bHaveOptions ? "and" : "where");
		Show(true);
		out.WriteLine();
	}

} // BuildHelp

//---------------------------------------------------------------------------
void KOptions::Help(KOutStream& out)
//---------------------------------------------------------------------------
{
	KOutStreamRAII KO(&m_CurrentOutputStream, out);

	// we have a help option registered through the constructor
	auto Callback = FindParam("help", true);

	if (!Callback)
	{
		SetError("no help registered");
	}
	else
	{
		try
		{
			// yes - call the function with empty args
			ArgList Args;
			Callback->m_Callback(Args);
		}
		DEKAF2_CATCH (const NoError& error)
		{
#ifdef _MSC_VER
			error.what();
#endif
		}
	}

} // Help

//---------------------------------------------------------------------------
KOptions::OptionalParm KOptions::Option(KStringView sOption, KStringViewZ sArgDescription)
//---------------------------------------------------------------------------
{
	return OptionalParm(*this, sOption, sArgDescription, false);
}

//---------------------------------------------------------------------------
KOptions::OptionalParm KOptions::Command(KStringView sCommand, KStringViewZ sArgDescription)
//---------------------------------------------------------------------------
{
	return OptionalParm(*this, sCommand, sArgDescription, true);
}

//---------------------------------------------------------------------------
const KOptions::CallbackParam* KOptions::FindParam(KStringView sName, bool bIsOption) const
//---------------------------------------------------------------------------
{
	auto& Lookup = bIsOption ? m_Options : m_Commands;
	auto it = Lookup.find(sName);
	if (it == Lookup.end() || it->second >= m_Callbacks.size())
	{
		return nullptr;
	}
	auto Callback = &m_Callbacks[it->second];
	// mark option as used
	Callback->m_bUsed = true;
	return Callback;

} // FindParam

//---------------------------------------------------------------------------
void KOptions::Register(CallbackParam OptionOrCommand)
//---------------------------------------------------------------------------
{
	auto& Lookup = OptionOrCommand.IsCommand() ? m_Commands : m_Options;
	auto  iIndex = m_Callbacks.size();

	for (auto sOption : OptionOrCommand.m_sNames.Split())
	{
		if (sOption != "!")
		{
			sOption.TrimLeft(OptionOrCommand.IsCommand() ? " \f\v\t\r\n\b" : " -\f\v\t\r\n\b");

			if (sOption.front() == '-')
			{
				SetError(kFormat("a command, other than an option, may not start with - : {}", sOption));
				return;
			}

			// strip name at first special character or space
			auto pos = sOption.find_first_of(" <>[]|=\t\r\n\b");

			if (pos != KStringView::npos)
			{
				sOption.erase(pos);
			}

			if (OptionOrCommand.IsCommand())
			{
				m_bHaveCommands = true;
			}
			else
			{
				m_bHaveOptions = true;
			}
		}

		kDebug(3, "adding option: '{}'", sOption);

		auto Pair = Lookup.insert( { sOption, iIndex } );

		if (!Pair.second)
		{
			kDebug(1, "overriding existing {}: {}", OptionOrCommand.IsCommand() ? "command" : "option", sOption);
			Pair.first->second = iIndex;
		}
	}

	m_Callbacks.push_back(std::move(OptionOrCommand));

	if (m_Callbacks.size() != iIndex+1)
	{
		SetError("registration race");
	}

} // Register

//---------------------------------------------------------------------------
void KOptions::RegisterUnknownOption(CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam("!", CallbackParam::fNone, 0, "", Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterUnknownCommand(CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam("!", CallbackParam::fIsCommand, 0, "", Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOptions, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sOptions, CallbackParam::fNone, iMinArgs, sMissingParms, Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommands, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sCommands, CallbackParam::fIsCommand, iMinArgs, sMissingParms, Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterOption(sOption, 0, "", [func = std::move(Function)](ArgList& unused)
	{
		func();
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, Callback0 Function)
//---------------------------------------------------------------------------
{
	RegisterCommand(sCommand, 0, "", [func = std::move(Function)](ArgList& unused)
	{
		func();
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	RegisterOption(sOption, 1, sMissingParm, [func = std::move(Function)](ArgList& args)
	{
		func(args.pop());
	});
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	RegisterCommand(sCommand, 1, sMissingParm, [func = std::move(Function)](ArgList& args)
	{
		func(args.pop());
	});
}

//---------------------------------------------------------------------------
int KOptions::Parse(int argc, char const* const* argv, KOutStream& out)
//---------------------------------------------------------------------------
{
	return Execute(CLIParms(argc, argv), out);

} // Parse

//---------------------------------------------------------------------------
int KOptions::Parse(KString sCLI, KOutStream& out)
//---------------------------------------------------------------------------
{
	kDebug (1, sCLI);

	// create a permanent buffer of the passed CLI
	m_ParmBuffer.push_front(std::move(sCLI));

	std::vector<KStringViewZ> parms;
	kSplitArgsInPlace(parms, m_ParmBuffer.front(), /*svDelim  =*/" \f\v\t\r\n\b", /*svQuotes =*/"\"'", /*chEscape =*/'\\');

	return Execute(CLIParms(parms), out);

} // Parse

//-----------------------------------------------------------------------------
int KOptions::Parse(KInStream& In, KOutStream& out)
//-----------------------------------------------------------------------------
{
	KString sCLI = kFirstNonEmpty(GetProgramPath(),
								  Dekaf::getInstance().GetProgName(),
								  "Unnamed");

	for (auto& sLine : In)
	{
		sLine.Trim();

		if (!sLine.empty() && sLine.front() != '#')
		{
			sCLI += ' ';
			sCLI += sLine;
		}
	}

	return Parse(std::move(sCLI), out);

} // Parse

//-----------------------------------------------------------------------------
int KOptions::ParseFile(KStringViewZ sFileName, KOutStream& out)
//-----------------------------------------------------------------------------
{
	KInFile InFile(sFileName);

	if (!InFile.is_open())
	{
		return SetError(kFormat("cannot open input file: {}", sFileName));
	}

	return Parse(InFile, out);

} // ParseFile

//---------------------------------------------------------------------------
int KOptions::ParseCGI(KStringViewZ sProgramName, KOutStream& out)
//---------------------------------------------------------------------------
{
	url::KQuery Query;

	// parse and dequote the query string parameters
	Query.Parse(kGetEnv(KCGIInStream::QUERY_STRING));

	// create a permanent buffer for the strings, as the rest of
	// KOptions operates on string views
	m_ParmBuffer.push_front(sProgramName);

	// create a vector of string views pointing to the string buffers
	std::vector<KStringViewZ> QueryArgs;
	QueryArgs.push_back(m_ParmBuffer.front());

	for (const auto& it : Query.get())
	{
		m_ParmBuffer.push_front(std::move(it.first));
		QueryArgs.push_back(m_ParmBuffer.front());

		if (!it.second.empty())
		{
			m_ParmBuffer.push_front(std::move(it.second));
			QueryArgs.push_back(m_ParmBuffer.front());
		}
	}

	kDebug(2, "parsed {} arguments from CGI query string: {}", QueryArgs.size() - 1, kJoined(QueryArgs, " "));

	return Execute(CLIParms(QueryArgs), out);

} // ParseCGI

//---------------------------------------------------------------------------
bool KOptions::IsCGIEnvironment()
//---------------------------------------------------------------------------
{
	return !kGetEnv(KCGIInStream::REQUEST_METHOD).empty();
}

//---------------------------------------------------------------------------
KOutStream& KOptions::GetCurrentOutputStream()
//---------------------------------------------------------------------------
{
	if (m_CurrentOutputStream)
	{
		return *m_CurrentOutputStream;
	}
	else
	{
		return KOut;
	}

} // GetCurrentOutputStream

//---------------------------------------------------------------------------
int KOptions::SetError(KStringViewZ sError, KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_bThrow)
	{
		throw KException(sError);
	}
	else
	{
		out.WriteLine(sError);
	}

	return 1;

} // SetError

//---------------------------------------------------------------------------
KString KOptions::BadBoundsReason(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const
//---------------------------------------------------------------------------
{
	switch (Type)
	{
		case Integer:
		case Float:
			return kFormat("{} is not in limits [{}..{}]", sParm, iMinBound, iMaxBound);

		case Unsigned:
			return kFormat("{} is not in limits [{}..{}]", sParm, static_cast<uint64_t>(iMinBound), static_cast<uint64_t>(iMaxBound));

		case String:
		case File:
		case Path:
		case Directory:
		case Email:
		case URL:
			return kFormat("length of {} not in limits [{}..{}]", sParm.size(), iMinBound, iMaxBound);

		default:
			break;
	}

	return {};

} // BadBoundsReason

//---------------------------------------------------------------------------
bool KOptions::ValidBounds(ArgTypes Type, KStringView sParm, int64_t iMinBound, int64_t iMaxBound) const
//---------------------------------------------------------------------------
{
	switch (Type)
	{
		case Integer:
		{
			auto iValue = sParm.Int64();
			return iValue >= iMinBound && iValue <= iMaxBound;
		}

		case Unsigned:
		{
			auto iValue = sParm.UInt64();
			return iValue >= static_cast<uint64_t>(iMinBound) && iValue <= static_cast<uint64_t>(iMaxBound);
		}

		case Float:
		{
			auto iValue = sParm.Double();
			return iValue >= iMinBound && iValue <= iMaxBound;
		}

		case String:
		case File:
		case Path:
		case Directory:
		case Email:
		case URL:
		{
			auto iValue = static_cast<int64_t>(sParm.size());
			return iValue >= iMinBound && iValue <= iMaxBound;
		}

		default:
			break;
	}

	return true;

} // ValidBounds

//---------------------------------------------------------------------------
KString KOptions::BadArgReason(ArgTypes Type, KStringView sParm) const
//---------------------------------------------------------------------------
{
	switch (Type)
	{
		case Integer:
			return kFormat("not an integer: {}", sParm);

		case Unsigned:
			return kFormat("not an unsigned integer: {}", sParm);

		case Float:
			return kFormat("not a float: {}", sParm);

		case Boolean:
			return kFormat("not a boolean: {}", sParm);

		case String:
			return kFormat("not a string: {}", sParm);

		case File:
			return kFormat("not an existing file: {}", sParm);

		case Directory:
			return kFormat("not an existing directory: {}", sParm);

		case Path:
			return kFormat("not an existing directory base: {}", sParm);

		case Email:
			return kFormat("not an email: {}", sParm);

		case URL:
			return kFormat("not a URL: {}", sParm);
	}

	return {};

} // BadArgReason

//---------------------------------------------------------------------------
bool KOptions::ValidArgType(ArgTypes Type, KStringViewZ sParm) const
//---------------------------------------------------------------------------
{
	switch (Type)
	{
		case Integer:
			return kIsInteger(sParm);

		case Unsigned:
			return kIsUnsigned(sParm);

		case Float:
			return kIsFloat(sParm) || kIsInteger(sParm);

		case Boolean:
			return true;

		case String:
			return true;

		case File:
			return kFileExists(sParm);

		case Directory:
			return kDirExists(sParm);

		case Path:
		{
			KString sDirname = kDirname(sParm);
			return kDirExists(sDirname);
		}

		case Email:
			return kIsEmail(sParm);

		case URL:
			return kIsURL(sParm);
	}

	return true;

} // ValidArgType

//---------------------------------------------------------------------------
KStringViewZ KOptions::ModifyArgument(KStringViewZ sArg, const CallbackParam* Callback)
//---------------------------------------------------------------------------
{
	if (Callback->ToLower())
	{
		m_ParmBuffer.push_front(sArg.ToLower());
	}
	else if (Callback->ToUpper())
	{
		m_ParmBuffer.push_front(sArg.ToUpper());
	}
	else
	{
		return sArg;
	}

	return m_ParmBuffer.front().ToView();

} // ModifyArgument

//---------------------------------------------------------------------------
int KOptions::Execute(CLIParms Parms, KOutStream& out)
//---------------------------------------------------------------------------
{
	KOutStreamRAII KO(&m_CurrentOutputStream, out);

	if (m_sProgramPathName.empty())
	{
		m_sProgramPathName = Parms.GetProgramPath();
	}

	CLIParms::iterator lastCommand;

	DEKAF2_TRY
	{

		if (!Parms.empty())
		{
			// using explicit iterators so that options can move the loop forward more than one step
			for (auto it = Parms.begin() + 1; it != Parms.end(); ++it)
			{
				lastCommand = it;
				ArgList Args;
				bool bIsUnknown { false };
				m_sCurrentArg = it->sArg;

				auto Callback = FindParam(it->sArg, it->IsOption());

				if (DEKAF2_UNLIKELY(Callback == nullptr))
				{
					// check if we have a handler for an unknown arg
					Callback = FindParam("!", it->IsOption());

					if (Callback)
					{
						// we pass the current Arg as the first arg of Args,
						// but we need to take care to not take it into account
						// when we readjust the remaining args after calling the
						Args.push_front(it->sArg);
						bIsUnknown = true;
					}
				}

				if (Callback)
				{
					it->bConsumed = true;

					auto iMaxArgs = Callback->m_iMaxArgs;

					// isolate parms until next command and add them to the ArgList
					for (auto it2 = it + 1; iMaxArgs-- > 0 && it2 != Parms.end() && !it2->IsOption(); ++it2)
					{
						Args.push_front(ModifyArgument(it2->sArg, Callback));

						if (Args.size() <= Callback->m_iMinArgs)
						{
							// check minimum arguments for compliance with the requested type
							if (!ValidArgType(Callback->m_ArgType, Args.front()))
							{
								DEKAF2_THROW(WrongParameterError(BadArgReason(Callback->m_ArgType, Args.front())));
							}
							else if (Callback->CheckBounds() && !ValidBounds(Callback->m_ArgType, Args.front(), Callback->m_iLowerBound, Callback->m_iUpperBound))
							{
								DEKAF2_THROW(WrongParameterError(BadBoundsReason(Callback->m_ArgType, Args.front(), Callback->m_iLowerBound, Callback->m_iUpperBound)));
							}
						}
					}

					if (Callback->m_iMinArgs > Args.size())
					{
						if (!Callback->m_sMissingArgs.empty())
						{
							DEKAF2_THROW(MissingParameterError(Callback->m_sMissingArgs));
						}
						DEKAF2_THROW(MissingParameterError(kFormat("{} arguments required, but only {} found", Callback->m_iMinArgs, Args.size())));
					}

					// keep record of the initial args count
					auto iOldSize = Args.size();

					// finally call the callback
					Callback->m_Callback(Args);

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
			}
		}
		
		return Evaluate(Parms, out);
	}

	DEKAF2_CATCH (const MissingParameterError& error)
	{
		return SetError(kFormat("{}: missing parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()));
	}

	DEKAF2_CATCH (const WrongParameterError& error)
	{
		return SetError(kFormat("{}: wrong parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()));
	}

	DEKAF2_CATCH (const BadOptionError& error)
	{
		return SetError(kFormat("{}: {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()));
	}

	DEKAF2_CATCH (const Error& error)
	{
		return SetError(error.what());
	}

	DEKAF2_CATCH (const NoError& error)
	{
#ifdef _MSC_VER
		error.what();
#endif
	}

	return 1;

} // Execute

//---------------------------------------------------------------------------
int KOptions::Evaluate(const CLIParms& Parms, KOutStream& out)
//---------------------------------------------------------------------------
{
	if (Parms.size() < 2)
	{
		if (m_bEmptyParmsIsError)
		{
			Help(out);
			return -1;
		}
	}

	bool bError { false };

	size_t iUnconsumed {0};

	for (const auto& it : Parms)
	{
		if (!it.bConsumed)
		{
			++iUnconsumed;
		}
	}

	if (iUnconsumed)
	{
		out.FormatLine("have {} excess argument{}:", iUnconsumed, iUnconsumed == 1 ? "" : "s");

		for (const auto& it : Parms)
		{
			if (!it.bConsumed)
			{
				out.Write(it.Dashes());
				out.WriteLine(it.sArg);
			}
		}

		bError = true;
	}

	// now check for required options
	for (auto& Callback : m_Callbacks)
	{
		if (Callback.IsRequired() && !Callback.m_bUsed)
		{
			bError = true;
			out.FormatLine("missing required argument: {}{}", Callback.IsCommand() ? "" : "-", Callback.m_sNames);
		}
		else
		{
			// reset flag
			Callback.m_bUsed = false;
		}
	}

	return bError; // 0 or 1

} // Evaluate

//---------------------------------------------------------------------------
KStringViewZ KOptions::GetProgramPath() const
//---------------------------------------------------------------------------
{
	return m_sProgramPathName;

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
