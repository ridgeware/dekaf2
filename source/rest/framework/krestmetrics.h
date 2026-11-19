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

/// @file krestmetrics.h
/// the metrics that the REST server keeps of its requests

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/http/protocol/khttp_method.h>
#include <dekaf2/util/metrics/kmetrics.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup rest_framework
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The metrics of the requests of a REST server, in a KMetrics registry:
///
/// - http_requests_total, by route, method and class of the status (2xx, 4xx, ..)
/// - http_request_duration_seconds, a histogram by route and method
/// - http_request_bytes_total and http_response_bytes_total, the bodies, by route and method
/// - http_requests_started_total and http_requests_finished_total
/// - http_requests_in_flight, the requests that are started and not finished
/// - on request http_request_phase_seconds, the time of the phases of a request (rx,
///   route, parse, process, serialize, tx) as sum and count, by route, method and phase
///
/// The route is the route as it was added to the route table, not the path of a
/// request, so that the count of values stays the count of routes. Requests that
/// match no route have the route "unmatched".
///
/// Set it up with KRESTServer::Options::SetMetrics().
class DEKAF2_PUBLIC KRESTMetrics
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the values for the requests of one route with one method
	struct DEKAF2_PUBLIC Requests
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the count of requests by the first digit of their status, with all other statuses at 0
		std::array<std::reference_wrapper<KMetrics::Counter>, 6> Status;
		KMetrics::Counter&   RequestBytes;
		KMetrics::Counter&   ResponseBytes;
		KMetrics::Histogram& Duration;
		/// the time of each phase, in the order of the phases - empty if the phases are not kept
		std::vector<std::reference_wrapper<KMetrics::Histogram>> Phases;
	};

	/// the count of phases of a request
	static constexpr std::size_t iPhases = 6;

	/// @return the name of a phase, as the label of its values
	DEKAF2_NODISCARD
	static KStringViewZ PhaseName (std::size_t iPhase);

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// The values of one route for all methods. A route has one of these, and
	/// creates the values for a method at the first request with it.
	class DEKAF2_PUBLIC Route
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		Route ();
		/// the copy of a route starts without values
		Route (const Route&);
		Route (Route&&) noexcept;
		Route& operator=(const Route&);
		Route& operator=(Route&&) noexcept;
		~Route ();

		/// @return the values for requests with this method
		/// @param Metrics the metrics that create the values at the first call
		/// @param sRoute the route, for the label of the values
		/// @param Method the method of the request
		Requests& Get (KRESTMetrics& Metrics, KStringView sRoute, KHTTPMethod Method) const;

		/// call a function for the values of every method that had a request
		void ForEach (const std::function<void(KHTTPMethod, const Requests&)>& Callback) const;

	private:

		struct Slot
		{
			std::once_flag            Once;
			std::unique_ptr<Requests> Values;
			// set when Values can be read by other threads
			std::atomic<bool>         bCreated { false };
		};

		using Slots = std::array<Slot, KHTTPMethod::INVALID + 1>;

		std::unique_ptr<Slots> m_Slots;

	}; // Route

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Metrics with a registry of their own, created at the first use. A route
	/// table has them for the statistics of its routes, for a server that
	/// measures the phases of its requests but has no metrics set.
	class DEKAF2_PUBLIC Own
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		Own ();
		Own (Own&&) noexcept;
		Own& operator=(Own&&) noexcept;
		~Own ();

		/// @return the metrics, with the phases of the requests
		KRESTMetrics& Get () const;

	private:

		struct State
		{
			std::once_flag                Once;
			std::unique_ptr<KRESTMetrics> Metrics;
		};

		std::unique_ptr<State> m_State;

	}; // Own

	/// @param Metrics the registry that takes the metrics, or null for a new one
	/// @param bPhases keep the time of the phases of a request, too
	explicit KRESTMetrics (std::shared_ptr<KMetrics> Metrics, bool bPhases = false);
	~KRESTMetrics ();

	KRESTMetrics (const KRESTMetrics&)            = delete;
	KRESTMetrics& operator=(const KRESTMetrics&)  = delete;

	/// @return the registry
	DEKAF2_NODISCARD
	KMetrics& GetMetrics () noexcept { return *m_Metrics; }

	/// @return true if the time of the phases of a request is kept
	DEKAF2_NODISCARD
	bool HasPhases () const noexcept { return m_bPhases; }

	/// create the values for the requests of a route with a method
	DEKAF2_NODISCARD
	std::unique_ptr<Requests> CreateRequests (KStringView sRoute, KStringView sMethod);

	/// @return the values for requests that matched no route
	DEKAF2_NODISCARD
	Requests& Unmatched (KHTTPMethod Method) { return m_Unmatched.Get(*this, "unmatched", Method); }

	/// count a request that the server begins to answer
	void Started  () noexcept { m_Started.Inc();  }
	/// count a request that the server has answered
	void Finished () noexcept { m_Finished.Inc(); }

//----------
private:
//----------

	std::shared_ptr<KMetrics>  m_Metrics;
	KMetrics::CounterFamily&   m_Requests;
	KMetrics::HistogramFamily& m_Durations;
	KMetrics::CounterFamily&   m_RequestBytes;
	KMetrics::CounterFamily&   m_ResponseBytes;
	KMetrics::Counter&         m_Started;
	KMetrics::Counter&         m_Finished;
	Route                      m_Unmatched;
	bool                       m_bPhases;

}; // KRESTMetrics

/// @}

DEKAF2_NAMESPACE_END
