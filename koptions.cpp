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
#include "koutstringstream.h"

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
KOptions::CallbackParam::CallbackParam(KStringView sNames, KStringView sMissingArgs, uint16_t fFlags, uint16_t iMinArgs, CallbackN Func)
//---------------------------------------------------------------------------
: m_Callback     ( std::move(Func) )
, m_sNames       ( sNames          )
, m_sMissingArgs ( sMissingArgs    )
, m_iMinArgs     ( iMinArgs        )
, m_iFlags       ( fFlags          )
{
	if (m_iMinArgs == 0 && !m_sMissingArgs.empty())
	{
		m_iMinArgs = 1;
	}

} // CallbackParam ctor

//---------------------------------------------------------------------------
KOptions::OptionalParm::~OptionalParm()
//---------------------------------------------------------------------------
{
	m_base->Register(std::move(*this));

} // OptionalParm dtor

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::IntSection(KStringView sSection)
//---------------------------------------------------------------------------
{
	// generate a sub-section
	CallbackParam Section;

	Section.m_iFlags    = CallbackParam::fIsSection;

	if (IsCommand())
	{
		Section.m_iFlags |= fIsCommand;
	}

	Section.m_sNames    = sSection;
	Section.m_iHelpRank = m_iHelpRank;

	m_base->Register(std::move(Section));

	return *this;

} // OptionalParm::IntSection

//---------------------------------------------------------------------------
KOptions::OptionalParm& KOptions::OptionalParm::IntHelp(KStringView sHelp, uint16_t iHelpRank)
//---------------------------------------------------------------------------
{
	m_sHelp     = sHelp.Trim();
	m_iHelpRank = iHelpRank;
	return *this;

} // OptionalParm::IntHelp

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

} // OptionalParm::Range

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

} // OptionalParm::Callback

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

} // OptionalParm::Callback

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

} // CLIParms::Arg_t ctor

//---------------------------------------------------------------------------
KStringViewZ KOptions::CLIParms::Arg_t::Dashes() const
//---------------------------------------------------------------------------
{
	KStringViewZ sReturn { s_sDoubleDash };
	sReturn.remove_prefix(2 - iDashes);

	return sReturn;

} // CLIParms::Arg_t::Dashes

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
KStringView KOptions::HelpFormatter::HelpParams::GetProgramName() const
//---------------------------------------------------------------------------
{
	return kBasename(GetProgramPath());

} // HelpFormatter::HelpParams::GetProgramName

//---------------------------------------------------------------------------
KStringView KOptions::HelpFormatter::SplitAtLinefeed(KStringView& sInput)
//---------------------------------------------------------------------------
{
	KStringView sHead;
	auto iBreak = sInput.find('\n');

	if (iBreak == KStringView::npos)
	{
		sHead = sInput;
		sInput.clear();
	}
	else
	{
		sHead = sInput.ToView(0, iBreak);
		sInput.remove_prefix(iBreak + 1);
	}

	return sHead;

} // HelpFormatter::SplitAtLinefeed

//---------------------------------------------------------------------------
KStringView::size_type KOptions::HelpFormatter::AdjustPos(KStringView::size_type iPos, int iAdjust)
//---------------------------------------------------------------------------
{
	if (iPos == KStringView::npos)
	{
		iPos = 0;
	}
	else
	{
		iPos = static_cast<KStringView::size_type>(static_cast<ssize_t>(iPos) + iAdjust);
	}
	return iPos;

} // HelpFormatter::AdjustPos

