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
#include "dekaf2.h"
#include "koutstringstream.h"
#include <utility>

DEKAF2_NAMESPACE_BEGIN

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
KOptions::CallbackParam::CallbackParam(KStringView sNames, KStringView sMissingArgs, uint16_t fFlags, uint16_t iMinArgs, uint16_t iMaxArgs, CallbackN Func)
//---------------------------------------------------------------------------
: m_Callback     ( std::move(Func) )
, m_sNames       ( sNames          )
, m_sMissingArgs ( sMissingArgs    )
, m_iMinArgs     ( iMinArgs        )
, m_iMaxArgs     ( iMaxArgs        )
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
	// generate a subsection
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
void KOptions::CLIParms::Create(KStringViewZ sArg, PersistedStrings& Strings)
//---------------------------------------------------------------------------
{
	if (sArg.front() == '-')
	{
		// split an arg with "key=value", for any options, being them single or double dashed

		KStringViewZ::size_type iPos { 0 };

		for (auto ch : sArg)
		{
			if (KASCII::kIsBlank(ch))
			{
				break;
			}

			if (ch == '=')
			{
				if (iPos > 0)
				{
					KString sNewArg = sArg.ToView(0, iPos);
					m_ArgVec.push_back(Strings.Persist(sNewArg).ToView());
					sArg.remove_prefix(iPos + 1);
				}
				break;
			}

			++iPos;
		}
	}

	m_ArgVec.push_back(sArg);

} // Create

//---------------------------------------------------------------------------
void KOptions::CLIParms::Create(const std::vector<KStringViewZ>& parms, PersistedStrings& Strings)
//---------------------------------------------------------------------------
{
	m_ArgVec.clear();
	m_ArgVec.reserve(parms.size());

	for (auto it : parms)
	{
		Create(it, Strings);
	}

	if (!m_ArgVec.empty())
	{
		m_sProgramPathName = m_ArgVec.front().sArg;
		m_ArgVec.front().bConsumed = true;
	}

} // CLIParms::Create

