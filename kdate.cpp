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

#include "kdate.h"
#include "ktime.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KString KConstDate::to_string (KStringView sFormat) const
//-----------------------------------------------------------------------------
{
	if (empty())
	{
		return KString();
	}

	return detail::FormTimestamp(to_tm(), sFormat);

} // to_string

//-----------------------------------------------------------------------------
KString KConstDate::to_string (const std::locale& locale, KStringView sFormat) const
//-----------------------------------------------------------------------------
{
	if (empty())
	{
		return KString();
	}

	return detail::FormTimestamp(locale, to_tm(), sFormat);

} // to_string

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
		sDateDiff += kFormat("{}y", years().count());
	}

	if (months() > chrono::months(0))
	{
		if (years() > chrono::years(0)) 
		{
			sDateDiff += ' ';
		}

		sDateDiff += kFormat("{}m", months().count());
	}

	if (days() > chrono::days(0) || sDateDiff.size() < 2)
	{
		if (years() > chrono::years(0) || months() > chrono::months(0))
		{
			sDateDiff += ' ';
		}

		sDateDiff += kFormat("{}d", days().count());
	}
	
	return sDateDiff;

} // to_string

//-----------------------------------------------------------------------------
KString KDays::to_string () const
//-----------------------------------------------------------------------------
{
	return kFormat("{}d", to_days().count());

} // to_string

DEKAF2_NAMESPACE_END

