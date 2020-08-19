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

#include <vector>
#include "bits/kcppcompat.h"
#include "kstring.h"
#include "kstack.h"
#include "ksplit.h"
#include "kinshell.h"
#include "kgetruntimestack.h"

#ifdef DEKAF2_IS_UNIX
#include <execinfo.h>          // for backtrace
#include <cxxabi.h>            // for demangling
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

namespace dekaf2
{

#define NUM_ELEMENTS(X) (sizeof(X)/sizeof((X)[0]))

using StringVec = std::vector<KString>;
using FrameVec  = std::vector<KStackFrame>;

namespace detail
{

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

			KString sCmdLine = "atos -p ";
			sCmdLine += std::to_string(getpid());
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

			char sMyExeName[512];
			ssize_t n;
			if ( (n = readlink ("/proc/self/exe", sMyExeName, NUM_ELEMENTS (sMyExeName)-1)) > 0)
			{
				sMyExeName[n] = '\0';

				KString sCmdLine = "addr2line -f -C -e \"";
				sCmdLine += sMyExeName;
				sCmdLine += '"';
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
		return KString();
	}

	return sBacktraceLine.substr(i + 1, e - i - 1);

} // GetStackAddress

} // namespace detail

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
		#define SUPPORT_BACKTRACE_PRINTCALLSTACK 1
	#endif
#endif

#if SUPPORT_GDBATTACH_PRINTCALLSTACK

namespace detail
{

//-----------------------------------------------------------------------------
StringVec GetGDBCallstack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	StringVec Stack;

	DEKAF2_TRY
	{
		char name_buf[512];
		ssize_t iRead = readlink("/proc/self/exe", name_buf, NUM_ELEMENTS(name_buf)-1);
		if (iRead < 0)
		{
			return Stack;
		}
		name_buf[iRead] = 0;

		KString sCmdLine = "gdb --batch -n -ex thread -ex bt \"";
		sCmdLine += name_buf;
		sCmdLine += "\" ";
		sCmdLine += KString::to_string(getpid());
		sCmdLine += " 2>&1";

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

} // namespace detail
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
namespace detail
{

//-----------------------------------------------------------------------------
FrameVec GetBacktraceCallstack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	enum   { MAXSTACK = 500 };
	void*  Stack[MAXSTACK+1];
	int    iStackSize { 0 };

	iStackSize = backtrace (Stack, MAXSTACK);
	char** Names = backtrace_symbols (Stack, iStackSize);

	std::vector<KStringView> Addresses;

	// account for own stack frame
	++iSkipStackLines;

#ifndef DEKAF2_IS_OSX
	// need to skip one more frame on Linux
	++iSkipStackLines;
#endif

	for (int ii = iSkipStackLines; ii < iStackSize; ++ii)
	{
		Addresses.push_back(detail::GetStackAddress(Names[ii]));
	}

	auto vStack = Addr2Line(Addresses);

	free (Names);

	FrameVec Frames;

	for (const auto& it : vStack)
	{
		Frames.push_back(KStackFrame(it));
	}

	return Frames;

} // GetBacktraceCallstack

} // namespace detail
#endif

//-----------------------------------------------------------------------------
FrameVec kGetBacktraceVector (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	FrameVec Stack;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	// account for own stack frame
	++iSkipStackLines;

	Stack = detail::GetBacktraceCallstack(iSkipStackLines);

#endif

	return Stack;

} // kGetBacktraceVector

//-----------------------------------------------------------------------------
KString kGetBacktrace (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	// account for own stack frame
	++iSkipStackLines;

	sStack = detail::PrintFrameVector(kGetBacktraceVector(iSkipStackLines), true);

#endif

	return sStack;

} // kGetBacktrace

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

//-----------------------------------------------------------------------------
KString kNormalizeFunctionName(KStringView sFunctionName)
//-----------------------------------------------------------------------------
{
	KString sReturn;

	sFunctionName = RemoveFunctionParms(sFunctionName);

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

	return sReturn;

} // kNormalizeFunctionName

//-----------------------------------------------------------------------------
KString kGetRuntimeStack (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KString sStack;

	// account for own stack frame
	++iSkipStackLines;

#if SUPPORT_GDBATTACH_PRINTCALLSTACK
	sStack = detail::PrintStackVector(detail::GetGDBCallstack(iSkipStackLines));
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
	// fall back to libc based backtrace if GDB is not available
	if (sStack.empty())
	{
		sStack = kGetBacktrace(iSkipStackLines);
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

	auto sStack = kGetBacktraceVector(iSkipStackLines);

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

	auto Stack = kGetBacktraceVector(iSkipStackLines);

	for (auto& Frame : Stack)
	{
		if (!Frame.sFile.In(sSkipFiles))
		{
			return Frame;
		}
	}

	return {};

} // kFilterTrace

//-----------------------------------------------------------------------------
KString kGetAddress2Line(KStringView sAddresses)
//-----------------------------------------------------------------------------
{
	auto vAddresses = sAddresses.Split(" ");

	auto vResult = detail::Addr2Line(vAddresses);

	if (vResult.size() == 1)
	{
		return vResult.front();
	}
	else
	{
		return detail::PrintStackVector(vResult);
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
	// isolate filename
	if (!sTraceline.empty() && sTraceline.back() == ')')
	{
		auto fend = sTraceline.rfind('(');
		auto pos = fend;
		if (pos != KStringView::npos)
		{
			++pos;
			auto sFileAndLine = sTraceline.ToView(pos, sTraceline.size()-(pos+1));
#elif DEKAF2_IS_UNIX
	// parse addr2line output
	// isolate filename
	if (!sTraceline.empty())
	{
		auto fend = sTraceline.rfind('/');
		auto pos = fend;
		if (pos == KStringView::npos)
		{
			pos = sTraceline.rfind(" at ");
			if (pos != KStringView::npos)
			{
				pos += 3;
			}
		}
		if (pos != KStringView::npos)
		{
			++pos;
			auto end = sTraceline.find(' ', pos);
			if (end == KStringView::npos)
			{
				end = sTraceline.size();
			}
			auto sFileAndLine = sTraceline.ToView(pos, end - pos);
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

			sTraceline.remove_suffix(sTraceline.size() - fend);
			sTraceline.TrimRight();
			*this = { sTraceline, sFile, sLine };
		}
	}
#endif

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

	sLine += '(';
	sLine += sFile;
	sLine += ':';
	sLine += sLineNumber;
	sLine += ')';

	return sLine;

} // KStackFrame::Serialize


} // of namespace dekaf2

