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


#include "kstring.h"
#include "kcppcompat.h"
#include "kpipereader.h"
#include "kgetruntimestack.h"

#ifdef UNIX
#include <execinfo.h>          // for backtrace
#include <cxxabi.h>            // for demangling
#include <sys/resource.h>      // to allow core dumps
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

namespace dekaf2
{

namespace detail
{
	KString DemangleCPlusPlusName_ (const KString& sName)
	{
		KString sResult;
#ifdef UNIX
		int status = 0;
		char* realName = abi::__cxa_demangle (sName.c_str (), 0, 0, &status);
		if (status == 0)
		{
			sResult = realName;
		}
		else
		{
			// if demangling fails just return the original string
			sResult = sName;
		}
		if (realName != nullptr)
		{
			free (realName);
		}
#endif
		return sResult;
	}

	KString DemangleBacktraceDumpLine_ (const KString& sName)
	{
		// It appears empirically that the C++ symbol name appears between the ( and + characters, so find that, substitute, and return the rest
		size_t i = sName.find ('(');
		if (i == KString::npos)
		{
			return sName;
		}
		size_t e = sName.find ('+', i);
		if (e == KString::npos)
		{
			return sName;
		}
		KString sRet = sName.substr(0, i+1);
		sRet += DemangleCPlusPlusName_ (sName.substr(i + 1, e - i - 1));
		sRet += sName.substr (e);
		return sRet;
//		return sName.substr(0, i+1) + DemangleCPlusPlusName_ (sName.substr(i + 1, e - i - 1)) + sName.substr (e);
	}
} // namespace detail


#ifndef USE_ADDR2LINE
#define USE_ADDR2LINE 1
#endif

#define NUM_ELEMENTS(X) (sizeof(X)/sizeof((X)[0]))

namespace detail
{
	// This function will try and invoke the external addr2line code to map a hex process address to a source file/line number. It will
	// return an empty string upon failure
	KString Addr2LineMsg_ (const KString& sAddress)
	{
#ifdef UNIX
	#if USE_ADDR2LINE
		try {
			char sMyExeName[512];
			ssize_t n;
			if ( (n = readlink ("/proc/self/exe", sMyExeName, NUM_ELEMENTS (sMyExeName)-1)) < 0)
			{
				// if this fails - we are already crashing - don't worry about this...
				return KString();
			}
			sMyExeName[n] = '\0';

			KString sCmdLine = "addr2line -f -C -e \"";
			sCmdLine += sMyExeName;
			sCmdLine += "\" ";
			sCmdLine += sAddress;
			KInShell pipe;
			if (pipe.Open (sCmdLine))
			{
				KString	sLineBuf;
				KString sResult;
				enum {MAX = 1024};

				if (pipe.ReadLine (sLineBuf))
				{
					sLineBuf.TrimRight();
					sResult += sLineBuf;
				}

				if (pipe.ReadLine (sLineBuf))
				{
					sLineBuf.TrimRight();
					if (!sLineBuf.StartsWith("??:")) {
						sResult += " at ";
						sResult += sLineBuf;
					}
				}

				return sResult;
			}
		}
		catch (...) {
			// ignore - just do default - returning empty string
		}
	#endif
#endif
		return KString();
	}


	KString ComputeSrcLinesAppendageForBacktraceDumpLine_ (const KString& sBacktraceLine)
	{
		// It appears empirically that the C++ symbol name appears between the ( and + characters, so find that, substitute, and return the rest
		size_t i = sBacktraceLine.find ("[0x");
		if (i == KString::npos)
		{
			return KString();
		}
		size_t e = sBacktraceLine.find (']', i);
		if (e == KString::npos)
		{
			return KString();
		}
		return Addr2LineMsg_ (sBacktraceLine.substr(i + 1, e - i - 1));
	}
} // namespace detail

#ifndef SUPPORT_GDBATTACH_PRINTCALLSTACK
#define SUPPORT_GDBATTACH_PRINTCALLSTACK 1
#endif

#ifdef WIN32
#define SUPPORT_GDBATTACH_PRINTCALLSTACK 0
#define SUPPORT_BACKTRACE_PRINTCALLSTACK 0
#endif

#if SUPPORT_GDBATTACH_PRINTCALLSTACK

namespace detail
{
	KString GetGDBAttachBased_Callstack_ ()
	{
		KString sStack;

		try
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
				enum {MAX=1024};
				enum {TIMEOUT_SEC=10};

				KString	sLineBuf;
				while (pipe.ReadLine (sLineBuf))
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
		catch (...)
		{
			sStack.clear();
		}

		return sStack;
	}
} // namespace detail
#endif


#ifndef SUPPORT_BACKTRACE_PRINTCALLSTACK
#define SUPPORT_BACKTRACE_PRINTCALLSTACK 1
#endif

#if SUPPORT_BACKTRACE_PRINTCALLSTACK
namespace detail
{
	KString GetBacktraceBased_Callstack_ (size_t iSkipStackLines)
	{
		KString sStack;

		enum   {MAXSTACK = 500};
		void*  Stack[MAXSTACK+1];
		size_t iStackSize = 0;
	 
		iStackSize = backtrace (Stack, MAXSTACK);
		char** Names = backtrace_symbols (Stack, iStackSize);
	 
		for (size_t ii = iSkipStackLines; ii < iStackSize; ++ii)
		{
			KString sFrame;
			sFrame = std::to_string(iStackSize-ii);
			sFrame.PadLeft(3);
			sFrame += ' ';
//			sFrame += detail::DemangleBacktraceDumpLine_(Names[ii]);
			sFrame += detail::ComputeSrcLinesAppendageForBacktraceDumpLine_(Names[ii]);
			sFrame += '\n';
			sStack.append(sFrame);
		}
	 
		free (Names);

		return sStack;
	}
} // namespace detail
#endif

//-----------------------------------------------------------------------------
KString kGetRuntimeStack ()
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_GDBATTACH_PRINTCALLSTACK
	// Joe only wants one of these, but do old-style if gdb-style fails
	sStack = detail::GetGDBAttachBased_Callstack_();

#endif
#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	if (sStack.empty())
	{
		sStack = detail::GetBacktraceBased_Callstack_(0);
	}

#endif

	return sStack;

} // kGetRuntimeStack

//-----------------------------------------------------------------------------
KString kGetBacktrace (size_t iSkipStackLines)
//-----------------------------------------------------------------------------
{
	KString sStack;

#if SUPPORT_BACKTRACE_PRINTCALLSTACK

	sStack = detail::GetBacktraceBased_Callstack_(iSkipStackLines);

#endif

	return sStack;

} // kGetRuntimeStack

} // of namespace dekaf2

