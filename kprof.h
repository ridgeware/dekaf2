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

#pragma once

/// @file kprof.h
/// provides a code profiler

#if defined(DEKAF2_ENABLE_PROFILING) || defined(DEKAF2_LIBRARY_BUILD)

#include <map>
#include <cinttypes>
#include <cstring>
#include <chrono>
#include <mutex>
#include <atomic>

namespace dekaf2 {
namespace enabled {

class KProf;

extern const char* g_empty_label;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// "Quick-and-Dirty" profiling helper
/// - simply prints one line per call with label and used usecs
/// - can be disabled and enabled on command, e.g. for branch analysis
class KQDProf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	typedef std::chrono::steady_clock clock_t;

	//-----------------------------------------------------------------------------
	explicit KQDProf(const char* label = g_empty_label)
	//-----------------------------------------------------------------------------
		: m_label(label)
		, m_start(clock_t::now())
	{
	}

	//-----------------------------------------------------------------------------
	~KQDProf()
	//-----------------------------------------------------------------------------
	{
		if (m_print)
		{
#if defined (__APPLE__) || defined (__arm__) || defined (_MSC_VER)
			fprintf(stdout, "KQDProf::%-25.25s %10lli nsecs\n", m_label, (clock_t::now()-m_start).count());
#else
			fprintf(stdout, "KQDProf::%-25.25s %10li nsecs\n", m_label, (clock_t::now()-m_start).count());
#endif
		}
	}

	//-----------------------------------------------------------------------------
	void disable()
	//-----------------------------------------------------------------------------
	{
		m_print = false;
	}

	//-----------------------------------------------------------------------------
	void enable()
	//-----------------------------------------------------------------------------
	{
		m_print = true;
	}

//----------
private:
//----------

	const char*         m_label;
	bool                m_print{true};
	clock_t::time_point m_start;

}; // KQDProf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// profiler class, working single process, single thread, multi threaded
class KSharedProfiler
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	friend class KProf;

//----------
public:
//----------

	typedef std::chrono::steady_clock clock_t;

	//-----------------------------------------------------------------------------
	KSharedProfiler();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KSharedProfiler();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// put this profiler to sleep when it is waiting
	/// for another thread (with own profiler) to finish
	void sleep()
	//-----------------------------------------------------------------------------
	{
		m_sleeps_since = clock_t::now();
	}

	//-----------------------------------------------------------------------------
	/// wake this profiler up when the other thread has
	/// finished
	void wake()
	//-----------------------------------------------------------------------------
	{
		m_slept_for += clock_t::now() - m_sleeps_since;
	}

	//-----------------------------------------------------------------------------
	/// we need to call this on Windows in user code because automatic result output
	/// there comes too late in the process deinitialization
	void finalize();
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	typedef std::chrono::nanoseconds duration_t;

	struct data_t
	{
		//-----------------------------------------------------------------------------
		data_t()
		//-----------------------------------------------------------------------------
		{
		}

		//-----------------------------------------------------------------------------
		data_t(uint32_t level_p, uint32_t order_p)
		//-----------------------------------------------------------------------------
			: level(level_p)
			, order(order_p)
		{
		}

		//-----------------------------------------------------------------------------
		data_t& operator +=(const data_t& d);
		//-----------------------------------------------------------------------------

		uint64_t   count{0};
		uint32_t   level{0};
		uint32_t   order{0};
		duration_t duration{0};

	}; // data_t

	struct less_for_c_strings
	{
		//-----------------------------------------------------------------------------
		bool operator()(const char* p1, const char* p2) const
		//-----------------------------------------------------------------------------
		{
			return std::strcmp(p1, p2) < 0;
		}
	};

	typedef std::map<const char*, data_t, less_for_c_strings> map_t;

	//-----------------------------------------------------------------------------
	KSharedProfiler& operator+=(const KSharedProfiler& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void print();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint32_t level_up()
	//-----------------------------------------------------------------------------
	{
		// the postinc is on purpose to make 0 the first level
		return m_level++;
	}

	//-----------------------------------------------------------------------------
	void level_down()
	//-----------------------------------------------------------------------------
	{
		--m_level;
	}

	//-----------------------------------------------------------------------------
	uint32_t current_level()
	//-----------------------------------------------------------------------------
	{
		return m_level;
	}

	//-----------------------------------------------------------------------------
	map_t::iterator find(const char* label, uint32_t level = 0);
	//-----------------------------------------------------------------------------

	static size_t                s_refcount;
	static KSharedProfiler*      s_parent;
	static std::mutex            s_constructor_mutex;
	static std::atomic<uint32_t> s_order;

	clock_t::time_point          m_start;             // used for percentage calculations
	clock_t::time_point          m_sleeps_since;
	duration_t                   m_profiled_runtime;  // for how long did this profiler wake?
	duration_t                   m_slept_for;
	map_t                        m_map;
	uint32_t                     m_level{0};
	bool                         m_bFinalized{false};

}; // KSharedProfiler

#ifdef _MSC_VER

#pragma optimize("", off)

