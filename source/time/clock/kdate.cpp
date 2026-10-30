/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2023, Ridgeware, Inc.
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

#include <dekaf2/time/clock/kdate.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
void ReportInvalidMonth(const chrono::year_month_day& ymd)
//-----------------------------------------------------------------------------
{
	// the error is in the computation of the date, so name the first caller outside of the date
	// and time code
	kTraceDownCaller(2, "klog.cpp,klog.h,kgetruntimestack.cpp,kgetruntimestack.h,kdate.cpp,kdate.h,ktime.cpp,ktime.h",
	                 kFormat("invalid date {}-{:02}-{:02}, the month is outside of 1 to 12",
	                         static_cast<int>(ymd.year()),
	                         static_cast<unsigned>(ymd.month()),
	                         static_cast<unsigned>(ymd.day())));

} // ReportInvalidMonth

} // end of namespace detail

//-----------------------------------------------------------------------------
KString KDateDiff::to_string () const
//-----------------------------------------------------------------------------
{
	KString sDateDiff;

	if (is_negative())
	{
		sDateDiff += '-';
	}

	// we currently do not use the duration formatting of fmt because it displays
	// years, months and days as multiples of seconds

	if (years() > chrono::years(0))
	{
		kFormatAppend(sDateDiff, "{}y", years().count());
	}

	if (months() > chrono::months(0))
	{
		if (years() > chrono::years(0)) 
		{
			sDateDiff += ' ';
		}

		kFormatAppend(sDateDiff, "{}m", months().count());
	}

	if (days() > chrono::days(0) || sDateDiff.size() < 2)
	{
		if (years() > chrono::years(0) || months() > chrono::months(0))
		{
			sDateDiff += ' ';
		}

		kFormatAppend(sDateDiff, "{}d", days().count());
	}
	
	return sDateDiff;

} // to_string

DEKAF2_NAMESPACE_END

