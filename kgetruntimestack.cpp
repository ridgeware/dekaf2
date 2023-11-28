/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "kgetruntimestack.h"
#include "kcompatibility.h"
#include "kstring.h"
#include "kstack.h"
#include "ksplit.h"
#include "kinshell.h"
#include "ksystem.h"
#include "kformat.h"

#include <vector>

#ifdef DEKAF2_IS_UNIX
	#ifndef DEKAF2_HAS_MUSL
		#include <execinfo.h>          // for backtrace
	#endif
#include <cxxabi.h>            // for demangling
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

namespace dekaf2
{

using StringVec = std::vector<KString>;
using FrameVec  = std::vector<KStackFrame>;

namespace detail {
namespace bt {

//-----------------------------------------------------------------------------
// This function will try and invoke the external addr2line code to map a vector
// of hex process addresses to a vector of source file/line numbers. On Mac OS X
// it will use atos instead of addr2line. It will return an empty vector upon
// failure.
StringVec Addr2Line (const std::vector<KStringView>& vsAddress)
//-----------------------------------------------------------------------------
{
	StringVec vsResult;

	DEKAF2_TRY
	{
		if (!vsAddress.empty())
		{

#ifdef DEKAF2_IS_OSX

			KString sCmdLine = dekaf2::kFormat("atos -p {}", getpid());

			for (const auto& it : vsAddress)
			{
				sCmdLine += ' ';
				sCmdLine += it;
			}

			KInShell Shell;
			Shell.SetReaderRightTrim("\n\r\t ");

			if (Shell.Open (sCmdLine))
			{
				KString sLineBuf;

				while (Shell.ReadLine (sLineBuf))
				{
					// remove part of string "(in PROGRAM)"
					auto pos = sLineBuf.find(" (in ");
					if (pos != KString::npos)
					{
						auto iend = sLineBuf.find(')', pos + 4);
						if (iend != KString::npos)
						{
							sLineBuf.erase(pos, (iend - pos) + 1);
						}
					}
					// and report
					vsResult.push_back(sLineBuf);
				}

			}

#elif DEKAF2_IS_UNIX

			KString sMyExeName = kGetOwnPathname();

			if (!sMyExeName.empty())
			{
				KString sCmdLine = kFormat("addr2line -f -C -e \"{}\"", sMyExeName);

				for (const auto& it : vsAddress)
				{
					sCmdLine += ' ';
					sCmdLine += it;
				}

				KInShell Shell;
				Shell.SetReaderRightTrim("\n\r\t ");

				if (Shell.Open (sCmdLine))
				{
					KString sLineBuf;
					KString sResult;

					while (Shell.ReadLine (sLineBuf))
					{
						sResult = sLineBuf;

						if (Shell.ReadLine (sLineBuf))
						{
							if (!sLineBuf.starts_with("??:")) {
								sResult += " at ";
								sResult += sLineBuf;
							}
						}
						vsResult.push_back(sResult);
					}

				}
			}
#endif
			if (vsResult.empty())
			{
				// addr2line unsuccessful, simply copy addresses back
				for (const auto& it : vsAddress)
				{
					vsResult.push_back(it);
				}
			}
		}
	}

	DEKAF2_CATCH (...)
	{
		// ignore - just do default - returning empty string
	}

	return vsResult;
}

//-----------------------------------------------------------------------------
KString PrintStackVector(const StringVec& vsFrames)
//-----------------------------------------------------------------------------
{
	KString sStack;

	// avoid unnecessary reallocations
	size_t iFrameMax = 0;
	for (const auto& it : vsFrames)
	{
		iFrameMax += it.size() + 5;
	}

	sStack.reserve(iFrameMax);

	size_t ii = vsFrames.size();
	for (auto& it : vsFrames)
	{
		KString sFrame;
		sFrame = std::to_string(ii--);
		sFrame.PadLeft(3);
		sFrame += ' ';
		sFrame += it;
		sFrame += '\n';
		sStack.append(sFrame);
	}

	return sStack;

} // PrintStackVector

//-----------------------------------------------------------------------------
KString PrintFrameVector(const FrameVec& Frames, bool bNormalize)
//-----------------------------------------------------------------------------
{
	KString sStack;

	sStack.reserve(Frames.size() * 80);

	size_t ii = Frames.size();
	for (const auto& it : Frames)
	{
		KString sFrame;
		sFrame = std::to_string(ii--);
		sFrame.PadLeft(3);
		sFrame += ' ';
		sFrame += it.Serialize(bNormalize);
		sFrame += '\n';

		sStack.append(sFrame);
	}

	return sStack;

} // PrintFrameVector

//-----------------------------------------------------------------------------
KStringView GetStackAddress (KStringView sBacktraceLine)
//-----------------------------------------------------------------------------
{
	size_t i = sBacktraceLine.find ("[0x");
	char cCloser = ']';

	if (i == KString::npos)
	{
		i = sBacktraceLine.find (" 0x");
		cCloser = ' ';
	}

	if (i == KString::npos)
	{
		return {};
	}

	size_t e = sBacktraceLine.find(cCloser, i+3);

	if (e == KString::npos)
	{
		return KStringView();
	}

	return sBacktraceLine.substr(i + 1, e - i - 1);

} // GetStackAddress

#ifdef WIN32
	#define SUPPORT_GDBATTACH_PRINTCALLSTACK 0
	#define SUPPORT_BACKTRACE_PRINTCALLSTACK 0
#else
	#ifndef DEKAF2_IS_OSX
		#ifndef SUPPORT_GDBATTACH_PRINTCALLSTACK
			#define SUPPORT_GDBATTACH_PRINTCALLSTACK 1
		#endif
	#endif
	#ifndef SUPPORT_BACKTRACE_PRINTCALLSTACK
		#ifndef DEKAF2_HAS_MUSL
			#define SUPPORT_BACKTRACE_PRINTCALLSTACK 1
		#endif
	#endif
#endif

#if SUPPORT_GDBATTACH_PRINTCALLSTACK

//-----------------------------------------------------------------------------
StringVec GetGDBCallstack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	StringVec Stack;

