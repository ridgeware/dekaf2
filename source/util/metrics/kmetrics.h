/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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


#pragma once

/// @file kmetrics.h
/// counters, gauges and histograms, with output in the Prometheus text format and as JSON

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/duration/kduration.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_metrics
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A registry of metrics: named values that a program counts or measures while
/// it runs, and that a monitoring system reads from time to time.
///
/// There are three types. A counter only grows (requests served). A gauge goes
/// up and down (requests in flight). A histogram counts observed values in
/// buckets with fixed upper bounds (durations of requests), from which the
/// reader computes percentiles.
///
/// A metric can have labels, like the route and the method of a request. Each
/// set of label values has its own value. Get() looks the value up under a lock
/// and returns a reference that stays valid as long as the registry lives: keep
/// the reference where the value changes often. Changing a value takes no lock
/// and allocates nothing.
///
/// Counters and histograms keep their values in cells: several copies, of which
/// a thread always writes the same one, and which a reader adds up. Threads that
/// run at the same time then do not write to the same memory, which would cost
/// far more than the count itself. There are at least four times as many cells
/// as hardware threads, a counter takes 8 bytes in each of them.
///
/// @code
/// KMetrics Metrics;
///
/// auto& Requests = Metrics.AddCounter  ("http_requests_total", "count of requests", { "route", "status" });
/// auto& Duration = Metrics.AddHistogram("http_request_duration_seconds", "duration of requests", { "route" });
///
/// auto& Users2xx      = Requests.Get({ "/users", "2xx" });
/// auto& UsersDuration = Duration.Get({ "/users" });
///
/// Users2xx.Inc();
/// UsersDuration.Observe(Stopwatch.elapsed());
///
/// // a value that is asked for when the metrics are read
/// Metrics.AddCallback("pool_objects_used", "used objects of the pool", KMetrics::Type::Gauge,
///                     [&Pool]() { return static_cast<double>(Pool.GetStats().iUsed); });
///
/// auto sText = Metrics.Serialize(); // the Prometheus text format
/// auto jAll  = Metrics.ToJSON();
/// @endcode
///
/// Names are not checked: the Prometheus format wants metric names of letters,
/// digits, '_' and ':', and label names of letters, digits and '_'.
class DEKAF2_PUBLIC KMetrics
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum class Type : uint8_t
	{
		Counter,
		Gauge,
		Histogram
	};

	using LabelNames  = std::vector<KString>;
	using LabelValues = std::vector<KString>;
	/// a function that returns the current value of a metric
	using Callback    = std::function<double()>;

	/// the content type of Serialize() for an HTTP response
	static constexpr KStringViewZ ContentType { "text/plain; version=0.0.4; charset=utf-8" };

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// @return a number for the calling thread, starting with 1, in the order of the first calls
	static uint32_t ThreadNumber () noexcept
	//-----------------------------------------------------------------------------
	{
		// constant initialized: the access needs no call of an initializer
		static thread_local uint32_t t_iNumber { 0 };

		if (DEKAF2_UNLIKELY(t_iNumber == 0))
		{
			t_iNumber = NextThreadNumber();
		}

		return t_iNumber;
	}

	static uint32_t NextThreadNumber () noexcept;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// The memory for the values of counters and histograms. A value has a slot,
	/// and every cell has all slots. A thread writes the slots of its cell, a
	/// reader adds a slot up over all cells. More threads than cells share cells,
	/// which costs time only while two of them run at once.
	class DEKAF2_PUBLIC Cells
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		// the slots come in chunks, which never move once they exist
		static constexpr std::size_t iChunkSlots = 256;
		static constexpr std::size_t iMaxChunks  = 1024;

		using Slots    = std::array<std::atomic<uint64_t>, iChunkSlots>;
		using iterator = Slots::iterator;

		explicit Cells (std::size_t iCells);

		/// reserve neighbouring slots, at most iChunkSlots
		/// @return the first of them
		std::size_t Allocate (std::size_t iCount);

		/// @return the slot in the cell of the calling thread
		iterator Mine (std::size_t iSlot) noexcept
		{
			auto& cell = m_Cells[ThreadNumber() & m_iMask];
			return cell.Chunks[iSlot / iChunkSlots]->Values.begin() + (iSlot % iChunkSlots);
		}

		/// @return the sum of a slot over all cells
		DEKAF2_NODISCARD
		uint64_t Sum (std::size_t iSlot) const noexcept;

		/// @return the count of cells
		DEKAF2_NODISCARD
		std::size_t size () const noexcept { return m_Cells.size(); }

	private:

		struct Chunk
		{
			// the chunks of two cells never share a cache line, wherever the allocator puts them
			std::array<char, 128> Before;
			Slots                 Values;
			std::array<char, 128> After;
		};

		struct Cell
		{
			std::array<std::unique_ptr<Chunk>, iMaxChunks> Chunks;
		};

		bool AddChunk (std::size_t iChunk);

		std::vector<Cell> m_Cells;
		std::size_t       m_iMask;
		std::mutex        m_Mutex;
		std::size_t       m_iNextSlot { 0 };
		bool              m_bFull     { false };

	}; // Cells

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a value that only grows
	class DEKAF2_PUBLIC Counter
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		explicit Counter (Cells& cells);

		void     Inc (uint64_t iCount = 1) noexcept { m_Cells.Mine(m_iSlot)->fetch_add(iCount, std::memory_order_relaxed); }
		DEKAF2_NODISCARD
		uint64_t Get () const noexcept              { return m_Cells.Sum(m_iSlot);                                          }

	private:

		Cells&      m_Cells;
		std::size_t m_iSlot;

	}; // Counter

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A value that goes up and down. Other than a counter it is one value for all
	/// threads, because it can be set: for a gauge that every request changes,
	/// like the requests in flight, take two counters and a callback with their
	/// difference.
	class Gauge
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		void    Set (int64_t iValue)     noexcept { m_iValue.store(iValue, std::memory_order_relaxed);      }
		void    Inc (int64_t iCount = 1) noexcept { m_iValue.fetch_add(iCount, std::memory_order_relaxed);  }
		void    Dec (int64_t iCount = 1) noexcept { m_iValue.fetch_sub(iCount, std::memory_order_relaxed);  }
		DEKAF2_NODISCARD
		int64_t Get () const noexcept             { return m_iValue.load(std::memory_order_relaxed);        }

	private:

		std::atomic<int64_t> m_iValue { 0 };

	}; // Gauge

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// The buckets of a histogram. Values are observed as integers, like durations
	/// in nanoseconds, and written in the unit of the output, like seconds.
	struct DEKAF2_PUBLIC Buckets
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// The upper bounds of the buckets in ascending order, at most iMaxBounds of them. A
		/// bucket takes the values up to and including its bound. Values above the last bound
		/// are counted as well.
		std::vector<uint64_t> Bounds;
		/// how many units of the observed values make one unit of the output
		uint64_t              iUnitsPerOutputUnit { 1 };

		static constexpr std::size_t iMaxBounds = 254;

		/// @return buckets for durations in nanoseconds with output in seconds, from 5 milliseconds to 10 seconds
		DEKAF2_NODISCARD
		static Buckets Durations ();

		/// @return buckets for durations in nanoseconds with output in seconds, with these bounds
		DEKAF2_NODISCARD
		static Buckets Durations (std::initializer_list<KDuration> Bounds);
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// counts of observed values in buckets, with their sum
	class DEKAF2_PUBLIC Histogram
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		Histogram (Cells& cells, const Buckets& buckets);

		/// count a value
		void Observe (uint64_t iValue) noexcept
		{
			std::size_t iBucket = 0;

			auto& Bounds = m_Buckets.Bounds;

			// a handful of bounds: a linear search is faster than a binary one
			while (iBucket < Bounds.size() && iValue > Bounds[iBucket])
			{
				++iBucket;
			}

			// the counts of the buckets and the sum are neighbours in the cell of this thread
			auto Mine = m_Cells.Mine(m_iSlot);

			Mine[iBucket].fetch_add(1, std::memory_order_relaxed);
			Mine[Bounds.size() + 1].fetch_add(iValue, std::memory_order_relaxed);
		}

		/// count a duration, in nanoseconds
		void Observe (KDuration Duration) noexcept
		{
			auto iNanoseconds = Duration.nanoseconds().count();
			Observe(iNanoseconds < 0 ? uint64_t(0) : static_cast<uint64_t>(iNanoseconds));
		}

		/// @return the count of all observed values
		DEKAF2_NODISCARD
		uint64_t GetCount () const noexcept;

		/// @return the sum of all observed values, in their unit
		DEKAF2_NODISCARD
		uint64_t GetSum () const noexcept { return m_Cells.Sum(m_iSlot + m_Buckets.Bounds.size() + 1); }

		/// @return the counts of the buckets, one per bound and a last one for the values above all bounds
		DEKAF2_NODISCARD
		std::vector<uint64_t> GetCounts () const;

		/// Estimate the value below which a share of the observed values lies, by interpolation
		/// inside of a bucket. The estimate is as exact as the buckets are narrow.
		/// @param fQuantile the share, from 0 to 1: 0.99 for the 99th percentile
		/// @return the value in the unit of the output. The last bound, if the share reaches into
		/// the values above all bounds. 0 without observations.
		DEKAF2_NODISCARD
		double Quantile (double fQuantile) const;

		DEKAF2_NODISCARD
		const Buckets& GetBuckets () const noexcept { return m_Buckets; }

	private:

		Cells&      m_Cells;
		Buckets     m_Buckets;
		// the first of the slots: one per bucket, one for the values above all bounds, one for the sum
		std::size_t m_iSlot;

	}; // Histogram

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a metric with its values, one per set of label values
	class DEKAF2_PUBLIC Family
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KMetrics;

	public:

		virtual ~Family ();

		Family (const Family&)            = delete;
		Family& operator=(const Family&)  = delete;

		DEKAF2_NODISCARD const KString&    GetName       () const noexcept { return m_sName;  }
		DEKAF2_NODISCARD const KString&    GetHelp       () const noexcept { return m_sHelp;  }
		DEKAF2_NODISCARD Type              GetType       () const noexcept { return m_Type;   }
		DEKAF2_NODISCARD const LabelNames& GetLabelNames () const noexcept { return m_Labels; }

	protected:

		Family (Cells& cells, KStringView sName, KStringView sHelp, Type type, LabelNames Labels, bool bCallback = false);

		/// a value with its label values
		template<class Metric>
		struct Child
		{
			template<class... Args>
			Child (LabelValues _Values, Args&&... args)
			: Values(std::move(_Values))
			, Value(std::forward<Args>(args)...)
			{
			}

			LabelValues Values;
			Metric      Value;
		};

		/// @return the label values in the count of the label names: missing values are empty, further ones dropped
		DEKAF2_NODISCARD
		LabelValues Normalized (const LabelValues& Values) const;

		/// find the child with these label values, or create it. The reference stays valid.
		template<class Metric, class... Args>
		Metric& FindOrCreate (std::deque<Child<Metric>>& Children, const LabelValues& Values, Args&&... args)
		{
			auto Key = Normalized(Values);

			std::lock_guard<std::mutex> Lock(m_Mutex);

			auto it = m_Index.find(Key);

			if (it != m_Index.end())
			{
				return Children[it->second].Value;
			}

			m_Index.emplace(Key, Children.size());
			Children.emplace_back(std::move(Key), std::forward<Args>(args)...);

			return Children.back().Value;
		}

		/// append the lines of the Prometheus text format for all values
		virtual void  PrintValues (KStringRef& sOut) const = 0;
		/// @return a JSON array with one object per value
		virtual KJSON JSONValues  () const = 0;

		/// append one line of the text format
		void PrintSample (KStringRef& sOut, KStringView sSuffix, const LabelValues& Values,
		                  KStringView sExtraLabel, KStringView sExtraValue, KStringView sValue) const;
		/// @return the labels as a JSON object
		DEKAF2_NODISCARD
		KJSON JSONLabels (const LabelValues& Values) const;

		mutable std::mutex                 m_Mutex;
		std::map<LabelValues, std::size_t> m_Index;
		Cells&                             m_Cells;

	private:

		KString    m_sName;
		KString    m_sHelp;
		LabelNames m_Labels;
		Type       m_Type;
		bool       m_bCallback;

	}; // Family

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC CounterFamily : public Family
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KMetrics;

	public:

		/// @return the counter for these label values, in the order of the label names
		DEKAF2_NODISCARD
		Counter& Get (const LabelValues& Values = LabelValues{}) { return FindOrCreate(m_Children, Values, m_Cells); }

	protected:

		using Family::Family;

		void  PrintValues (KStringRef& sOut) const override;
		KJSON JSONValues  () const override;

	private:

		std::deque<Child<Counter>> m_Children;

	}; // CounterFamily

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC GaugeFamily : public Family
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KMetrics;

	public:

		/// @return the gauge for these label values, in the order of the label names
		DEKAF2_NODISCARD
		Gauge& Get (const LabelValues& Values = LabelValues{}) { return FindOrCreate(m_Children, Values); }

	protected:

		using Family::Family;

		void  PrintValues (KStringRef& sOut) const override;
		KJSON JSONValues  () const override;

	private:

		std::deque<Child<Gauge>> m_Children;

	}; // GaugeFamily

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC HistogramFamily : public Family
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KMetrics;

	public:

		/// @return the histogram for these label values, in the order of the label names
		DEKAF2_NODISCARD
		Histogram& Get (const LabelValues& Values = LabelValues{}) { return FindOrCreate(m_Children, Values, m_Cells, m_Buckets); }

	protected:

		HistogramFamily (Cells& cells, KStringView sName, KStringView sHelp, Type type, LabelNames Labels, Buckets buckets);

		void  PrintValues (KStringRef& sOut) const override;
		KJSON JSONValues  () const override;

	private:

		Buckets                      m_Buckets;
		std::deque<Child<Histogram>> m_Children;

	}; // HistogramFamily

	/// @param iCells the count of cells for the values of counters and histograms, rounded up to a
	/// power of two. With 0 it is at least four times the count of hardware threads.
	explicit KMetrics (std::size_t iCells = 0);
	~KMetrics ();

	/// @return the count of cells
	DEKAF2_NODISCARD
	std::size_t GetCellCount () const noexcept { return m_Cells.size(); }

	KMetrics (const KMetrics&)            = delete;
	KMetrics& operator=(const KMetrics&)  = delete;

	/// Add a counter. A second call with the same name returns the counter of the first.
	/// @param sName the name of the metric, by convention with the suffix _total
	/// @param sHelp one line that says what the metric is
	/// @param Labels the names of its labels
	CounterFamily&   AddCounter   (KStringView sName, KStringView sHelp, LabelNames Labels = LabelNames{});

	/// Add a gauge. A second call with the same name returns the gauge of the first.
	GaugeFamily&     AddGauge     (KStringView sName, KStringView sHelp, LabelNames Labels = LabelNames{});

	/// Add a histogram. A second call with the same name returns the histogram of the first.
	/// @param sName the name of the metric, by convention with its unit as suffix, like _seconds
	/// @param sHelp one line that says what the metric is
	/// @param Labels the names of its labels
	/// @param buckets the bounds of its buckets and the unit of the output
	HistogramFamily& AddHistogram (KStringView sName, KStringView sHelp, LabelNames Labels = LabelNames{},
	                               Buckets buckets = Buckets::Durations());

	/// Add a value that is asked for whenever the metrics are written. The callback runs in the
	/// thread that writes them, and has to stay callable until RemoveCallback() or the end of
	/// the registry. It must not call into the registry. Further calls with the same name add
	/// values with other label values.
	/// @param sName the name of the metric
	/// @param sHelp one line that says what the metric is
	/// @param type Type::Counter or Type::Gauge
	/// @param callback returns the current value
	/// @param Labels the names of its labels
	/// @param Values the label values of this value
	/// @return false if the name is taken by a metric of another kind, or the label values by another callback
	bool AddCallback (KStringView sName, KStringView sHelp, Type type, Callback callback,
	                  LabelNames Labels = LabelNames{}, LabelValues Values = LabelValues{});

	/// Remove a value that AddCallback() added. After the return the callback is not called anymore.
	bool RemoveCallback (KStringView sName, const LabelValues& Values = LabelValues{});

	/// @return all metrics in the Prometheus text format, in the order they were added
	DEKAF2_NODISCARD
	KString Serialize () const;

	/// append all metrics in the Prometheus text format
	void Serialize (KStringRef& sOut) const;

	/// @return all metrics as a JSON array, histograms with estimated percentiles
	DEKAF2_NODISCARD
	KJSON ToJSON () const;

//----------
private:
//----------

	class CallbackFamily;

	template<class FamilyType, class... Args>
	FamilyType& Add (bool bCallback, KStringView sName, KStringView sHelp, Type type, LabelNames Labels, Args&&... args);

	Cells                                m_Cells;
	mutable std::mutex                   m_Mutex;
	std::vector<std::unique_ptr<Family>> m_Families;
	KUnorderedMap<KString, std::size_t>  m_Index;
	// families whose name was taken by a metric of another kind: their values are never written
	std::vector<std::unique_ptr<Family>> m_Unnamed;

}; // KMetrics

/// @}

DEKAF2_NAMESPACE_END
