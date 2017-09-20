/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kprof.h"

#if defined(ENABLE_PROFILING) || defined(DEKAF2_LIBRARY_BUILD)

#include <algorithm>
#include <set>
#include <cstring>
#include "kstringutils.h"

using namespace dekaf2::enabled;

size_t                KSharedProfiler::s_refcount{0};
KSharedProfiler*      KSharedProfiler::s_parent{nullptr};
std::mutex            KSharedProfiler::s_constructor_mutex;
std::atomic<uint32_t> KSharedProfiler::s_order{0};
const char*           dekaf2::enabled::g_empty_label{"(unnamed)"};

//-----------------------------------------------------------------------------
KSharedProfiler::KSharedProfiler()
//-----------------------------------------------------------------------------
	:	m_start{clock_t::now()}
{
	std::lock_guard<std::mutex> Lock(s_constructor_mutex);
	if (!s_refcount++)
	{
		s_parent = this;
	}
}

//-----------------------------------------------------------------------------
KSharedProfiler::~KSharedProfiler()
//-----------------------------------------------------------------------------
{
	m_profiled_runtime += (clock_t::now() - m_start) - m_slept_for;

	std::lock_guard<std::mutex> Lock(s_constructor_mutex);
	if (--s_refcount)
	{
		*s_parent += *this;
	}
	else
	{
		print();
	}
}

//-----------------------------------------------------------------------------
KSharedProfiler& KSharedProfiler::operator+=(const KSharedProfiler& other)
//-----------------------------------------------------------------------------
{
	for (const auto& it : other.m_map)
	{
		m_map[it.first] += it.second;
	}

	m_profiled_runtime += other.m_profiled_runtime;

	return *this;
}

//-----------------------------------------------------------------------------
void KSharedProfiler::print()
//-----------------------------------------------------------------------------
{
	// declare helpers for sorting
	typedef map_t::value_type set_value_t;
	struct compare_set
	{
		bool operator()(const set_value_t& a, const set_value_t& b)
		{
			return a.second.order < b.second.order;
		}
	};
	typedef std::multiset<set_value_t, compare_set> set_t;

	// sort result map into an ordered set
	// and compute max length of labels:
	size_t maxlen{0};
	set_t set;
	for (const auto& it : m_map)
	{
		maxlen = std::max(maxlen, std::strlen(it.first)+it.second.level*2);
		set.insert(it);
	}

	maxlen = std::max(maxlen, std::strlen("accumulated runtime"));
	int imaxlen = static_cast<int>(maxlen);

	// report:
	fprintf (stdout, "\n\nperformance stats:\n\n");
	for (const auto& it : set)
	{
		double  nPercent = (m_profiled_runtime.count())
				  ? (it.second.duration.count()*100.0)/(m_profiled_runtime.count()*1.0)
				  : 0.0;
		std::string dispname(it.second.level * 2, '.');
		dispname += it.first;
		fprintf (stdout,
				 "|  %-*.*s : %12s usecs : %5.1f%% : %10s calls : %16s nsecs\n",
				 imaxlen, imaxlen,
				 dispname.c_str(),
				 kFormNumber(std::chrono::duration_cast<std::chrono::microseconds>(it.second.duration).count()).c_str(),
				 nPercent,
				 kFormNumber(it.second.count).c_str(),
				 kFormNumber(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(it.second.duration).count()) / it.second.count).c_str()); // it.second.count is always non-zero..
	}

	fprintf (stdout, "\n");

	fprintf (stdout,
			 "|  %-*.*s : %12s usecs : 100.0%%\n",
			 imaxlen, imaxlen, "accumulated runtime",
			 kFormNumber(std::chrono::duration_cast<std::chrono::microseconds>(m_profiled_runtime).count()).c_str());

	fprintf (stdout,
			 "|  %-*.*s : %12s usecs\n",
			 imaxlen, imaxlen, "wall clock",
			 kFormNumber(std::chrono::duration_cast<std::chrono::microseconds>(clock_t::now()-m_start).count()).c_str());

	fprintf (stdout, "\n");

} // print

//-----------------------------------------------------------------------------
KSharedProfiler::map_t::iterator KSharedProfiler::find(const char* label, uint32_t level)
//-----------------------------------------------------------------------------
{
	if (!label || !*label)
	{
		label = g_empty_label;
	}

	auto it = m_map.find(label);
	if (it != m_map.end())
	{
		return it;
	}
	else
	{
		auto it2 = m_map.emplace(label, data_t(level, ++s_order));
		return it2.first;
	}
}


//-----------------------------------------------------------------------------
KSharedProfiler::data_t& KSharedProfiler::data_t::operator +=(const data_t& d)
//-----------------------------------------------------------------------------
{
	duration += d.duration;
	count += d.count;
	if (!order)
	{
		level = d.level;
		order = d.order;
	}
	return *this;
}

#ifndef DISABLE_AUTOMATIC_PROFILER
thread_local KSharedProfiler dekaf2::g_Prof;
#endif

//-----------------------------------------------------------------------------
#ifdef DISABLE_AUTOMATIC_PROFILER
KProf::KProf(KSharedProfiler& parent, const char* label, bool increment_level)
	: m_parent(parent)
#else
KProf::KProf(const char* label, bool increment_level)
	: m_parent(g_Prof)
#endif
//-----------------------------------------------------------------------------
	, m_increment_level(increment_level)
{
	if (increment_level)
	{
		m_data.level = m_parent.level_up();
	}
	else
	{
		m_data.level = m_parent.current_level();
	}
	m_perfcounter = m_parent.find(label, m_data.level);

	start();
}

//-----------------------------------------------------------------------------
KProf::~KProf()
//-----------------------------------------------------------------------------
{
	stop();
}

//-----------------------------------------------------------------------------
void KProf::start()
//-----------------------------------------------------------------------------
{
	if (!m_stopped)
	{
		m_slept_before = m_parent.m_slept_for;
		m_start = KSharedProfiler::clock_t::now();
	}
}

//-----------------------------------------------------------------------------
void KProf::stop()
//-----------------------------------------------------------------------------
{
	if (!m_stopped)
	{
		// add the new interval, but reduce by the time that the parent profiler has slept for
		KSharedProfiler::duration_t interval = KSharedProfiler::clock_t::now() - m_start;
		KSharedProfiler::duration_t slept_for = m_parent.m_slept_for - m_slept_before;
		interval -= slept_for;
		m_data.duration = interval;
		m_data.count = m_callct_multiplier;
		m_stopped = true;
		m_perfcounter->second += m_data;

		if (m_increment_level)
		{
			m_parent.level_down();
		}
	}
}

#endif // ENABLE_PROFILING