//---------------------------------------------------------------------------
KStringView KOptions::HelpFormatter::WrapOutput(KStringView& sInput, std::size_t iMaxSize, bool bKeepLineFeeds)
//---------------------------------------------------------------------------
{
	auto sWrapped = sInput;

	auto iBreak = sWrapped.find('\n');

	if (iBreak != KStringView::npos)
	{
		sWrapped.erase(iBreak);
	}

	if (sWrapped.size() > iMaxSize)
	{
		sWrapped.erase(iMaxSize);

		if (!(sInput.size() > iMaxSize && KASCII::kIsSpace(sInput[sWrapped.size()])))
		{
			auto iLastSpace = AdjustPos(sWrapped.find_last_of(detail::kASCIISpaces), 0);
			auto iLastPunct = AdjustPos(sWrapped.find_last_of("-.,;:|?!/"), 1);

			if (sInput.size() - iLastSpace < iMaxSize)
			{
				// we prefer wrapping at spaces, and particularly so when the
				// remainder would then fit into the next line without further
				// wrapping (as we will have two lines anyways)
				sWrapped.erase(iLastSpace);
			}
			else
			{
				// wrap at spaces, or at punctuation if that results in at least
				// twice as many characters
				sWrapped.erase((iLastPunct > iLastSpace + iLastSpace / 2) ? iLastPunct : iLastSpace);
			}
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
			if (!bKeepLineFeeds || sInput.front() != '\n')
			{
				// this is only possible due to a logic error,
				// make sure we do not loop forever
				kDebugLog(1, "KOptions::WrapOutput() resulted in an empty string");
				sWrapped = sInput;
			}
		}
	}

	sInput.remove_prefix(sWrapped.size());

	if (bKeepLineFeeds)
	{
		if (sInput.front() == '\n')
		{
			sInput.remove_prefix(1);
		}
	}
	else
	{
		sInput.TrimLeft();
	}

	return sWrapped;

} // HelpFormatter::WrapOutput

//---------------------------------------------------------------------------
void KOptions::HelpFormatter::GetEnvironment()
//---------------------------------------------------------------------------
{
	// get local versions of the config parameters, overridden by env var
	// env format: DEKAF2_HELP=SEPARATOR,WRAPPED_INDENT,LINEFEED_BETWEEN_OPTIONS,SPACING_PER_SECTION
	// e.g.: DEKAF2_HELP=--,2,true,false or DEKAF2_HELP=::,0
	auto sEnv = kGetEnv("DEKAF2_HELP");

	if (!sEnv.empty())
	{
		auto sPart = sEnv.Split();

		if (sPart.size() > 0)
		{
			m_sSeparator = sPart[0];

			if (sPart.size() > 1)
			{
				m_iWrappedHelpIndent = sPart[1].UInt16();

				if (sPart.size() > 2)
				{
					m_bLinefeedBetweenOptions = sPart[2].Bool();

					if (sPart.size() > 3)
					{
						m_bSpacingPerSection = sPart[3].Bool();
					}
				}
			}
		}
	}

} // HelpFormatter::GetEnvironment

//---------------------------------------------------------------------------
void KOptions::HelpFormatter::CalcExtends(const CallbackParam& Callback)
//---------------------------------------------------------------------------
{
	if (!Callback.IsHidden()  &&
		!Callback.IsSection() &&
		!Callback.IsUnknown())
	{
		if (Callback.IsCommand())
		{
			m_bHaveCommands = true;
		}
		else
		{
			m_bHaveOptions = true;
		}

		auto& iMaxLen = m_MaxLens[(m_bSpacingPerSection && Callback.IsCommand()) ? 1 : 0];

		for (const auto sFragment : Callback.m_sNames.Split('\n'))
		{
			auto iSize = (Callback.m_sNames.front() == '-') ? sFragment.size() - 1 : sFragment.size();
			iMaxLen = std::max(iMaxLen, iSize);
		}
	}

} // HelpFormatter::CalcExtends

//---------------------------------------------------------------------------
void KOptions::HelpFormatter::CalcExtends(const std::vector<CallbackParam>& Callbacks)
//---------------------------------------------------------------------------
{
	for (const auto& Callback : Callbacks)
	{
		CalcExtends(Callback);
	}

} // HelpFormatter::CalcExtends

//---------------------------------------------------------------------------
KOptions::HelpFormatter::Mask::Mask(const HelpFormatter& Formatter, bool bForCommands)
//---------------------------------------------------------------------------
{
	auto& iMaxLen = Formatter.m_MaxLens[(Formatter.m_bSpacingPerSection && bForCommands) ? 1 : 0];
	iMaxHelp      = Formatter.m_iColumns - (iMaxLen + Formatter.m_sSeparator.size() + 5);
	// format wrapped help texts so that they start earlier at the left if
	// argument size is bigger than help size
	bOverlapping  = iMaxHelp < iMaxLen;
	// kFormat crashes when we insert the spacing parm at the same time with
	// the actual parms, therefore we prepare the format strings before inserting
	// the parms
	sFormat   = kFormat("   {{}}{{:<{}}} {{}}{} {{}}", iMaxLen, Formatter.m_sSeparator);
	sOverflow = kFormat("   {{:<{}}} {{}}",
						bOverlapping
						? 1 + Formatter.m_iWrappedHelpIndent // we need the + 1 to avoid a total of 0 which would crash kFormat
						: 2 + iMaxLen + Formatter.m_sSeparator.size() + Formatter.m_iWrappedHelpIndent);

} // HelpFormatter::CalcFormatMasks

