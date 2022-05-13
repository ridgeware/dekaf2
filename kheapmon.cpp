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

#include "kheapmon.h"
#include "kconfiguration.h"

#ifdef DEKAF2_HAS_JEMALLOC

#include "klog.h"
#include "kfilesystem.h"
#include "kreader.h"
#include <jemalloc/jemalloc.h>

namespace dekaf2 {
namespace Heap {

namespace jemalloc {

//---------------------------------------------------------------------------
template<typename Out>
bool Control(const char* sName, Out& out)
//---------------------------------------------------------------------------
{
	size_t iLen = sizeof(Out);

	int iError = mallctl(sName, &out, &iLen, 0, 0);

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
template<typename Out, typename In>
bool Control(const char* sName, Out& out, In in)
//---------------------------------------------------------------------------
{
	size_t iLenOut = sizeof(Out);
	size_t iLenIn  = sizeof(In);

	int iError = mallctl(sName, &out, &iLenOut, &in, iLenIn);

	if (!iError)
	{
		return true;
		if (iError == EAGAIN) {}
	}
	else
	{
		kDebug(1, "{}", strerror(iError));
		return false;
	}
}

namespace detail {

//---------------------------------------------------------------------------
/// callback for malloc_stats_print
void PrintFromJEMalloc(void* theString, const char* sMessage)
//---------------------------------------------------------------------------
{
	if (theString && sMessage)
	{
		auto sStats = static_cast<KString*>(theString);
		*sStats += sMessage;
	}
}

} // namespace detail

//---------------------------------------------------------------------------
KString GetStats(bool bAsJSON)
//---------------------------------------------------------------------------
{
	//	void malloc_stats_print(	void (*write_cb) (void *, const char *) ,
	//							void *cbopaque,
	//							const char *opts);

	KString sStats;

	malloc_stats_print(&detail::PrintFromJEMalloc, &sStats, bAsJSON ? "J" : "");

	return sStats;
}

} // namespace jemalloc

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

		if (jemalloc::Control("config.prof", bAllowed))
		{
			if (bAllowed)
			{
				if (jemalloc::Control("opt.prof_active", bAllowed))
				{
					return bAllowed;
				}
			}
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

	bool bReturn = IsAvailable() && jemalloc::Control("prof.active", bWasActive, true);

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

	return IsAvailable() && jemalloc::Control("prof.active", bWasActive, false);
}

//---------------------------------------------------------------------------
bool Dump(KStringViewZ sDumpFile)
//---------------------------------------------------------------------------
{
	const char* sFile = sDumpFile.c_str();

	return IsAvailable() && jemalloc::Control("prof.dump", sFile);
}

//---------------------------------------------------------------------------
KString Dump()
//---------------------------------------------------------------------------
{
	KString sDumped;

	if (IsAvailable())
	{
		KTempDir TempDir;

		auto sFile = kFormat("{}{}{}", TempDir.Name(), kDirSep, "dump.out");

		if (Dump(sFile))
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
	int v;
	return IsAvailable() && jemalloc::Control("prof.reset", v);
}

//---------------------------------------------------------------------------
bool IsStarted()
//---------------------------------------------------------------------------
{
	bool bStarted;

	if (IsAvailable() && jemalloc::Control("prof.active", bStarted))
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
} // dekaf2

#else // no jemalloc

namespace dekaf2    {
namespace Heap      {

KString GetStats    (bool)           { return false;     }

namespace Profiling {

bool    IsAvailable ()               { return false;     }
bool    Start       ()               { return false;     }
bool    Stop        ()               { return false;     }
bool    Dump        (KStringViewZ)   { return false;     }
KString Dump        ()               { return KString{}; }
bool    Reset       ()               { return false;     }
bool    Started     ()               { return false;     }

} // Heap
} // dekaf2

#endif