	DEKAF2_TRY
	{
		KString sMyExeName = kGetOwnPathname();

		if (sMyExeName.empty())
		{
			return Stack;
		}

		KString sCmdLine = kFormat("gdb --batch -n -ex thread -ex bt \"{}\" {} 2>&1", sMyExeName, getpid());

		KInShell Shell;
		Shell.SetReaderRightTrim("\n\r\t ");

		if (Shell.Open (sCmdLine))
		{
			KString	sLine;
			bool bSeenOwnStackFrame { false };

			while (Shell.ReadLine(sLine))
			{
				if (sLine.empty())
				{
					continue;			// skip blank lines
				}

				// Joe says to just skip these
				if (sLine.starts_with ("warning: (Internal error:"))
				{
					continue;			// skip bogus lines
				}

				if (!bSeenOwnStackFrame)
				{
					if (sLine.find(__FUNCTION__) != KString::npos)
					{
						bSeenOwnStackFrame = true;
					}
					continue;			// skip prefixing lines (from our call to gdb)
				}

				if (iSkipStackLines > 0)
				{
					--iSkipStackLines;
					continue;
				}

				if (sLine.starts_with('#'))
				{
					sLine.erase(0, 4);
				}

				if (sLine.starts_with("0x"))
				{
					size_t pos = sLine.find(' ');

					if (pos != KString::npos)
					{
						sLine.erase(0, pos+1);

						if (sLine.starts_with("in "))
						{
							sLine.erase(0, 3);
						}
					}
				}

				if (sLine.ends_with(") ()"))
				{
					sLine.remove_suffix(3);
				}

				Stack.push_back(sLine);
			}
		}
	}

	DEKAF2_CATCH (...)
	{
		Stack.clear();
	}