//---------------------------------------------------------------------------
void KOptions::HelpFormatter::FormatOne(KOutStream& out,
										const CallbackParam& Callback,
										const Mask& mask)
//---------------------------------------------------------------------------
{
	bool bFirst   = true;
	auto iHelp    = mask.iMaxHelp;
	auto sHelp    = Callback.m_sHelp;
	auto sNames   = Callback.m_sNames;

	if (sHelp.empty())
	{
		sHelp     = Callback.m_sMissingArgs;
	}

	if (Callback.IsSection())
	{
		out.Write("\n ");
		out.Write(Callback.m_sNames);

		if (!Callback.m_sHelp.empty())
		{
			out.Write(' ');
			out.Write(Callback.m_sHelp);
		}

		out.Write('\n');
	}
	else
	{
		while (bFirst || !sHelp.empty() || !sNames.empty())
		{
			auto sLimited      = WrapOutput(sHelp, iHelp, false);
			auto sLimitedNames = SplitAtLinefeed(sNames);

			if (bFirst)
			{
				bFirst = false;
				out.FormatLine(mask.sFormat,
							   (Callback.IsOption() && Callback.m_sNames.front() != '-') ? "-" : "",
							   sLimitedNames,
							   Callback.IsOption() ? "" : " ",
							   sLimited);

				if (mask.bOverlapping)
				{
					iHelp = m_iColumns - (1 + m_iWrappedHelpIndent);
				}
				else
				{
					iHelp -= m_iWrappedHelpIndent;
				}
			}
			else
			{
				out.FormatLine(mask.sOverflow, sLimitedNames, sLimited);
			}
		}
	}

} // HelpFormatter::FormatOne

//---------------------------------------------------------------------------
KOptions::HelpFormatter::HelpFormatter(KOutStream& out,
									   const KOptions::CallbackParam& Callback,
									   const HelpParams& HelpParams,
									   uint16_t iTerminalWidth)
//---------------------------------------------------------------------------
: m_Params(HelpParams)
, m_iColumns((iTerminalWidth) ? iTerminalWidth - 1 : m_Params.iMaxHelpRowWidth)
{
	GetEnvironment();
	CalcExtends(Callback);
	FormatOne(out, Callback, Mask(*this, Callback.IsCommand()));

} // HelpFormatter::BuildOne

//---------------------------------------------------------------------------
KOptions::HelpFormatter::HelpFormatter(KOutStream& out,
									   const std::vector<KOptions::CallbackParam>& Callbacks,
									   const HelpParams& HelpParams,
									   uint16_t iTerminalWidth)
//---------------------------------------------------------------------------
: m_Params(HelpParams)
, m_iColumns((iTerminalWidth) ? iTerminalWidth - 1 : m_Params.iMaxHelpRowWidth)
{
	GetEnvironment();
	CalcExtends(Callbacks);

	out.WriteLine();
	out.Format("{} -- ", m_Params.GetProgramName());

	{
		auto iIndent = m_Params.GetProgramName().size() + 4;

		if (m_iColumns < iIndent + 10)
		{
			iIndent = 0;
		}

		KStringView sDescription = m_Params.GetBriefDescription();

		auto sLimited = WrapOutput(sDescription, m_iColumns - iIndent, false);
		out.WriteLine(sLimited);

		iIndent += m_iWrappedHelpIndent;

		while (sDescription.size())
		{
			auto iSpaces = iIndent;
			while (iSpaces--)
			{
				out.Write(' ');
			}

			auto sLimited = WrapOutput(sDescription, m_iColumns - iIndent, false);
			out.WriteLine(sLimited);
		}
	}

	out.WriteLine();
	out.FormatLine("usage: {}{}{}{}{}",
				   m_Params.GetProgramName(),
				   m_bHaveOptions  ? " [<options>]" : "",
				   m_bHaveCommands ? " [<actions>]" : "",
				   m_Params.sAdditionalArgDesc.empty() ? "" : " ",
				   m_Params.sAdditionalArgDesc);
	out.WriteLine();

	auto Show = [&](bool bCommands)
	{
		if (bCommands)
		{
			out.WriteLine("with <actions>:");
		}
		else
		{
			out.WriteLine("with <options>:");
		}

		if (m_bLinefeedBetweenOptions)
		{
			out.WriteLine();
		}

		Mask mask(*this, bCommands);

		for (const auto& Callback : Callbacks)
		{
			if (!Callback.IsHidden()
				&& Callback.IsCommand() == bCommands
				&& !Callback.IsUnknown())
			{
				FormatOne(out, Callback, mask);

				if (m_bLinefeedBetweenOptions)
				{
					out.WriteLine();
				}
			}
		}

		out.WriteLine();
	};

	if (m_bHaveOptions)
	{
		Show(false);
	}

	if (m_bHaveCommands)
	{
		Show(true);
	}

	KStringView sAdditionalHelp = m_Params.GetAdditionalHelp();

	if (!sAdditionalHelp.empty())
	{
		while (sAdditionalHelp.size())
		{
			auto sLimited = WrapOutput(sAdditionalHelp, m_iColumns, true);
			out.WriteLine(sLimited);
		}
		out.WriteLine();
	}

} // HelpFormatter::BuildAll

