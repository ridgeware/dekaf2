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


#include <dekaf2/rest/framework/krestmetrics.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringViewZ sInFlight { "http_requests_in_flight"    };
constexpr KStringViewZ sPhases   { "http_request_phase_seconds" };

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KRESTMetrics::Route::Route()
//-----------------------------------------------------------------------------
: m_Slots(std::make_unique<Slots>())
{
}

//-----------------------------------------------------------------------------
KRESTMetrics::Route::Route(const Route&)
//-----------------------------------------------------------------------------
: Route()
{
}

//-----------------------------------------------------------------------------
KRESTMetrics::Route::Route(Route&&) noexcept = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics::Route& KRESTMetrics::Route::operator=(const Route& other)
//-----------------------------------------------------------------------------
{
	if (this != &other)
	{
		m_Slots = std::make_unique<Slots>();
	}

	return *this;
}

//-----------------------------------------------------------------------------
KRESTMetrics::Route& KRESTMetrics::Route::operator=(Route&&) noexcept = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics::Route::~Route() = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics::Requests& KRESTMetrics::Route::Get(KRESTMetrics& Metrics, KStringView sRoute, KHTTPMethod Method) const
//-----------------------------------------------------------------------------
{
	auto  iMethod = static_cast<std::size_t>(static_cast<KHTTPMethod::Method>(Method));
	auto& Slot    = (*m_Slots)[iMethod < m_Slots->size() ? iMethod : static_cast<std::size_t>(KHTTPMethod::INVALID)];

	// only the first request with a method takes the lock of the registry
	std::call_once(Slot.Once, [&]()
	{
		Slot.Values = Metrics.CreateRequests(sRoute, Method == KHTTPMethod::INVALID ? KStringViewZ("invalid") : Method.Serialize());
		Slot.bCreated.store(true, std::memory_order_release);
	});

	return *Slot.Values;

} // Get

//-----------------------------------------------------------------------------
void KRESTMetrics::Route::ForEach(const std::function<void(KHTTPMethod, const Requests&)>& Callback) const
//-----------------------------------------------------------------------------
{
	if (!m_Slots || !Callback)
	{
		return;
	}

	for (std::size_t iMethod = 0; iMethod < m_Slots->size(); ++iMethod)
	{
		auto& Slot = (*m_Slots)[iMethod];

		if (Slot.bCreated.load(std::memory_order_acquire))
		{
			Callback(static_cast<KHTTPMethod::Method>(iMethod), *Slot.Values);
		}
	}

} // ForEach

//-----------------------------------------------------------------------------
KRESTMetrics::Own::Own()
//-----------------------------------------------------------------------------
: m_State(std::make_unique<State>())
{
}

//-----------------------------------------------------------------------------
KRESTMetrics::Own::Own(Own&&) noexcept = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics::Own& KRESTMetrics::Own::operator=(Own&&) noexcept = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics::Own::~Own() = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KRESTMetrics& KRESTMetrics::Own::Get() const
//-----------------------------------------------------------------------------
{
	std::call_once(m_State->Once, [this]()
	{
		m_State->Metrics = std::make_unique<KRESTMetrics>(std::make_shared<KMetrics>(), true);
	});

	return *m_State->Metrics;

} // Get

//-----------------------------------------------------------------------------
KStringViewZ KRESTMetrics::PhaseName(std::size_t iPhase)
//-----------------------------------------------------------------------------
{
	// in the order of KRESTServer::Timer
	static constexpr std::array<KStringViewZ, iPhases> s_Names
	{{
		"rx", "route", "parse", "process", "serialize", "tx"
	}};

	return (iPhase < s_Names.size()) ? s_Names[iPhase] : KStringViewZ("");

} // PhaseName

//-----------------------------------------------------------------------------
KRESTMetrics::KRESTMetrics(std::shared_ptr<KMetrics> Metrics, bool bPhases)
//-----------------------------------------------------------------------------
: m_Metrics      (Metrics ? std::move(Metrics) : std::make_shared<KMetrics>())
, m_Requests     (m_Metrics->AddCounter  ("http_requests_total"          , "count of answered requests by the class of their status", { "route", "method", "status" }))
, m_Durations    (m_Metrics->AddHistogram("http_request_duration_seconds", "time from the received request header to the sent response, in seconds", { "route", "method" }))
, m_RequestBytes (m_Metrics->AddCounter  ("http_request_bytes_total"     , "size of the bodies of the requests, in bytes" , { "route", "method" }))
, m_ResponseBytes(m_Metrics->AddCounter  ("http_response_bytes_total"    , "size of the bodies of the responses, in bytes", { "route", "method" }))
, m_Started      (m_Metrics->AddCounter  ("http_requests_started_total"  , "count of requests whose header was received").Get())
, m_Finished     (m_Metrics->AddCounter  ("http_requests_finished_total" , "count of requests that were answered").Get())
, m_bPhases      (bPhases)
{
	// A gauge that every request sets would be one value for all threads. The two
	// counters are not, and their difference is the same number.
	m_Metrics->AddCallback(sInFlight, "requests that are started and not finished", KMetrics::Type::Gauge, [this]()
	{
		auto iStarted  = m_Started.Get();
		auto iFinished = m_Finished.Get();

		// the two counters are not read at the same instant
		return (iStarted > iFinished) ? static_cast<double>(iStarted - iFinished) : 0.0;
	});

} // ctor

//-----------------------------------------------------------------------------
KRESTMetrics::~KRESTMetrics()
//-----------------------------------------------------------------------------
{
	// the callback points to this object
	m_Metrics->RemoveCallback(sInFlight);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KRESTMetrics::Requests> KRESTMetrics::CreateRequests(KStringView sRoute, KStringView sMethod)
//-----------------------------------------------------------------------------
{
	std::vector<std::reference_wrapper<KMetrics::Histogram>> Phases;

	if (m_bPhases)
	{
		// sum and count per phase, without buckets
		KMetrics::Buckets NoBounds;
		NoBounds.iUnitsPerOutputUnit = 1000000000;

		auto& Family = m_Metrics->AddHistogram(sPhases, "time of the phases of the requests, in seconds", { "route", "method", "phase" }, NoBounds);

		Phases.reserve(iPhases);

		for (std::size_t iPhase = 0; iPhase < iPhases; ++iPhase)
		{
			Phases.push_back(Family.Get({ sRoute, sMethod, PhaseName(iPhase) }));
		}
	}

	return std::unique_ptr<Requests>(new Requests
	{
		{{
			m_Requests.Get({ sRoute, sMethod, "other" }),
			m_Requests.Get({ sRoute, sMethod, "1xx"   }),
			m_Requests.Get({ sRoute, sMethod, "2xx"   }),
			m_Requests.Get({ sRoute, sMethod, "3xx"   }),
			m_Requests.Get({ sRoute, sMethod, "4xx"   }),
			m_Requests.Get({ sRoute, sMethod, "5xx"   })
		}},
		m_RequestBytes .Get({ sRoute, sMethod }),
		m_ResponseBytes.Get({ sRoute, sMethod }),
		m_Durations    .Get({ sRoute, sMethod }),
		std::move(Phases)
	});

} // CreateRequests

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::size_t KRESTMetrics::iPhases;
#endif

DEKAF2_NAMESPACE_END