	return Stack;
}

#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

/*
Linux clang release:

 attempting to print a backtrace:
  11 detail::GetBacktraceCallstack() (:)
  10 kGetBacktrace() (:)
   9 kGetRuntimeStack() (:)
   8 kCrashExitExt() (:)
   7 ??() (:)
   6 TestBacktraces() (:)
...
= -5

Linux clang debug:

  23 asan_interceptors.cpp.o:?() (asan_interceptors.cpp.o:)
  22 detail::GetBacktraceCallstack() (kgetruntimestack.cpp:)
  21 detail::GetBacktraceVector() (kgetruntimestack.cpp:)
  20 kGetBacktrace() (kgetruntimestack.cpp:)
  19 kGetRuntimeStack() (kgetruntimestack.cpp:)
  18 /home/vagrant/src/dekaf2/kcrashexit.cpp:185() (kcrashexit.cpp:)
  17 ??() (:)
  16 TestBacktraces() (klog_main.cpp:)
...
= -7

Linux gcc release:

 attempting to print a backtrace:
  13 detail::GetBacktraceCallstack() (:)
  12 detail::GetBacktraceVector() (:)
  11 kGetBacktrace() (:)
  10 kGetRuntimeStack() (:)
   9 kCrashExitExt() (:)
   8 ??() (:)
   7 TestBacktraces() (:)
...
= -6

Linux gcc debug:

 attempting to print a backtrace:
  23 ??() (:)
  22 detail::GetBacktraceCallstack() (kgetruntimestack.cpp:)
  21 detail::GetBacktraceVector() (kgetruntimestack.cpp:)
  20 kGetBacktrace() (kgetruntimestack.cpp:)
  19 kGetRuntimeStack() (kgetruntimestack.cpp:)
  18 (kcrashexit.cpp:)
  17 ??() (:)
  16 TestBacktraces() (klog_main.cpp:)
...
= -7

OSX clang release:

 attempting to print a backtrace:
  10 detail::GetBacktraceCallstack() (+65)
   9 kGetBacktrace() (+53)
   8 kGetRuntimeStack() (+35)
   7 kCrashExitExt() (+2815)
   6 _sigtramp() (+29)
   5 folly::fbstring_core::reserveSmall() (+207)
...
= -5

OSX clang debug:

 attempting to print a backtrace:
  25 detail::GetBacktraceCallstack() (kgetruntimestack.cpp)
  24 detail::GetBacktraceVector() (kgetruntimestack.cpp)
  23 kGetBacktrace() (kgetruntimestack.cpp)
  22 kGetRuntimeStack() (kgetruntimestack.cpp)
  21 (kcrashexit.cpp)
  20 _sigtramp() (+29)
  19 0x0000ffff()
...
= -6

 therefore: search for kGetBacktrace, and peel another frame off
            depending if the call came through kGetBacktrace or
            kGetRuntimeStack ..

 */

//-----------------------------------------------------------------------------
FrameVec GetBacktraceCallstack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	FrameVec Frames;

	enum   { MAXSTACK = 500 };
	void*  Stack[MAXSTACK+1];
	int    iStackSize { 0 };

	iStackSize = backtrace (Stack, MAXSTACK);
	char** Names = backtrace_symbols (Stack, iStackSize);

	std::vector<KStringView> Addresses;

	for (int ii = 0; ii < iStackSize; ++ii)
	{
		Addresses.push_back(GetStackAddress(Names[ii]));
	}

	auto vStack = Addr2Line(Addresses);

	free (Names);

	bool bHadkGetBacktrace { false };

	for (const auto& it : vStack)
	{
		KStackFrame Frame(it);

		if (!bHadkGetBacktrace)
		{
			auto sFunction = kNormalizeFunctionName(Frame.sFunction);
			if (sFunction.starts_with("kGetBacktrace"))
			{
				bHadkGetBacktrace = true;
			}
			continue;
		}
		else if (iSkipStackLines > 0)
		{
			--iSkipStackLines;
			continue;
		}

		Frames.push_back(std::move(Frame));
	}

	if (Frames.empty())
	{
		// return all frames if above filter did not match
		for (const auto& it : vStack)
		{
			Frames.push_back(KStackFrame(it));
		}
	}

	return Frames;

} // GetBacktraceCallstack

#endif

//-----------------------------------------------------------------------------
KStringView RemoveFunctionParms(KStringView sFunction)
//-----------------------------------------------------------------------------
{
	// scan forward until the first '(' on level 0. '<' increases level, '>' decreases

	int iLevel = 0;

	for (size_t i = 0; i < sFunction.size(); ++i)
	{
		switch (sFunction[i])
		{
			case '<':
				++iLevel;
				break;

			case '>':
				--iLevel;
				break;

			case '(':
				if (!iLevel)
				{
					sFunction.erase(i);
					return sFunction;
				}
				break;
		}
	}

	return sFunction;

} // RemoveFunctionParms

} // end of namespace bt
} // end of namespace detail