//---------------------------------------------------------------------------
KOptions::KOptions(bool bEmptyParmsIsError, KStringView sCliDebugTo/*=KLog::STDOUT*/, bool bThrow/*=false*/)
//---------------------------------------------------------------------------
	: m_bThrow(bThrow)
	, m_bEmptyParmsIsError(bEmptyParmsIsError)
{
	SetBriefDescription("KOptions based option parsing");
	SetMaxHelpWidth(120);
	SetWrappedHelpIndent(0);

	Option("h,help")
		.Help("this help", -1 -1) // show this help before the other automatic options, but after the user defined options
		.Section("further <options>:")
	([this]()
	{
		AutomaticHelp();
		// and abort further parsing
		throw NoError {};
	});

#ifdef DEKAF2_WITH_KLOG
	auto SetKLogDebugging = [](KStringView sArg, KStringView sDebugTo, bool bUSeconds)
	{
		int iLevel = (sArg == (bUSeconds ? "ud0" : "d0")) ? 0 : static_cast<int>(sArg.size()) - bUSeconds;
		KLog::getInstance()
			.SetLevel    (iLevel)
			.KeepCLIMode (true)
			.SetUSecMode (bUSeconds)
			.SetDebugLog (sDebugTo);
		kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
	};

	Option("d0,d,dd,ddd")
		.Help("increasing optional stdout debug levels", -1)
	([this,sCliDebugTo, &SetKLogDebugging]()
	{
		SetKLogDebugging(GetCurrentArg(), sCliDebugTo, false);
	});

	Option("ud0,ud,udd,uddd")
		.Help("increasing optional stdout debug levels, with microseconds timestamps", -1)
	([this,sCliDebugTo,&SetKLogDebugging]()
	 {
		SetKLogDebugging(GetCurrentArg(), sCliDebugTo, true);
	});

	Option("dgrep,dgrepv <regex>", "grep expression")
		.Help("search (not) for grep expression in debug output", -1)
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
		KString sLowerGrep = sGrep.ToLower();
		kDebug (1, "debug {} set to: '{}'", bIsInverted	? "egrep -v" : "egrep", sLowerGrep);
		KLog::getInstance().LogWithGrepExpression(true, bIsInverted, sLowerGrep);
		KLog::getInstance().KeepCLIMode (true);
	});
#endif

	Option("ini <filename>", "ini file name")
		.Type(File)
		.Help("load options from ini file", -1)
	([this](KStringViewZ sIni)
	{
		if (ParseFile(sIni, KOut))
		{
			// error was already displayed - just abort parsing
			throw NoError {};
		}
	});

} // KOptions ctor

//---------------------------------------------------------------------------
void KOptions::AutomaticHelp() const
//---------------------------------------------------------------------------
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
		// create a copy
		auto Callbacks = m_Callbacks;
		// make sure help is (stable) sorted by help rank
		std::stable_sort(Callbacks.begin(), Callbacks.end(), [](const CallbackParam& left, const CallbackParam& right)
		{
			return (left.m_iHelpRank < right.m_iHelpRank);
		});

		HelpFormatter(out, Callbacks, m_HelpParams, GetCurrentOutputStreamWidth());
	}

} // AutomaticHelp

