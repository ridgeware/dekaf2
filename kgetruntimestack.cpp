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

#include <iostream>

namespace dekaf2
{

#define NUM_ELEMENTS(X) (sizeof(X)/sizeof((X)[0]))

namespace kgetruntimestack_detail
{

//-----------------------------------------------------------------------------
// This function will try and invoke the external addr2line code to map a vector
// of hex process addresses to a vector of source file/line numbers. On Mac OS X
// it will use atos instead of addr2line. It will return an empty vector upon
// failure.
std::vector<KString> Addr2LineMsg_ (const std::vector<KString>& vsAddress)
//-----------------------------------------------------------------------------
{
	std::vector<KString> vsResult;
	if (!vsAddress.empty())
	{
#ifdef DEKAF2_IS_OSX
		DEKAF2_TRY {
			KString sCmdLine = "atos -p ";
			sCmdLine += std::to_string(getpid());
			for (const auto& it : vsAddress)
			{
				sCmdLine += ' ';
				sCmdLine += it;
			}
			KInShell pipe;
			if (pipe.Open (sCmdLine))
			{
				KString sLineBuf;

				while (pipe.ReadLine (sLineBuf))
				{
					sLineBuf.TrimRight();
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
					vsResult.emplace_back(std::move(sLineBuf));
				}

				// add an empty line at the end of the stack trace
				vsResult.push_back("");

			}
		}

		DEKAF2_CATCH (...) {
			// ignore - just do default - returning empty string
		}
#elif DEKAF2_IS_UNIX
		DEKAF2_TRY {
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
				KInShell pipe;
				if (pipe.Open (sCmdLine))
				{
					KString sLineBuf;
					KString sResult;

					while (pipe.ReadLine (sLineBuf))
					{
						sLineBuf.TrimRight();
						sResult += sLineBuf;

						if (pipe.ReadLine (sLineBuf))
						{
							sLineBuf.TrimRight();
							if (!sLineBuf.StartsWith("??:")) {
								sResult += " at ";
								sResult += sLineBuf;
							}
						}

						vsResult.emplace_back(std::move(sResult));
					}

					// add an empty line at the end of the stack trace
					vsResult.push_back("");

				}
			}
		}

		DEKAF2_CATCH (...) {
			// ignore - just do default - returning empty string
		}
#endif
	}

	return vsResult;
}


//-----------------------------------------------------------------------------
KString GetStackAddress (const KString& sBacktraceLine)
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
		return KString();
	}
	size_t e = sBacktraceLine.find(cCloser, i+3);
	if (e == KString::npos)
	{
		return KString();
	}
	return sBacktraceLine.substr(i + 1, e - i - 1);
}

} // namespace kgetruntimestack_detail

#ifndef SUPPORT_GDBATTACH_PRINTCALLSTACK
#define SUPPORT_GDBATTACH_PRINTCALLSTACK 1
#endif

#ifdef WIN32
#define SUPPORT_GDBATTACH_PRINTCALLSTACK 0
#define SUPPORT_BACKTRACE_PRINTCALLSTACK 0
#endif

#if SUPPORT_GDBATTACH_PRINTCALLSTACK

namespace kgetruntimestack_detail
{

//-----------------------------------------------------------------------------
KString GetGDBAttachBased_Callstack_ ()
//-----------------------------------------------------------------------------
{
	KString sStack;

	DEKAF2_TRY
	{
		bool seenPrintGDBAttachedxxx = false;
		char name_buf[512];
		ssize_t iRead = readlink("/proc/self/exe", name_buf, NUM_ELEMENTS(name_buf)-1);
		if (iRead < 0)
		{
			return sStack;
		}
		name_buf[iRead] = 0;
		// don't use Format() inside the stacktrace
		KString sCmdLine = "gdb --batch -n -ex thread -ex bt \"";
		sCmdLine += name_buf;
		sCmdLine += "\" ";
		sCmdLine += std::to_string(getpid());
		sCmdLine += " 2>&1";
		KInShell pipe;

		if (pipe.Open (sCmdLine))
		{
			//enum {TIMEOUT_SEC=10};

			KString	sLineBuf;
			while (pipe.ReadLine(sLineBuf))
			{
				sLineBuf.TrimRight();
				if (0 == sLineBuf.length())
				{
					continue;			// skip blank lines
				}

				// Joe says to just skip these
				if (sLineBuf.StartsWith ("warning: (Internal error:"))
				{
					continue;			// skip blank lines
				}

				if (!seenPrintGDBAttachedxxx)
				{
					if (sLineBuf.find(__FUNCTION__) != KString::npos)
					{
						seenPrintGDBAttachedxxx = true;
					}
				}
				if (!seenPrintGDBAttachedxxx)
				{
					continue;			// skip prefixing lines (from our call to gdb)
				}

				sStack += sLineBuf;
				sStack += '\n';
			}
		}
	}

	DEKAF2_CATCH (...)
	{
		sStack.clear();
	}

	return sStack;
}

} // namespace kgetruntimestack_detail
#endif


#ifndef SUPPORT_BACKTRACE_PRINTCALLSTACK
#define SUPPORT_BACKTRACE_PRINTCALLSTACK 1
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
namespace kgetruntimestack_detail
{

//-----------------------------------------------------------------------------
KString GetBacktraceBased_Callstack_ (int iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KString sStack;

	enum   {MAXSTACK = 500};
	void*  Stack[MAXSTACK+1];
	int    iStackSize = 0;

	iStackSize = backtrace (Stack, MAXSTACK);
	char** Names = backtrace_symbols (Stack, iStackSize);

	std::vector<KString> vsAddress;
	for (int ii = iSkipStackLines; ii < iStackSize; ++ii)
	{
		vsAddress.emplace_back(kgetruntimestack_detail::GetStackAddress(Names[ii]));
	}

	free (Names);

	std::vector<KString> vsFrames = Addr2LineMsg_(vsAddress);

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
}

} // namespace kgetruntimestack_detail
#endif

//-----------------------------------------------------------------------------
KString kGetRuntimeStack (int iSkipStackLines /*=2*/)
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_GDBATTACH_PRINTCALLSTACK
	sStack = kgetruntimestack_detail::GetGDBAttachBased_Callstack_();
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
	// fall back to libc based backtrace if GDB is not available
	if (sStack.empty())
	{
		sStack = kgetruntimestack_detail::GetBacktraceBased_Callstack_(iSkipStackLines);
	}
#endif

	return sStack;

} // kGetRuntimeStack

//-----------------------------------------------------------------------------
KJSON kGetRuntimeStackJSON (int iSkipStackLines /*=3*/)
//-----------------------------------------------------------------------------
{
	KStack <KString> List;
	kSplit (List, kGetRuntimeStack (iSkipStackLines), "\n");

	KJSON aStack = KJSON::array();
	for (auto& item : List)	{
		aStack += item;
	}

	return aStack;

} // kGetRuntimeStackJSON

//-----------------------------------------------------------------------------
KString kGetBacktrace (int iSkipStackLines /*=2*/)
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	sStack = kgetruntimestack_detail::GetBacktraceBased_Callstack_(iSkipStackLines);

#endif

	return sStack;

} // kGetRuntimeStack

} // of namespace dekaf2