//---------------------------------------------------------------------------
void KOptions::CLIParms::Create(int argc, char const* const* argv, PersistedStrings& Strings)
//---------------------------------------------------------------------------
{
	std::vector<KStringViewZ> parms;
	parms.reserve(argc);

	while (argc-- > 0)
	{
		parms.emplace_back(*argv++);
	}

	Create(parms, Strings);

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
KOptions::CLIParms::iterator KOptions::CLIParms::ExpandToSingleCharArgs(iterator it, const std::vector<KStringViewZ>& SplittedArgs)
//---------------------------------------------------------------------------
{
	auto iPos = it - begin();

	it = m_ArgVec.erase(it);

	for (auto sArg : SplittedArgs)
	{
		it = m_ArgVec.insert(it, Arg_t(sArg, 1));
		++it;
	}

	return begin() + iPos;

} // ExpandToSingleCharArgs

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
				// wrapping (as we will have two lines anyway)
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

		if (!sPart.empty())
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
KOptions::HelpFormatter::Mask::Mask(const HelpFormatter& Formatter, bool bForCommands, bool bIsRequired)
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

	if (bIsRequired)
	{
		sFormat = kFormat("   \x1b[1m{{}}{{:<{}}}\x1b[22m {{}}{} {{}}", iMaxLen, Formatter.m_sSeparator);
	}
	else
	{
		sFormat = kFormat("   {{}}{{:<{}}} {{}}{} {{}}", iMaxLen, Formatter.m_sSeparator);
	}
	sOverflow   = kFormat("   {{:<{}}} {{}}",
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
	// we only highlight required options on stdout
	bool bIsStdOut = &out == &KOut;
	FormatOne(out, Callback, Mask(*this, Callback.IsCommand(), bIsStdOut && Callback.IsRequired()));

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
	// we only highlight required options on stdout
	bool bIsStdOut = &out == &KOut;

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

		while (!sDescription.empty())
		{
			auto iSpaces = iIndent;
			while (iSpaces--)
			{
				out.Write(' ');
			}

			sLimited = WrapOutput(sDescription, m_iColumns - iIndent, false);
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

		for (const auto& Callback : Callbacks)
		{
			if (!Callback.IsHidden()
				&& Callback.IsCommand() == bCommands
				&& !Callback.IsUnknown())
			{
				Mask mask(*this, bCommands, bIsStdOut && Callback.IsRequired());

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
		while (!sAdditionalHelp.empty())
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
	SetDefaults(sCliDebugTo);
}

//---------------------------------------------------------------------------
KOptions::KOptions (bool bEmptyParmsIsError, int argc, char const* const* argv, KStringView sCliDebugTo/*= KLog::STDOUT*/, bool bThrow/*=false*/)
//---------------------------------------------------------------------------
: m_bThrow(bThrow)
, m_bEmptyParmsIsError(bEmptyParmsIsError)
, m_bAllowAdHocArgs(true)
{
	SetDefaults(sCliDebugTo);
	Parse(argc, argv);
}

//---------------------------------------------------------------------------
KOptions::~KOptions()
//---------------------------------------------------------------------------
{
	Throw(false);
	Check();
}

//---------------------------------------------------------------------------
void KOptions::SetDefaults(KStringView sCliDebugTo/*=KLog::STDOUT*/)
//---------------------------------------------------------------------------
{
//	SetBriefDescription("KOptions based option parsing");
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
			.SetUSecMode (bUSeconds)
			.KeepCLIMode (true)
			.SetLevel    (iLevel)
			.SetDebugLog (sDebugTo);
		kDebug (1, "debug level set to: {}", KLog::getInstance().GetLevel());
	};

	Option("d0,d,dd,ddd")
		.Help("increasing optional stdout debug levels", -1)
	([this, sCliDebugTo, &SetKLogDebugging]()
	{
		SetKLogDebugging(GetCurrentArg(), sCliDebugTo, false);
	});

	Option("ud0,ud,udd,uddd")
		.Help("increasing optional stdout debug levels, with microseconds timestamps", -1)
	([this, sCliDebugTo, &SetKLogDebugging]()
	 {
		SetKLogDebugging(GetCurrentArg(), sCliDebugTo, true);
	});

	Option("dgrep,dgrepv <regex>", "grep expression")
		.Help("search (not) for grep expression in debug output", -1)
	([this, sCliDebugTo](KStringViewZ sGrep)
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

	Option("config,ini <filename>", "config file name")
		.Type(File)
		.Help("load options from config file", -1)
	([this](KStringViewZ sIni)
	{
		if (ParseFile(sIni, KOut))
		{
			// error was already displayed - just abort parsing
			throw NoError {};
		}
	});

	// we need the count of the automatic options to allow ad hoc parms automatically
	// if no other options were set
	m_iAutomaticOptionCount = m_Options.size();

} // SetDefaults

//---------------------------------------------------------------------------
void KOptions::AutomaticHelp() const
//---------------------------------------------------------------------------
{
	if (m_bAllowAdHocArgs && 
		!m_bHaveAdHocArgs &&
		m_iAutomaticOptionCount == m_Options.size() &&
		!m_bProcessAdHocForHelp)
	{
		m_bProcessAdHocForHelp = true;
		return;
	}
	
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
		// we're calling ourselves in a loop, because the user has set up an own
		// help callback which calls KOptions::Help()..
		// Stop here, and output the automatic help
		AutomaticHelp();
		return;
	}

	// do we have a help option registered?
	auto Callback = FindParam("help", true);

	if (!Callback)
	{
		SetError("no help registered", out);
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
const KOptions::CallbackParam* KOptions::FindParam(KStringView sName, bool bIsOption) const
//---------------------------------------------------------------------------
{
	auto& Lookup = bIsOption ? m_Options : m_Commands;

	auto it = Lookup.find(sName);

	if (it == Lookup.end() || it->second >= m_Callbacks.size())
	{
		return nullptr;
	}

	return &m_Callbacks[it->second];

} // FindParam

//---------------------------------------------------------------------------
const KOptions::CallbackParam* KOptions::FindParam(KStringView sName, bool bIsOption, bool bMarkAsUsed) const
//---------------------------------------------------------------------------
{
	auto Callback = FindParam(sName, bIsOption);

	if (Callback && bMarkAsUsed)
	{
		// mark option as used
		Callback->m_bUsed = true;
	}

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
			sOption = IsolateOptionNamesFromSuffix(sOption);
		}

		kDebug(3, "adding option: '{}'", sOption);

		auto Pair = Lookup.insert( { sOption, iIndex } );

		if (!Pair.second)
		{
			if (Pair.first->second < m_Callbacks.size())
			{
				auto& Callback = m_Callbacks[Pair.first->second];

				if (Callback.IsCommand() == OptionOrCommand.IsCommand())
				{
					if (OptionOrCommand.IsAdHoc())
					{
						if (!Callback.IsAdHoc())
						{
							kDebug(1, "merging with non-ad-hoc option, which should not happen");
						}
						else
						{
							kDebug(2, "merging help with existing ad-hoc option");
						}

						if (OptionOrCommand.m_Args.empty())
						{
							// move the already collected args
							OptionOrCommand.m_Args = std::move(Callback.m_Args);
						}
						else
						{
							kDebug(2, "merging args, which should not happen");
							for (auto sArg : Callback.m_Args)
							{
								OptionOrCommand.m_Args.push_back(sArg);
							}
						}
					}

					kDebug(1, "overriding existing {}: {}", OptionOrCommand.IsCommand() ? "command" : "option", sOption);
					Pair.first->second = iIndex;

					if (!Callback.IsHidden())
					{
						Callback.m_iFlags |= CallbackParam::fIsHidden;
					}
				}
			}
			else
			{
				kDebug(1, "bad index in callbacks");
			}
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
						   std::move(Function)));
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
						   std::move(Function)));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOptions, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sOptions, sMissingParms, CallbackParam::fNone, iMinArgs, std::move(Function)));
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommands, uint16_t iMinArgs, KStringViewZ sMissingParms, CallbackN Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sCommands, sMissingParms, CallbackParam::fIsCommand, iMinArgs, std::move(Function)));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, Callback0 Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sOption, "", CallbackParam::fNone, 0, 0, [func = std::move(Function)](ArgList& unused)
	{
		func();
	}));
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, Callback0 Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sCommand, "", CallbackParam::fIsCommand, 0, 0, [func = std::move(Function)](ArgList& unused)
	{
		func();
	}));
}