//---------------------------------------------------------------------------
void KOptions::Help(KOutStream& out)
//---------------------------------------------------------------------------
{
	KOutStreamRAII KO(&m_CurrentOutputStream, out);

	if (m_iRecursedHelp)
	{
		// we're calling ourself in a loop, because the user has setup an own
		// help callback which calls KOptions::Help()..
		// Stop here, and output the automatic help
		AutomaticHelp();
		return;
	}

	// do we have a help option registered?
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
			++m_iRecursedHelp;
			ArgList Args;
			Callback->m_Callback(Args);
			--m_iRecursedHelp;
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
const KOptions::CallbackParam* KOptions::FindParam(KStringView sName, bool bIsOption)
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
	if (OptionOrCommand.IsSection())
	{
		// a section separator does not have any callable
		m_Callbacks.push_back(std::move(OptionOrCommand));
		return;
	}

	auto& Lookup = OptionOrCommand.IsCommand() ? m_Commands : m_Options;
	auto  iIndex = m_Callbacks.size();

	for (auto sOption : OptionOrCommand.m_sNames.Split())
	{
		if (sOption != UNKNOWN_ARG)
		{
			sOption.TrimLeft(OptionOrCommand.IsCommand() ? " \f\v\t\r\n\b" : " -\f\v\t\r\n\b");

			if (sOption.front() == '-')
			{
				SetError(kFormat("a command, other than an option, may not start with - : {}", sOption));
				return;
			}

			// strip name at first special character or space
			auto pos = sOption.find_first_of(" <>[]|.;=\t\r\n\b");

			if (pos != KStringView::npos)
			{
				sOption.erase(pos);
			}
		}

		kDebug(3, "adding option: '{}'", sOption);

		auto Pair = Lookup.insert( { sOption, iIndex } );

		if (!Pair.second)
		{
			kDebug(1, "overriding existing {}: {}", OptionOrCommand.IsCommand() ? "command" : "option", sOption);
			Pair.first->second = iIndex;
			// disable the display of the existing option in the automatic help
			std::find_if(m_Callbacks.begin(), m_Callbacks.end(), [&](CallbackParam& cbp)
			{
				if (cbp.IsCommand() == OptionOrCommand.IsCommand()
					&& cbp.m_sNames == OptionOrCommand.m_sNames
					&& !cbp.IsHidden())
				{
					cbp.m_iFlags |= CallbackParam::fIsHidden;
					return true;
				}
				return false;
			});
		}
	}

	m_Callbacks.push_back(std::move(OptionOrCommand));

	if (m_Callbacks.size() != iIndex+1)
	{
		SetError("registration race");
	}

} // Register

//---------------------------------------------------------------------------
void KOptions::UnknownOption(CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(UNKNOWN_ARG,
						   "",
						   CallbackParam::fIsUnknown |
						   CallbackParam::fIsHidden,
						   0,
						   Function));
}

//---------------------------------------------------------------------------
void KOptions::UnknownCommand(CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(UNKNOWN_ARG,
						   "",
						   CallbackParam::fIsCommand |
						   CallbackParam::fIsUnknown |
						   CallbackParam::fIsHidden,
						   0,
						   Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOptions, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sOptions, sMissingParms, CallbackParam::fNone, iMinArgs, Function));
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommands, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sCommands, sMissingParms, CallbackParam::fIsCommand, iMinArgs, Function));
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

	std::vector<KStringViewZ> parms;
	kSplitArgsInPlace(parms, m_Strings.MutablePersist(std::move(sCLI)), /*svDelim  =*/" \f\v\t\r\n\b", /*svQuotes =*/"\"'", /*chEscape =*/'\\');

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

	// create a vector of string views pointing to the string buffers
	std::vector<KStringViewZ> QueryArgs;
	QueryArgs.push_back(m_Strings.Persist(sProgramName));

	for (const auto& it : Query.get())
	{
		QueryArgs.push_back(m_Strings.Persist(std::move(it.first)));

		if (!it.second.empty())
		{
			QueryArgs.push_back(m_Strings.Persist(std::move(it.second)));
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
KOutStream& KOptions::GetCurrentOutputStream() const
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
uint16_t KOptions::GetCurrentOutputStreamWidth() const
//---------------------------------------------------------------------------
{
	bool bIsStdOut = !m_CurrentOutputStream ||
	                  m_CurrentOutputStream == &KOut;

	auto iColumns  = m_HelpParams.iMaxHelpRowWidth;

	if (bIsStdOut)
	{
		auto TTY = kGetTerminalSize(0, iColumns);

		if (TTY.columns > 9)
		{
			iColumns = TTY.columns;
		}
	}

	kDebug(2, "terminal width: {}", iColumns);

	return iColumns;

} // GetCurrentOutputStreamWidth

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
		return m_Strings.Persist(sArg.ToLower());
	}
	else if (Callback->ToUpper())
	{
		return m_Strings.Persist(sArg.ToUpper());
	}
	else
	{
		return sArg;
	}

} // ModifyArgument