	inline void KeepVar(const void*) {}
	inline void KeepMe() {}

#pragma optimize("", on)

#endif

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// utility (RAII) class used to measure and count activity
/// it has to stay in the same thread as the parent it is reporting
/// to, or it will cause races.
class KProf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// keep a variable from being optimized away
	static void Force(void *p)
	//-----------------------------------------------------------------------------
	{
#ifdef _MSC_VER
		KeepVar(p);
#else
		__asm__ __volatile__ ("" : "+g"(p) : "g"(p) : "memory");
#endif
	}

	//-----------------------------------------------------------------------------
	/// keep a loop from being optimized away
	static void Force()
	//-----------------------------------------------------------------------------
	{
#ifdef _MSC_VER
		KeepMe();
#else
		__asm__ __volatile__ ("" : : : "memory");
#endif
	}

#ifdef DEKAF2_DISABLE_AUTOMATIC_PROFILER
	//-----------------------------------------------------------------------------
	explicit KProf(SharedProfiler& parent, const char* label, bool increment_level = true);
	//-----------------------------------------------------------------------------
#else
	//-----------------------------------------------------------------------------
	explicit KProf(const char* label, bool increment_level = true);
	//-----------------------------------------------------------------------------
#endif
	//-----------------------------------------------------------------------------
	~KProf();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetMultiplier(size_t iMultiplier)
	//-----------------------------------------------------------------------------
	{
		m_callct_multiplier = iMultiplier;
	}

	//-----------------------------------------------------------------------------
	void start();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void stop();
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	KSharedProfiler&                     m_parent;
	KSharedProfiler::map_t::iterator     m_perfcounter;
	KSharedProfiler::clock_t::time_point m_start;
	KSharedProfiler::duration_t          m_slept_before;
	KSharedProfiler::data_t              m_data;
	size_t                               m_callct_multiplier{1};
	bool                                 m_stopped{false};
	bool                                 m_increment_level{true};

}; // KProf

} // of namespace enabled
} // namespace dekaf2

#endif // DEKAF2_ENABLE_PROFILING || DEKAF2_LIBRARY_BUILD

#if !defined(DEKAF2_ENABLE_PROFILING)

namespace dekaf2 {
namespace disabled {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KQDProf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	explicit KQDProf(const char* label = "")
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	~KQDProf()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void disable()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void enable()
	//-----------------------------------------------------------------------------
	{
	}

}; // KQDProf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the nil version that generates no code at all
class KSharedProfiler
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KSharedProfiler()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KSharedProfiler(KSharedProfiler& parent)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	~KSharedProfiler()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void sleep()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void wake()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void finalize()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void print()
	//-----------------------------------------------------------------------------
	{
	}

}; // KSharedProfiler

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the nil version that generates no code at all
class KProf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	static void Force(void *p)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	static void Force()
	//-----------------------------------------------------------------------------
	{
	}

#ifdef DEKAF2_DISABLE_AUTOMATIC_PROFILER
	//-----------------------------------------------------------------------------
	KProf(KSharedProfiler& parent, const char* label, bool increment_level = true) {}
	//-----------------------------------------------------------------------------
#else
	//-----------------------------------------------------------------------------
	KProf(const char* label, bool increment_level = true)
	//-----------------------------------------------------------------------------
	{
	}

#endif

	//-----------------------------------------------------------------------------
	~KProf()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void start()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	void stop()
	//-----------------------------------------------------------------------------
	{
	}

}; // KProf

} // of namespace disabled

} // of namespace dekaf2

#endif // DEKAF2_ENABLE_PROFILING == false

namespace dekaf2
{

#if defined(DEKAF2_ENABLE_PROFILING) || defined(DEKAF2_LIBRARY_BUILD)
#ifndef DEKAF2_DISABLE_AUTOMATIC_PROFILER
extern thread_local enabled::KSharedProfiler g_Prof;
#endif
#endif

#if defined(DEKAF2_ENABLE_PROFILING)
typedef enabled::KQDProf KQDProf;
typedef enabled::KSharedProfiler SharedProfiler;
typedef enabled::KProf KProf;

#ifndef DEKAF2_DISABLE_AUTOMATIC_PROFILER
inline void kProfSleep()
{
	g_Prof.sleep();
}
inline void kProfWake()
{
	g_Prof.wake();
}
inline void kProfFinalize()
{
	g_Prof.finalize();
}
#endif // DEKAF2_DISABLE_AUTOMATIC_PROFILER

#else // DEKAF2_ENABLE_PROFILING

typedef disabled::KQDProf KQDProf;
typedef disabled::KSharedProfiler KSharedProfiler;
typedef disabled::KProf KProf;

#ifndef DEKAF2_DISABLE_AUTOMATIC_PROFILER
inline void kProfSleep()
{}
inline void kProfWake()
{}
inline void kProfFinalize()
{}
#endif

#endif // DEKAF2_ENABLE_PROFILING == false

} // of namespace dekaf2