//---------------------------------------------------------------------------
void KOptions::RegisterOption(KStringView sOption, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sOption, sMissingParm, CallbackParam::fNone, 1, 1, [func = std::move(Function)](ArgList& args)
	{
		func(args.pop());
	}));
}

//---------------------------------------------------------------------------
void KOptions::RegisterCommand(KStringView sCommand, KStringViewZ sMissingParm, Callback1 Function)
//---------------------------------------------------------------------------
{
	Register(CallbackParam(sCommand, sMissingParm, CallbackParam::fIsCommand, 1, 1, [func = std::move(Function)](ArgList& args)
	{
		func(args.pop());
	}));
}

//---------------------------------------------------------------------------
int KOptions::Parse(int argc, char const* const* argv, KOutStream& out)
//---------------------------------------------------------------------------
{
	return Execute(CLIParms(argc, argv, m_Strings), out);

} // Parse

//---------------------------------------------------------------------------
int KOptions::Parse(KString sCLI, KOutStream& out)
//---------------------------------------------------------------------------
{
	kDebug (1, sCLI);

	// create a permanent buffer of the passed CLI

	std::vector<KStringViewZ> parms;
	kSplitArgsInPlace(parms, m_Strings.MutablePersist(std::move(sCLI)));

	return Execute(CLIParms(parms, m_Strings), out);

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
		return SetError(kFormat("cannot open input file: {}", sFileName), out);
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

	return Execute(CLIParms(QueryArgs, m_Strings), out);

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
	m_bHaveErrors = true;

	if (m_bThrow)
	{
		// stop the run of Check() at destruction (we throw already
		// for another error and do not want the overall analysis ..)
		m_bCheckWasCalled = true;
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
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case Socket:
#endif
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
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case Socket:
#endif
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
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case Socket:
			return kFormat("not an existing unix socket: {}", sParm);
#endif
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
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case Socket:
		{
			KFileStat Stat(sParm);
			return Stat.IsSocket();
		}
#endif
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
	m_bHaveAdHocArgs       = false;
	m_bCheckWasCalled      = false;
	m_bHaveErrors          = false;
	m_bProcessAdHocForHelp = false;

	for (auto& Callback : m_Callbacks)
	{
		// reset flag
		Callback.m_bUsed = false;
		// TODO we should delete the ad hoc options here
	}

} // ResetBeforeParsing

//---------------------------------------------------------------------------
std::vector<KStringViewZ> KOptions::CheckForCombinedArg(const CLIParms::Arg_t& arg)
//---------------------------------------------------------------------------
{
	bool bValid = true;
	auto iLast = arg.sArg.size();

	// must have at least two chars here, prefixed by ONE dash
	if (arg.DashCount() == 0 || iLast < 2)
	{
		bValid = false;
	}
	else
	{
		// check for each character if a single char arg exists
		for (std::size_t i = 0; i < iLast; ++i)
		{
			auto Callback = FindParam(arg.sArg.ToView(i, 1), true, false);

			if (Callback == nullptr)
			{
				bValid = false;
				break;
			}
			else
			{
				if (Callback->m_iMaxArgs > 0 && i != iLast-1)
				{
					bValid = false;
					break;
				}
			}
		}
	}

	std::vector<KStringViewZ> SplittedArgs;

	if (bValid)
	{
		for (auto ch : arg.sArg)
		{
			SplittedArgs.push_back(m_Strings.Persist(KString(1, ch)));
		}
	}

	return SplittedArgs;

} // CheckForCombinedArg

//---------------------------------------------------------------------------
int KOptions::Execute(CLIParms Parms, KOutStream& out)
//---------------------------------------------------------------------------
{
	if (m_iExecutions++)
	{
		// we ran Execute before - reset variables
		ResetBeforeParsing();
	}

	if (!m_bAllowAdHocArgs && m_iAutomaticOptionCount == m_Options.size() && m_Commands.empty())
	{
		kDebug(2, "switching ad-hoc options on as there were no other options declared");
		m_bAllowAdHocArgs = true;
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

				auto Callback = FindParam(it->sArg, it->IsOption(), true);

				if (DEKAF2_UNLIKELY(Callback == nullptr))
				{
					// check if this is might be a combined single char arg,
					// like -ets test for -e -t -s test
					auto SplittedArgs = CheckForCombinedArg(*it);
					
					if (!SplittedArgs.empty())
					{
						// yes, expand this arg into multiple args (in place of the
						// previous combined arg)
						it = Parms.ExpandToSingleCharArgs(it, SplittedArgs);
						// readjust the loop position
						--it;
						// and restart looping at the current position
						continue;
					}

					// check if we have a handler for an unknown arg
					Callback = FindParam(UNKNOWN_ARG, it->IsOption(), true);

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

				// if this is an option, and it was not found, and we allow ad hoc arguments,
				// then add this as a new option
				if (!Callback && it->IsOption() && m_bAllowAdHocArgs)
				{
					// do a sanity check on the option arg
					if (AdHocOptionSanityCheck(it->sArg))
					{
						// count parms until next option, to have a min/max setting for the new option
						std::size_t iArgs { 0 };

						for (auto it2 = it + 1; it2 != Parms.end() && !it2->IsOption(); ++it2)
						{
							++iArgs;
						}

						// and add the new ad hoc option
						Option(it->sArg).MinArgs(iArgs).MaxArgs(iArgs).AdHoc();
						// now search again for the option
						Callback = FindParam(it->sArg, true);
						// keep a note that we have to check the consumption
						// of ad hoc args with Check()
						m_bHaveAdHocArgs = true;
					}
				}

				if (Callback)
				{
					// this looks redundant, but it is needed when parsing options
					// from an ini file, and for ad-hoc options
					it->bConsumed = true;

					auto iMaxArgs = Callback->m_iMaxArgs;

					// isolate parms until next option and add them to the ArgList
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
						// get a backup of the args stack
						std::vector<KStringViewZ> ArgsCopy(Args.begin(), Args.end());
						// finally call the callback
						Callback->m_Callback(Args);
						// get count of consumed args
						auto iCopy = ArgsCopy.size() - Args.size();
						// copy any arg that was consumed by the callback
						// and store it for possible .Get() access
						for (;iCopy--;)
						{
							Callback->m_Args.push_back(ArgsCopy[iCopy]);
						}
					}
					else
					{
						// this is an edge case - there was no callback
						// copy all args from the stack/deque, and store them in reverse order for .Get()
						for (auto it = Args.crbegin(), ie = Args.crend(); it != ie; ++it)
						{
							Callback->m_Args.push_back(*it);
						}
						// and comsume them as if they had been used by the callback
						Args.clear();
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
					// TODO check if we shouldn't consume for ad hoc
					while (iOldSize-- > Args.size())
					{
						(++it)->bConsumed = true;
					}

					if (Callback->IsFinal())
					{
						// we're done
						// set flag to finalize program after this arg
						m_bStopAppAfterParsing = true;
						return !Evaluate(Parms, out);
					}

					if (Callback->IsStop())
					{
						// set flag to stop program after all args are parsed
						m_bStopAppAfterParsing = true;
					}
				}
			}
		}

		return !Evaluate(Parms, out);
	}

	DEKAF2_CATCH (const MissingParameterError& error)
	{
		return SetError(kFormat("{}: missing parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()), out);
	}

	DEKAF2_CATCH (const WrongParameterError& error)
	{
		return SetError(kFormat("{}: wrong parameter after {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()), out);
	}

	DEKAF2_CATCH (const BadOptionError& error)
	{
		return SetError(kFormat("{}: {}{}: {}", GetProgramName(), lastCommand->Dashes(), lastCommand->sArg, error.what()), out);
	}

	DEKAF2_CATCH (const Error& error)
	{
		return SetError(error.what(), out);
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
bool KOptions::Evaluate(const CLIParms& Parms, KOutStream& out)
//---------------------------------------------------------------------------
{
	if (Parms.size() < 2)
	{
		if (m_bEmptyParmsIsError)
		{
			Help(out);
			return false;
		}
	}

	bool bOK { true };

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

		bOK = false;
	}

	// now check for required options
	for (auto& Callback : m_Callbacks)
	{
		if (Callback.IsRequired() && !Callback.m_bUsed)
		{
			bOK = false;
			out.FormatLine("missing required argument: {}{}", Callback.IsCommand() ? "" : "-", Callback.m_sNames);
		}
	}

	if (!bOK) m_bHaveErrors = true;

	return bOK;

} // Evaluate

//---------------------------------------------------------------------------
bool KOptions::Check(KOutStream& out)
//---------------------------------------------------------------------------
{
	if (!m_bCheckWasCalled)
	{
		m_bCheckWasCalled = true;

		if (m_bProcessAdHocForHelp)
		{
			KOutStreamRAII KO(&m_CurrentOutputStream, out);
			AutomaticHelp();
			return false;
		}

		bool bErrors = false;

		if (m_bHaveAdHocArgs)
		{
			// now check for ad-hoc options
			for (auto& Callback : m_Callbacks)
			{
				if (Callback.IsAdHoc() && !Callback.m_bUsed && !Callback.IsHidden())
				{
					bErrors = true;
					out.FormatLine("excess argument: -{}", Callback.m_sNames);
				}
			}
		}

		if (bErrors)
		{
			// SetError() will also set the general m_bHaveErrors to true
			SetError("bad CLI parms", out);
		}
	}

	return !m_bHaveErrors;

} // Check

//---------------------------------------------------------------------------
bool KOptions::AdHocOptionSanityCheck(KStringView sOption)
//---------------------------------------------------------------------------
{
	if (sOption.size() > iMaxAdHocOptionLength)
	{
		kDebug(2, "ad hoc option would have length {}, exceeding the maximum of {}", sOption.size(), iMaxAdHocOptionLength);
		return false;
	}

	return true;

} // AdHocOptionSanityCheck

//---------------------------------------------------------------------------
std::pair<KStringView, KStringView> KOptions::IsolateOptionNamesFromHelp(KStringView sOptionName)
//---------------------------------------------------------------------------
{
	KStringView sHelp;

	auto iPos = sOptionName.find(':');

	if (iPos != npos)
	{
		sHelp = sOptionName.ToView(iPos + 1);
		sHelp.Trim();
		sOptionName.remove_suffix(sOptionName.size() - iPos);
	}

	sOptionName.Trim();

	return { sOptionName, sHelp };

} // IsolateOptionNamesFromHelp

//---------------------------------------------------------------------------
KStringView KOptions::IsolateOptionNamesFromSuffix(KStringView sOptionName)
//---------------------------------------------------------------------------
{

	auto iPos = sOptionName.find_first_of(" <>[]|:;=\t\r\n\b");

	if (iPos != npos)
	{
		sOptionName.remove_suffix(sOptionName.size() - iPos);
	}

	return sOptionName;

} // IsolateOptionNamesFromSuffix

//---------------------------------------------------------------------------
KOptions::Values KOptions::Get(KStringView sOptionName)
//---------------------------------------------------------------------------
{
	const CallbackParam* Callback { nullptr };

	sOptionName.TrimLeft(' ');
	sOptionName.TrimLeft('-');

	for (auto sName : IsolateOptionNamesFromSuffix(IsolateOptionNamesFromHelp(sOptionName).first).Split())
	{
		Callback = FindParam(sName, true, true);

		if (Callback)
		{
			break;
		}
	}

	if (!Callback)
	{
		if (!m_bProcessAdHocForHelp)
		{
			SetError(kFormat("missing required option: -{}", sOptionName));
		}

		// if we process for help generation, do not throw but add
		// each option as if it had an empty default value
		return Get(sOptionName, std::vector<KStringViewZ>{});
	}

	return Values(Callback->m_Args, true);

} // Get

//---------------------------------------------------------------------------
KOptions::Values KOptions::Get(KStringView sOptionName, const std::vector<KStringViewZ>& DefaultValues) noexcept
//---------------------------------------------------------------------------
{
	const CallbackParam* Callback { nullptr };

	sOptionName.TrimLeft(" \t-");

	auto pair = IsolateOptionNamesFromHelp(sOptionName);
	auto list = IsolateOptionNamesFromSuffix(pair.first).Split();

	for (auto sName : list)
	{
		Callback = FindParam(sName, true, true);

		if (Callback)
		{
			break;
		}
	}

	if (!Callback)
	{
		Option       (pair.first)
			.Help    (pair.second)
			.MinArgs (DefaultValues.size())
			.MaxArgs (DefaultValues.size())
			.AdHoc   ()
			.AddFlag (DefaultValues.empty() ? CallbackParam::fIsRequired : CallbackParam::fNone);

		Callback = FindParam(list.front(), true, true);

		if (!Callback)
		{
			kDebug(1, "cannot create option: {}", sOptionName);
			static std::vector<KStringViewZ> s_Empty;
			return Values(s_Empty, false);
		}

		// check if the args were already parsed under another name in the callback,
		// in which case we do not add the default args here, but treat this as an
		// existing parm set
		if (Callback->m_Args.empty())
		{
			for (auto sDefaultValue : DefaultValues)
			{
				Callback->m_Args.push_back(m_Strings.Persist(sDefaultValue));
			}
		}
	}

	return Values(Callback->m_Args, true);

} // Get

//---------------------------------------------------------------------------
KOptions::Values KOptions::Get(KStringView sOptionName, const KStringViewZ& sDefaultValue) noexcept
//---------------------------------------------------------------------------
{
	return Get(sOptionName, std::vector<KStringViewZ> { sDefaultValue });

} // Get

//---------------------------------------------------------------------------
bool KOptions::Values::Bool() const noexcept
//---------------------------------------------------------------------------
{
	const auto& sValue = String();

	if (sValue.empty())
	{
		return m_bFound;
	}

	return sValue.Bool();

} // Bool

//---------------------------------------------------------------------------
KStringViewZ KOptions::Values::operator [] (std::size_t index) const
//---------------------------------------------------------------------------
{
	if (index < size())
	{
		return m_Params[index];
	}
	else
	{
		throw KException(kFormat("cannot access element {} in an argument list of {}", index + 1, size()));
	}
}

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
	constexpr KStringViewZ KOptions::CLIParms::Arg_t::s_sDoubleDash;
	constexpr KStringView  KOptions::UNKNOWN_ARG;
	constexpr KStringView::size_type iMaxAdHocOptionLength;
#endif

DEKAF2_NAMESPACE_END