//-----------------------------------------------------------------------------
KString kNormalizeFunctionName(KStringView sFunctionName)
//-----------------------------------------------------------------------------
{
	KString sReturn;

	sFunctionName = detail::bt::RemoveFunctionParms(sFunctionName);

	// now scan back until first space, but take care to skip template types (<xyz<abc> >)
	int iTLevel { 0 };
	KStringView::size_type iTStart { 0 };
	KStringView::size_type iTEnd { 0 };
	bool bFound { false };
	auto iSig { sFunctionName.size() };

	while (iSig && !bFound)
	{
		switch (sFunctionName[--iSig])
		{
			case '<':
				--iTLevel;
				if (!iTLevel && iTEnd)
				{
					iTStart = iSig;
				}
				break;

			case '>':
				if (!iTLevel && !iTStart)
				{
					iTEnd = iSig;
				}
				++iTLevel;
				break;

			case ' ':
				if (!iTLevel)
				{
					++iSig;
					bFound = true;
					// this ends the while loop
				}
				break;

		}
	}

	if (iTStart && iTEnd)
	{
		sReturn = sFunctionName.Mid(iSig, iTStart - iSig);
		sFunctionName.remove_prefix(iTEnd + 1);
	}
	else
	{
		sFunctionName.remove_prefix(iSig);
	}

	sReturn += sFunctionName;

	sReturn.Replace("dekaf2::", "");
	sReturn.Replace("::self_type", "");
	sReturn.Replace("::self", "");
	sReturn.remove_prefix("&");

	return sReturn;

} // kNormalizeFunctionName

//-----------------------------------------------------------------------------
KString kGetBacktrace (int iSkipStackLines, bool bNormalize)
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	sStack = detail::bt::PrintFrameVector(detail::bt::GetBacktraceCallstack(iSkipStackLines), bNormalize);

#endif

	return sStack;

} // kGetBacktrace

//-----------------------------------------------------------------------------
KString kGetRuntimeStack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KString sStack;

	// account for own stack frame
	++iSkipStackLines;

#if SUPPORT_GDBATTACH_PRINTCALLSTACK
	sStack = detail::bt::PrintStackVector(detail::bt::GetGDBCallstack(iSkipStackLines));
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
	// fall back to libc based backtrace if GDB is not available
	if (sStack.empty())
	{
		sStack = kGetBacktrace(iSkipStackLines, false);
	}
#endif

	return sStack;

} // kGetRuntimeStack

//-----------------------------------------------------------------------------
KJSON kGetRuntimeStackJSON (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KJSON jStack = KJSON::array();

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
	// account for own stack frame
	++iSkipStackLines;

	auto sStack = detail::bt::GetBacktraceCallstack(iSkipStackLines);

	for (auto& item : sStack)
	{
		jStack += item.Serialize();
	}
#endif

	return jStack;

} // kGetRuntimeStackJSON

//-----------------------------------------------------------------------------
KStackFrame kFilterTrace (int iSkipStackLines, KStringView sSkipFiles)
//-----------------------------------------------------------------------------
{
	// account for own stack frame
	++iSkipStackLines;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
	auto Stack = detail::bt::GetBacktraceCallstack(iSkipStackLines);

	for (auto& Frame : Stack)
	{
		if (!Frame.sFile.In(sSkipFiles))
		{
			return Frame;
		}
	}
#endif

	return {};

} // kFilterTrace

//-----------------------------------------------------------------------------
KString kGetAddress2Line(KStringView sAddresses)
//-----------------------------------------------------------------------------
{
	auto vAddresses = sAddresses.Split(' ');

	auto vResult = detail::bt::Addr2Line(vAddresses);

	if (vResult.size() == 1)
	{
		return vResult.front();
	}
	else
	{
		return detail::bt::PrintStackVector(vResult);
	}

} // kGetAddress2Line

