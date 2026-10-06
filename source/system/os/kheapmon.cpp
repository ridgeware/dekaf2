/*
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2022, Ridgeware, Inc.
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
 */

#include <dekaf2/system/os/kheapmon.h>
#include "kconfiguration.h"

// on macos, jemalloc uses the prefix je_ in the symbols, but not in the headers..
// so we disable diagnostics there for now
#if defined(DEKAF2_HAS_JEMALLOC) && !defined(DEKAF2_IS_MACOS)

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/pipes/kinpipe.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/system/os/ksystem.h>
#include <algorithm>
#include <cerrno>
#include <vector>
#include <dekaf2/core/init/kcompatibility.h>
#include <jemalloc/jemalloc.h>

DEKAF2_NAMESPACE_BEGIN
namespace Heap     {
namespace jemalloc {
namespace detail   {

thread_local int iLastError { 0 };

//---------------------------------------------------------------------------
bool CheckError(int iError)
//---------------------------------------------------------------------------
{
	iLastError = iError;

	if (!iError)
	{
		return true;
	}
	else
	{
		kDebug(1, strerror(iError));
		return false;
	}
}

//---------------------------------------------------------------------------
/// callback for malloc_stats_print
void PrintFromJEMalloc(void* theString, const char* sMessage)
//---------------------------------------------------------------------------
{
	if (theString && sMessage)
	{
		auto sString = static_cast<KString*>(theString);
		*sString += sMessage;
	}
}

} // namespace detail

//---------------------------------------------------------------------------
template<typename Out>
bool ctlRead(const char* sName, Out& out)
//---------------------------------------------------------------------------
{
	size_t iLen = sizeof(Out);

	return detail::CheckError(mallctl(sName, &out, &iLen, nullptr, 0));
}

//---------------------------------------------------------------------------
template<typename In>
bool ctlWrite(const char* sName, In in)
//---------------------------------------------------------------------------
{
	size_t iLen = sizeof(In);

	return detail::CheckError(mallctl(sName, nullptr, nullptr, &in, iLen));
}

//---------------------------------------------------------------------------
template<typename Out, typename In>
bool ctlReadWrite(const char* sName, Out& out, In in)
//---------------------------------------------------------------------------
{
	size_t iLenOut = sizeof(Out);
	size_t iLenIn  = sizeof(In);

	return detail::CheckError(mallctl(sName, &out, &iLenOut, &in, iLenIn));
}

//---------------------------------------------------------------------------
KString GetStats(bool bAsJSON)
//---------------------------------------------------------------------------
{
	KString sStats;

	malloc_stats_print(&detail::PrintFromJEMalloc, &sStats, bAsJSON ? "J" : "");

	return sStats;
}

} // namespace jemalloc

//---------------------------------------------------------------------------
bool IsStandardMalloc()
//---------------------------------------------------------------------------
{
	return false;
}

//---------------------------------------------------------------------------
int LastError()
//---------------------------------------------------------------------------
{
	return jemalloc::detail::iLastError;
}

//---------------------------------------------------------------------------
KString GetStats(bool bAsJSON)
//---------------------------------------------------------------------------
{
	return jemalloc::GetStats(bAsJSON);
}

namespace Profiling {

//---------------------------------------------------------------------------
bool IsAvailable()
//---------------------------------------------------------------------------
{
	static bool s_bAllowed = []()
	{
		bool bAllowed;

		if (jemalloc::ctlRead("config.prof", bAllowed))
		{
			return bAllowed;
		}

		return false;
	}();

	return s_bAllowed;
}

//---------------------------------------------------------------------------
bool Start()
//---------------------------------------------------------------------------
{
	bool bWasActive;

	bool bReturn = IsAvailable() && jemalloc::ctlReadWrite("prof.active", bWasActive, true);

	if (bReturn && !bWasActive)
	{
		Reset();
	}

	return bReturn;
}

//---------------------------------------------------------------------------
bool Stop()
//---------------------------------------------------------------------------
{
	bool bWasActive;

	return IsAvailable() && jemalloc::ctlReadWrite("prof.active", bWasActive, false);
}

namespace {

//---------------------------------------------------------------------------
/// a jeprof option that callers may pass to Dump()
struct ReportOption
//---------------------------------------------------------------------------
{
	/// the value an option takes
	enum ValueType
	{
		Flag,     ///< no value, like --lines
		Integer,  ///< an unsigned integer, like --nodecount=80
		Fraction, ///< a decimal number, like --nodefraction=0.005
		Pattern   ///< a regular expression, like --focus=KString
	};

	KStringView sName;
	ValueType   Type;
};

// Only options that shape the report are accepted. jeprof options that start other
// programs (--tools, --gv, --evince, --web, --list, --disasm), read further files
// (--base, --add_lib, --lib_prefix) or change the output format are not.
constexpr ReportOption s_ReportOptions[]
{
	{ "inuse_space"   , ReportOption::Flag     },
	{ "inuse_objects" , ReportOption::Flag     },
	{ "alloc_space"   , ReportOption::Flag     },
	{ "alloc_objects" , ReportOption::Flag     },
	{ "show_bytes"    , ReportOption::Flag     },
	{ "drop_negative" , ReportOption::Flag     },
	{ "functions"     , ReportOption::Flag     },
	{ "lines"         , ReportOption::Flag     },
	{ "addresses"     , ReportOption::Flag     },
	{ "files"         , ReportOption::Flag     },
	{ "cum"           , ReportOption::Flag     },
	{ "nodecount"     , ReportOption::Integer  },
	{ "maxdegree"     , ReportOption::Integer  },
	{ "scale"         , ReportOption::Integer  },
	{ "thread"        , ReportOption::Integer  },
	{ "nodefraction"  , ReportOption::Fraction },
	{ "edgefraction"  , ReportOption::Fraction },
	{ "focus"         , ReportOption::Pattern  },
	{ "ignore"        , ReportOption::Pattern  },
	{ "retain"        , ReportOption::Pattern  },
	{ "exclude"       , ReportOption::Pattern  }
};

//---------------------------------------------------------------------------
/// appends the whitespace separated options of sOptions to Args, returns false
/// if one of them is not a report option of s_ReportOptions or has an invalid value
bool AddReportOptions(std::vector<KString>& Args, KStringView sOptions)
//---------------------------------------------------------------------------
{
	for (auto sOption : sOptions.Split(" \t\r\n", " \t\r\n", '\0', /*bCombineDelimiters=*/true, /*bRespectQuotes=*/false))
	{
		if (sOption.empty())
		{
			continue;
		}

		KStringView sName = sOption;

		if (!sName.remove_prefix("--"))
		{
			kDebug(1, "not an option: {}", sOption);
			return false;
		}

		KStringView sValue;
		bool bHasValue { false };

		auto iEquals = sName.find('=');

		if (iEquals != KStringView::npos)
		{
			sValue    = sName.substr(iEquals + 1);
			sName     = sName.substr(0, iEquals);
			bHasValue = true;
		}

		auto it = std::find_if(std::begin(s_ReportOptions), std::end(s_ReportOptions),
		                       [sName](const ReportOption& Option)
		{
			return Option.sName == sName;
		});

		if (it == std::end(s_ReportOptions))
		{
			kDebug(1, "option not permitted: {}", sOption);
			return false;
		}

		bool bValid { false };

		switch (it->Type)
		{
			case ReportOption::Flag:
				bValid = !bHasValue;
				break;

			case ReportOption::Integer:
				bValid = bHasValue && kIsUnsigned(sValue);
				break;

			case ReportOption::Fraction:
				bValid = bHasValue && (kIsUnsigned(sValue) || kIsFloat(sValue));
				break;

			case ReportOption::Pattern:
				bValid = bHasValue && !sValue.empty();
				break;
		}

		if (!bValid)
		{
			kDebug(1, "invalid value for option: {}", sOption);
			return false;
		}

		Args.emplace_back(sOption);
	}

	return true;
}

} // end of anonymous namespace

//---------------------------------------------------------------------------
bool Dump(KStringViewZ sDumpFile, ReportFormat Format, KStringView sAdditionalOptions)
//---------------------------------------------------------------------------
{
	if (!IsAvailable())
	{
		return false;
	}

	KStringView sFormat;

	switch (Format)
	{
		case ReportFormat::RAW:
			break;

		case ReportFormat::TEXT:
			sFormat = "--text";
			break;

		case ReportFormat::SVG:
			sFormat = "--svg";
			break;

		case ReportFormat::PDF:
			sFormat = "--pdf";
			break;
	}

	// jeprof runs without a shell, so every argument reaches it unchanged. The
	// options are checked before the dump is written, so a rejected call has no
	// side effects.
	std::vector<KString> Args;

	if (!sFormat.empty())
	{
		Args.emplace_back("jeprof");
		Args.emplace_back(sFormat);

		if (!AddReportOptions(Args, sAdditionalOptions))
		{
			jemalloc::detail::iLastError = EINVAL;
			return false;
		}

		Args.emplace_back(kGetOwnPathname());
		Args.emplace_back(sDumpFile);
	}

	if (!jemalloc::ctlWrite("prof.dump", sDumpFile.c_str()))
	{
		return false;
	}

	if (Args.empty())
	{
		// the raw dump is all we need
		return true;
	}

	KString sOutName { sDumpFile };
	sOutName += ".tmp";

	int iError { 0 };

	{
		KOutFile OutFile(sOutName);
		KInPipe  Pipe;

		if (!OutFile.is_open())
		{
			iError = errno ? errno : EIO;
		}
		else if (!Pipe.Open(std::move(Args)))
		{
			iError = Pipe.GetErrno() ? Pipe.GetErrno() : ECHILD;
		}
		else
		{
			OutFile.Write(Pipe);

			iError = Pipe.Close();

			if (!iError && !OutFile.good())
			{
				iError = EIO;
			}
		}
	}

	if (iError)
	{
		kRemoveFile(sOutName);
		jemalloc::detail::iLastError = iError;
		return false;
	}

	kRemoveFile(sDumpFile);

	if (!kRename(sOutName, sDumpFile))
	{
		jemalloc::detail::iLastError = errno;
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
KString Dump(ReportFormat Format, KStringView sAdditionalOptions)
//---------------------------------------------------------------------------
{
	KString sDumped;

	if (IsAvailable())
	{
		KTempDir TempDir;

		auto sFile = kFormat("{}{}{}", TempDir.Name(), kDirSep, "dump.out");

		if (Dump(sFile, Format, sAdditionalOptions))
		{
			sDumped = kReadAll(sFile);
		}
	}

	return sDumped;
}

//---------------------------------------------------------------------------
bool Reset()
//---------------------------------------------------------------------------
{
	return IsAvailable() && jemalloc::ctlWrite("prof.reset", std::size_t(0));
}

//---------------------------------------------------------------------------
bool IsStarted()
//---------------------------------------------------------------------------
{
	bool bStarted;

	if (IsAvailable() && jemalloc::ctlRead("prof.active", bStarted))
	{
		return bStarted;
	}
	else
	{
		return false;
	}
}

} // Profiling
} // Heap
DEKAF2_NAMESPACE_END

#else // no jemalloc

DEKAF2_NAMESPACE_BEGIN
namespace Heap      {

#if defined(DEKAF2_HAS_JEMALLOC)
bool    IsStandardMalloc()    { return false;     }
#else
bool    IsStandardMalloc()    { return true;      }
#endif
int     LastError   ()        { return EPERM;     }
KString GetStats    (bool)    { return KString{}; }

namespace Profiling {

bool    IsAvailable ()        { return false;     }
bool    Start       ()        { return false;     }
bool    Stop        ()        { return false;     }
bool    Dump        (KStringViewZ, ReportFormat, KStringView)
                              { return false;     }
KString Dump        (ReportFormat, KStringView)
                              { return KString{}; }
bool    Reset       ()        { return false;     }
bool    IsStarted   ()        { return false;     }

} // Profiling
} // Heap
DEKAF2_NAMESPACE_END

#endif