//---------------------------------------------------------------------------
KString KOptions::BuildParameterError(const CallbackParam& Callback, KString sMessage) const
//---------------------------------------------------------------------------
{
	if (!Callback.m_sHelp.empty())
	{
		sMessage += "\n\n";

		KString sHelp;
		KOutStringStream streamout(sMessage);

		HelpFormatter(streamout, Callback, m_HelpParams, GetCurrentOutputStreamWidth());
	}

	return sMessage;

} // BuildParameterError

//---------------------------------------------------------------------------
void KOptions::ResetBeforeParsing()
//---------------------------------------------------------------------------
{
	m_bStopAppAfterParsing = false;

	for (auto& Callback : m_Callbacks)
	{
		// reset flag
		Callback.m_bUsed = false;
	}

} // ResetBeforeParsing

//---------------------------------------------------------------------------
int KOptions::Execute(CLIParms Parms, KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_iExecutions++)
	{
		// we ran Execute before - reset variables
		ResetBeforeParsing();
	}

	KOutStreamRAII KO(&m_CurrentOutputStream, out);

	if (m_HelpParams.sProgramPathName.empty())
	{
		m_HelpParams.sProgramPathName = Parms.GetProgramPath();
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
					Callback = FindParam(UNKNOWN_ARG, it->IsOption());

					if (Callback)
					{
						// we pass the current Arg as the first arg of Args,
						// but we need to take care to not take it into account
						// when we readjust the remaining args after calling the
						// callback
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
								DEKAF2_THROW(WrongParameterError(BuildParameterError(*Callback, BadArgReason(Callback->m_ArgType, Args.front()))));
							}
							else if (Callback->CheckBounds() && !ValidBounds(Callback->m_ArgType, Args.front(), Callback->m_iLowerBound, Callback->m_iUpperBound))
							{
								DEKAF2_THROW(WrongParameterError(BuildParameterError(*Callback, BadBoundsReason(Callback->m_ArgType, Args.front(), Callback->m_iLowerBound, Callback->m_iUpperBound))));
							}
						}
					}

					if (Callback->m_iMinArgs > Args.size())
					{
						KString sMissingArgs;

						if (!Callback->m_sMissingArgs.empty())
						{
							sMissingArgs = Callback->m_sMissingArgs;
						}
						else
						{
							sMissingArgs = kFormat("{} arguments required, but only {} found",
												   Callback->m_iMinArgs,
												   Args.size());
						}

						DEKAF2_THROW(MissingParameterError(BuildParameterError(*Callback, sMissingArgs)));
					}

					// keep record of the initial args count
					auto iOldSize = Args.size();

					if (Callback->m_Callback)
					{
						// finally call the callback (if existing)
						Callback->m_Callback(Args);
					}
					else
					{
						// this is an edge case - there was no callback, so treat this
						// option as pure syntax check, consuming its minimum params
						auto iRemoveArgs = Callback->m_iMinArgs;

						while (!Args.empty() && iRemoveArgs--)
						{
							// this will trigger the arg to be marked as consumed below
							Args.pop();
						}
					}

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

					if (Callback->IsFinal())
					{
						// we're done
						// set flag to stop program after all args are parsed
						m_bStopAppAfterParsing = true;
						return Evaluate(Parms, out);
					}

					if (Callback->IsStop())
					{
						// set flag to stop program after all args are parsed
						m_bStopAppAfterParsing = true;
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
	}

	return bError; // 0 or 1

} // Evaluate

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
	constexpr KStringViewZ KOptions::CLIParms::Arg_t::s_sDoubleDash;
	constexpr KStringView  KOptions::UNKNOWN_ARG;
#endif

} // end of namespace dekaf2