//-----------------------------------------------------------------------------
KString kGetAddress2Line(const void* pAddress)
//-----------------------------------------------------------------------------
{
	return kGetAddress2Line(kFormat("{}", pAddress));

} // kGetAddress2Line

//-----------------------------------------------------------------------------
KStackFrame::KStackFrame(KStringView sTraceline)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_OSX
	// parse atos-output
	//
	// in debug builds, the format is:
	// dekaf2::KOptions::Parse(int, char const* const*, dekaf2::KOutStream&) (koptions.cpp:685)
	// isolate filename
	if (!sTraceline.empty() && sTraceline.back() == ')')
	{
		// keep the original trace line intact, we may need it later
		auto sInput = sTraceline;
		auto fend = sInput.rfind('(');
		auto pos = fend;
		if (pos != KStringView::npos)
		{
			++pos;
			auto sFileAndLine = sInput.ToView(pos, sInput.size()-(pos+1));
#elif DEKAF2_IS_UNIX
	// parse addr2line output
	// isolate filename
	if (!sTraceline.empty())
	{
		// keep the original trace line intact, we may need it later
		auto sInput = sTraceline;
		auto fend = sInput.rfind(" at ");
		auto pos = fend;
		if (pos != KStringView::npos)
		{
			pos += 3;
		}
		if (pos != KStringView::npos)
		{
			++pos;
			auto end = sInput.find(' ', pos);
			if (end == KStringView::npos)
			{
				end = sInput.size();
			}
			auto sFileAndLine = sInput.ToView(pos, end - pos);
#endif
#ifdef DEKAF2_IS_UNIX // UNIX includes OSX
			auto sFile = sFileAndLine;
			auto sLine = sFileAndLine;

			pos = sFile.find(':');
			if (pos != KStringView::npos)
			{
				sLine.remove_prefix(pos + 1);
				sFile.remove_suffix(sFile.size() - pos);
			}

			if (fend != KStringView::npos)
			{
				sInput.remove_suffix(sInput.size() - fend);
			}

			sInput.TrimRight();
			*this = { sInput, sFile, sLine };
		}
	}
#endif

	if (sFunction.empty() && !sTraceline.empty())
	{
#ifdef DEKAF2_IS_OSX
		// parse atos-output
		//
		// in release builds, atos often returns function name + byte offset
		// like: _sigtramp + 29
		auto iPlus = sTraceline.rfind(" + ");

		if (iPlus != KStringView::npos)
		{
			sLineNumber = sTraceline.ToView(iPlus + 3);
			sFunction   = sTraceline.ToView(0, iPlus);
		}
#endif
		if (sFunction.empty())
		{
			// just print what you have (this may include addr2line output that has
			// neither filename nor line number, only the naked function)
			sFunction = sTraceline;
		}
	}

} // ctor

//-----------------------------------------------------------------------------
KStackFrame::KStackFrame(KString _sFunction, KString _sFile, KString _sLineNumber)
//-----------------------------------------------------------------------------
	: sFunction(std::move(_sFunction))
	, sFile(std::move(_sFile))
	, sLineNumber(std::move(_sLineNumber))
{
} // ctor

//-----------------------------------------------------------------------------
KString KStackFrame::Serialize(bool bNormalize) const
//-----------------------------------------------------------------------------
{
	KString sLine;

	if (bNormalize)
	{
		sLine = kNormalizeFunctionName(sFunction);

		if (!sLine.empty())
		{
			sLine += "() ";
		}
	}
	else
	{
		sLine = sFunction;

		if (!sLine.empty())
		{
			sLine += ' ';
		}
	}

	if (!sFile.empty())
	{
		sLine += '(';
		sLine += sFile;

		if (!sLineNumber.empty())
		{
			sLine += ':';
			sLine += sLineNumber;
		}
		
		sLine += ')';
	}
	else if (!sLineNumber.empty() && sLineNumber != "?")
	{
		// e.g. atos byte offset
		sLine += '(';
		sLine += '+';
		sLine += sLineNumber;
		sLine += ')';
	}

	return sLine;

} // KStackFrame::Serialize

} // of namespace dekaf2
