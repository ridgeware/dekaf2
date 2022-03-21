/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "kron.h"
#include "kstringview.h"
#include "kstring.h"
#include "ktime.h"
#include "dekaf2.h"
#include "bits/kron_utils.h"
#include "croncpp.h"

namespace dekaf2 {

using KCronParser = cron_base<cron_standard_traits<KString, KStringView>, Kron_utils>;

namespace {

//-----------------------------------------------------------------------------
auto CronParserDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	delete static_cast<KCronParser::cronexpr*>(data);
};

//-----------------------------------------------------------------------------
inline const KCronParser::cronexpr* cxget(const KUniqueVoidPtr& p)
//-----------------------------------------------------------------------------
{
	return static_cast<KCronParser::cronexpr*>(p.get());
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KStringView Kron::Job::CutOffCommand(KStringView& sCronDef, uint16_t iElements)
//-----------------------------------------------------------------------------
{
	// split an input of "* 0/5 * * * ? fstrim -a >/dev/null 2>&1"
	// into "* 0/5 * * * ?" and "fstrim -a >/dev/null 2>&1"
	sCronDef.Trim();

	KStringView sCommand;

	// advance to first non-space after 6 fields, delimited by white space
	uint16_t iConsumedFields { 0 };
	KStringView::size_type iConsumedChars { 0 };
	bool bPassedField { false };

	for (const auto ch : sCronDef)
	{
		++iConsumedChars;

		if (!bPassedField)
		{
			if (KASCII::kIsSpace(ch))
			{
				bPassedField = true;
			}
		}
		else
		{
			if (KASCII::kIsSpace(ch) == false)
			{
				++iConsumedFields;

				if (iConsumedFields == iElements)
				{
					break;
				}
				else
				{
					bPassedField = false;
					continue;
				}
			}
		}
	}

	if (iConsumedFields == iElements)
	{
		sCommand = sCronDef.substr(iConsumedChars - 1);
		sCronDef.erase(iConsumedChars - 2);
	}

	return sCommand;

} // CutOffCommand

//-----------------------------------------------------------------------------
Kron::Job::Job(KStringView sCronDef, bool bHasSeconds)
//-----------------------------------------------------------------------------
{
	// we need to split in cron expression and command right here
	m_sCommand   = CutOffCommand(sCronDef, bHasSeconds ? 6 : 5);
	m_ParsedCron = KUniqueVoidPtr(new KCronParser::cronexpr(KCronParser::make_cron(sCronDef)),
															CronParserDeleter);
	m_tNext = KCronParser::cron_next(*cxget(m_ParsedCron), Dekaf::getInstance().GetCurrentTime());
	
} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(std::time_t tOnce, KStringView sCommand)
//-----------------------------------------------------------------------------
: m_sCommand(sCommand)
, m_tNext(tOnce)
{
} // ctor

//-----------------------------------------------------------------------------
time_t Kron::Job::Next(std::time_t tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		// either INVALID_TIME or time_t value
		return m_tNext;
	}

	if (tAfter == 0)
	{
		tAfter = Dekaf::getInstance().GetCurrentTime();
	}

	return KCronParser::cron_next(*cxget(m_ParsedCron), tAfter);

} // Next

//-----------------------------------------------------------------------------
KUTCTime Kron::Job::Next(const KUTCTime& tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		if (m_tNext != INVALID_TIME)
		{
			return m_tNext;
		}
		return KUTCTime();
	}

	return KCronParser::cron_next(*cxget(m_ParsedCron), tAfter.ToTM());

} // Next

} // end of namespace dekaf2
