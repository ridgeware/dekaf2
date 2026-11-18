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


#include <dekaf2/util/metrics/kmetrics.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <algorithm>
#include <cmath>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
KStringViewZ TypeName(KMetrics::Type type)
//-----------------------------------------------------------------------------
{
	switch (type)
	{
		case KMetrics::Type::Counter:   return "counter";
		case KMetrics::Type::Gauge:     return "gauge";
		case KMetrics::Type::Histogram: return "histogram";
	}

	return "";
}

//-----------------------------------------------------------------------------
/// a floating point number as the Prometheus text format writes it
KString FormatNumber(double fValue)
//-----------------------------------------------------------------------------
{
	if (std::isnan(fValue))
	{
		return "NaN";
	}

	if (std::isinf(fValue))
	{
		return (fValue > 0) ? "+Inf" : "-Inf";
	}

	// the shortest form that reads back as the same number
	return kFormat("{}", fValue);
}

//-----------------------------------------------------------------------------
/// append text with the escapes of the Prometheus text format: a help text
/// escapes the backslash and the line feed, a label value the quote as well
void AppendEscaped(KStringRef& sOut, KStringView sText, bool bIsLabelValue)
//-----------------------------------------------------------------------------
{
	for (auto ch : sText)
	{
		switch (ch)
		{
			case '\\':
				sOut += "\\\\";
				break;

			case '\n':
				sOut += "\\n";
				break;

			case '"':
				if (bIsLabelValue)
				{
					sOut += "\\\"";
				}
				else
				{
					sOut += ch;
				}
				break;

			default:
				sOut += ch;
				break;
		}
	}

} // AppendEscaped

//-----------------------------------------------------------------------------
/// @return a value in the unit of the output
double Scaled(uint64_t iValue, const KMetrics::Buckets& buckets)
//-----------------------------------------------------------------------------
{
	// a division, not a multiplication with a factor: 5000000 / 1000000000 is the
	// double nearest to 0.005, and is written as 0.005
	return static_cast<double>(iValue) / static_cast<double>(buckets.iUnitsPerOutputUnit ? buckets.iUnitsPerOutputUnit : 1);
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMetrics::Buckets KMetrics::Buckets::Durations()
//-----------------------------------------------------------------------------
{
	return Durations(
	{
		chrono::milliseconds(5),
		chrono::milliseconds(10),
		chrono::milliseconds(25),
		chrono::milliseconds(50),
		chrono::milliseconds(100),
		chrono::milliseconds(250),
		chrono::milliseconds(500),
		chrono::seconds(1),
		chrono::milliseconds(2500),
		chrono::seconds(5),
		chrono::seconds(10)
	});
}

//-----------------------------------------------------------------------------
KMetrics::Buckets KMetrics::Buckets::Durations(std::initializer_list<KDuration> Bounds)
//-----------------------------------------------------------------------------
{
	Buckets buckets;

	buckets.iUnitsPerOutputUnit = 1000000000;
	buckets.Bounds.reserve(Bounds.size());

	for (auto& Bound : Bounds)
	{
		auto iNanoseconds = Bound.nanoseconds().count();
		buckets.Bounds.push_back(iNanoseconds < 0 ? uint64_t(0) : static_cast<uint64_t>(iNanoseconds));
	}

	return buckets;
}

//-----------------------------------------------------------------------------
KMetrics::Histogram::Histogram(const Buckets& buckets)
//-----------------------------------------------------------------------------
: m_Buckets(buckets)
, m_Counts(buckets.Bounds.size() + 1)
{
	// the search for the bucket of a value wants the bounds in ascending order, each once
	auto& Bounds = m_Buckets.Bounds;

	std::sort(Bounds.begin(), Bounds.end());
	Bounds.erase(std::unique(Bounds.begin(), Bounds.end()), Bounds.end());
}

//-----------------------------------------------------------------------------
std::vector<uint64_t> KMetrics::Histogram::GetCounts() const
//-----------------------------------------------------------------------------
{
	std::vector<uint64_t> Counts;

	// one count per bound, and the one for the values above all bounds
	Counts.reserve(m_Buckets.Bounds.size() + 1);

	for (std::size_t iBucket = 0; iBucket <= m_Buckets.Bounds.size(); ++iBucket)
	{
		Counts.push_back(m_Counts[iBucket].load(std::memory_order_relaxed));
	}

	return Counts;
}

//-----------------------------------------------------------------------------
uint64_t KMetrics::Histogram::GetCount() const noexcept
//-----------------------------------------------------------------------------
{
	uint64_t iCount = 0;

	for (std::size_t iBucket = 0; iBucket <= m_Buckets.Bounds.size(); ++iBucket)
	{
		iCount += m_Counts[iBucket].load(std::memory_order_relaxed);
	}

	return iCount;
}

//-----------------------------------------------------------------------------
double KMetrics::Histogram::Quantile(double fQuantile) const
//-----------------------------------------------------------------------------
{
	auto     Counts = GetCounts();
	auto&    Bounds = m_Buckets.Bounds;
	uint64_t iTotal = 0;

	for (auto iCount : Counts)
	{
		iTotal += iCount;
	}

	if (iTotal == 0 || Bounds.empty())
	{
		return 0;
	}

	fQuantile = std::min(1.0, std::max(0.0, fQuantile));

	// the rank of the value that is asked for, among all values in ascending order
	auto     fRank  = fQuantile * static_cast<double>(iTotal);
	uint64_t iBelow = 0;

	for (std::size_t iBucket = 0; iBucket < Bounds.size(); ++iBucket)
	{
		auto iCount = Counts[iBucket];

		if (iCount > 0 && static_cast<double>(iBelow + iCount) >= fRank)
		{
			// the values of a bucket are taken as spread evenly from its lower to its upper bound
			auto fLower = (iBucket == 0) ? 0.0 : static_cast<double>(Bounds[iBucket - 1]);
			auto fUpper = static_cast<double>(Bounds[iBucket]);
			auto fShare = (fRank - static_cast<double>(iBelow)) / static_cast<double>(iCount);
			auto fValue = fLower + (fUpper - fLower) * std::max(0.0, fShare);

			return fValue / static_cast<double>(m_Buckets.iUnitsPerOutputUnit ? m_Buckets.iUnitsPerOutputUnit : 1);
		}

		iBelow += iCount;
	}

	// among the values above all bounds, of which only the count is known
	return Scaled(Bounds.back(), m_Buckets);

} // Quantile

//-----------------------------------------------------------------------------
KMetrics::Family::Family(KStringView sName, KStringView sHelp, Type type, LabelNames Labels, bool bCallback)
//-----------------------------------------------------------------------------
: m_sName(sName)
, m_sHelp(sHelp)
, m_Labels(std::move(Labels))
, m_Type(type)
, m_bCallback(bCallback)
{
}

//-----------------------------------------------------------------------------
KMetrics::Family::~Family() = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KMetrics::LabelValues KMetrics::Family::Normalized(const LabelValues& Values) const
//-----------------------------------------------------------------------------
{
	auto Normalized = Values;
	Normalized.resize(m_Labels.size());
	return Normalized;
}

//-----------------------------------------------------------------------------
void KMetrics::Family::PrintSample(KStringRef& sOut, KStringView sSuffix, const LabelValues& Values,
                                   KStringView sExtraLabel, KStringView sExtraValue, KStringView sValue) const
//-----------------------------------------------------------------------------
{
	sOut += m_sName;
	sOut += sSuffix;

	if (!m_Labels.empty() || !sExtraLabel.empty())
	{
		char chSeparator = '{';

		for (std::size_t iLabel = 0; iLabel < m_Labels.size() && iLabel < Values.size(); ++iLabel)
		{
			sOut += chSeparator;
			sOut += m_Labels[iLabel];
			sOut += "=\"";
			AppendEscaped(sOut, Values[iLabel], true);
			sOut += '"';
			chSeparator = ',';
		}

		if (!sExtraLabel.empty())
		{
			sOut += chSeparator;
			sOut += sExtraLabel;
			sOut += "=\"";
			AppendEscaped(sOut, sExtraValue, true);
			sOut += '"';
		}

		sOut += '}';
	}

	sOut += ' ';
	sOut += sValue;
	sOut += '\n';

} // PrintSample

//-----------------------------------------------------------------------------
KJSON KMetrics::Family::JSONLabels(const LabelValues& Values) const
//-----------------------------------------------------------------------------
{
	KJSON jLabels = KJSON::object();

	for (std::size_t iLabel = 0; iLabel < m_Labels.size() && iLabel < Values.size(); ++iLabel)
	{
		jLabels[m_Labels[iLabel]] = Values[iLabel];
	}

	return jLabels;
}

//-----------------------------------------------------------------------------
void KMetrics::CounterFamily::PrintValues(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		PrintSample(sOut, "", Child.Values, "", "", kFormat("{}", Child.Value.Get()));
	}
}

//-----------------------------------------------------------------------------
KJSON KMetrics::CounterFamily::JSONValues() const
//-----------------------------------------------------------------------------
{
	KJSON jValues = KJSON::array();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		jValues.push_back(KJSON
		{
			{ "labels", JSONLabels(Child.Values) },
			{ "value" , Child.Value.Get()        }
		});
	}

	return jValues;
}

//-----------------------------------------------------------------------------
void KMetrics::GaugeFamily::PrintValues(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		PrintSample(sOut, "", Child.Values, "", "", kFormat("{}", Child.Value.Get()));
	}
}

//-----------------------------------------------------------------------------
KJSON KMetrics::GaugeFamily::JSONValues() const
//-----------------------------------------------------------------------------
{
	KJSON jValues = KJSON::array();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		jValues.push_back(KJSON
		{
			{ "labels", JSONLabels(Child.Values) },
			{ "value" , Child.Value.Get()        }
		});
	}

	return jValues;
}

//-----------------------------------------------------------------------------
KMetrics::HistogramFamily::HistogramFamily(KStringView sName, KStringView sHelp, Type type, LabelNames Labels, Buckets buckets)
//-----------------------------------------------------------------------------
: Family(sName, sHelp, type, std::move(Labels))
, m_Buckets(std::move(buckets))
{
}

//-----------------------------------------------------------------------------
void KMetrics::HistogramFamily::PrintValues(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		auto&    Buckets = Child.Value.GetBuckets();
		auto     Counts  = Child.Value.GetCounts();
		auto     iSum    = Child.Value.GetSum();
		uint64_t iCount  = 0;

		// the format has cumulative counts: each bucket with all the values up to its bound
		for (std::size_t iBucket = 0; iBucket < Buckets.Bounds.size(); ++iBucket)
		{
			iCount += Counts[iBucket];
			PrintSample(sOut, "_bucket", Child.Values, "le", FormatNumber(Scaled(Buckets.Bounds[iBucket], Buckets)), kFormat("{}", iCount));
		}

		iCount += Counts.back();

		PrintSample(sOut, "_bucket", Child.Values, "le", "+Inf", kFormat("{}", iCount));
		PrintSample(sOut, "_sum"   , Child.Values, "", "", FormatNumber(Scaled(iSum, Buckets)));
		PrintSample(sOut, "_count" , Child.Values, "", "", kFormat("{}", iCount));
	}

} // PrintValues

//-----------------------------------------------------------------------------
KJSON KMetrics::HistogramFamily::JSONValues() const
//-----------------------------------------------------------------------------
{
	KJSON jValues = KJSON::array();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Child : m_Children)
	{
		auto&    Buckets  = Child.Value.GetBuckets();
		auto     Counts   = Child.Value.GetCounts();
		KJSON    jBuckets = KJSON::array();
		uint64_t iCount   = 0;

		for (std::size_t iBucket = 0; iBucket < Buckets.Bounds.size(); ++iBucket)
		{
			iCount += Counts[iBucket];

			jBuckets.push_back(KJSON
			{
				{ "le"   , Scaled(Buckets.Bounds[iBucket], Buckets) },
				{ "count", iCount                                   }
			});
		}

		iCount += Counts.back();

		jValues.push_back(KJSON
		{
			{ "labels" , JSONLabels(Child.Values)                  },
			{ "count"  , iCount                                    },
			{ "sum"    , Scaled(Child.Value.GetSum(), Buckets)     },
			{ "p50"    , Child.Value.Quantile(0.50)                },
			{ "p90"    , Child.Value.Quantile(0.90)                },
			{ "p99"    , Child.Value.Quantile(0.99)                },
			{ "buckets", std::move(jBuckets)                       }
		});
	}

	return jValues;

} // JSONValues

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// values that are asked for when the metrics are written
class KMetrics::CallbackFamily : public KMetrics::Family
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	CallbackFamily(KStringView sName, KStringView sHelp, Type type, LabelNames Labels)
	: Family(sName, sHelp, type, std::move(Labels), true)
	{
	}

	//-----------------------------------------------------------------------------
	bool Add(const LabelValues& Values, Callback callback)
	//-----------------------------------------------------------------------------
	{
		auto Key = Normalized(Values);

		std::lock_guard<std::mutex> Lock(m_Mutex);

		for (auto& Child : m_Children)
		{
			if (Child.Values == Key)
			{
				return false;
			}
		}

		m_Children.emplace_back(std::move(Key), std::move(callback));

		return true;
	}

	//-----------------------------------------------------------------------------
	bool Remove(const LabelValues& Values)
	//-----------------------------------------------------------------------------
	{
		auto Key = Normalized(Values);

		// the output calls the callbacks under this lock: after the return none of them runs
		std::lock_guard<std::mutex> Lock(m_Mutex);

		for (auto it = m_Children.begin(); it != m_Children.end(); ++it)
		{
			if (it->Values == Key)
			{
				m_Children.erase(it);
				return true;
			}
		}

		return false;
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	void PrintValues(KStringRef& sOut) const override
	//-----------------------------------------------------------------------------
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		for (auto& Child : m_Children)
		{
			PrintSample(sOut, "", Child.Values, "", "", FormatNumber(Child.Value()));
		}
	}

	//-----------------------------------------------------------------------------
	KJSON JSONValues() const override
	//-----------------------------------------------------------------------------
	{
		KJSON jValues = KJSON::array();

		std::lock_guard<std::mutex> Lock(m_Mutex);

		for (auto& Child : m_Children)
		{
			auto fValue = Child.Value();

			jValues.push_back(KJSON
			{
				{ "labels", JSONLabels(Child.Values)           },
				// JSON has no number for infinity and NaN
				{ "value" , std::isfinite(fValue) ? fValue : 0 }
			});
		}

		return jValues;
	}

//----------
private:
//----------

	std::vector<Child<Callback>> m_Children;

}; // CallbackFamily

//-----------------------------------------------------------------------------
KMetrics::KMetrics() = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KMetrics::~KMetrics() = default;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template<class FamilyType, class... Args>
FamilyType& KMetrics::Add(bool bCallback, KStringView sName, KStringView sHelp, Type type, LabelNames Labels, Args&&... args)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto it = m_Index.find(sName);

	if (it != m_Index.end())
	{
		auto& Existing = *m_Families[it->second];

		if (Existing.m_Type == type && Existing.m_bCallback == bCallback)
		{
			// the same metric again
			return static_cast<FamilyType&>(Existing);
		}

		// The name is taken by a metric of another kind. The caller gets a metric that
		// works, but that the output leaves out.
		kDebug(1, "the metric '{}' exists with another type, the {} of this name is not written", sName, TypeName(type));

		m_Unnamed.push_back(std::unique_ptr<Family>(new FamilyType(sName, sHelp, type, std::move(Labels), std::forward<Args>(args)...)));

		return static_cast<FamilyType&>(*m_Unnamed.back());
	}

	m_Index.emplace(sName, m_Families.size());
	m_Families.push_back(std::unique_ptr<Family>(new FamilyType(sName, sHelp, type, std::move(Labels), std::forward<Args>(args)...)));

	return static_cast<FamilyType&>(*m_Families.back());

} // Add

//-----------------------------------------------------------------------------
KMetrics::CounterFamily& KMetrics::AddCounter(KStringView sName, KStringView sHelp, LabelNames Labels)
//-----------------------------------------------------------------------------
{
	return Add<CounterFamily>(false, sName, sHelp, Type::Counter, std::move(Labels));
}

//-----------------------------------------------------------------------------
KMetrics::GaugeFamily& KMetrics::AddGauge(KStringView sName, KStringView sHelp, LabelNames Labels)
//-----------------------------------------------------------------------------
{
	return Add<GaugeFamily>(false, sName, sHelp, Type::Gauge, std::move(Labels));
}

//-----------------------------------------------------------------------------
KMetrics::HistogramFamily& KMetrics::AddHistogram(KStringView sName, KStringView sHelp, LabelNames Labels, Buckets buckets)
//-----------------------------------------------------------------------------
{
	return Add<HistogramFamily>(false, sName, sHelp, Type::Histogram, std::move(Labels), std::move(buckets));
}

//-----------------------------------------------------------------------------
bool KMetrics::AddCallback(KStringView sName, KStringView sHelp, Type type, Callback callback, LabelNames Labels, LabelValues Values)
//-----------------------------------------------------------------------------
{
	if (!callback || type == Type::Histogram)
	{
		return false;
	}

	auto& Callbacks = Add<CallbackFamily>(true, sName, sHelp, type, std::move(Labels));

	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		if (m_Index.find(sName) == m_Index.end() || m_Families[m_Index.find(sName)->second].get() != &Callbacks)
		{
			// the name is taken by a metric of another kind
			return false;
		}
	}

	return Callbacks.Add(Values, std::move(callback));

} // AddCallback

//-----------------------------------------------------------------------------
bool KMetrics::RemoveCallback(KStringView sName, const LabelValues& Values)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> Lock(m_Mutex);

	auto it = m_Index.find(sName);

	if (it == m_Index.end() || !m_Families[it->second]->m_bCallback)
	{
		return false;
	}

	auto& Callbacks = static_cast<CallbackFamily&>(*m_Families[it->second]);

	// families are never removed: the reference stays valid without the lock
	Lock.unlock();

	return Callbacks.Remove(Values);

} // RemoveCallback

//-----------------------------------------------------------------------------
void KMetrics::Serialize(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Family : m_Families)
	{
		sOut += "# HELP ";
		sOut += Family->m_sName;
		sOut += ' ';
		AppendEscaped(sOut, Family->m_sHelp, false);
		sOut += "\n# TYPE ";
		sOut += Family->m_sName;
		sOut += ' ';
		sOut += TypeName(Family->m_Type);
		sOut += '\n';

		Family->PrintValues(sOut);
	}

} // Serialize

//-----------------------------------------------------------------------------
KString KMetrics::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut);
	return sOut;
}

//-----------------------------------------------------------------------------
KJSON KMetrics::ToJSON() const
//-----------------------------------------------------------------------------
{
	KJSON jMetrics = KJSON::array();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	for (auto& Family : m_Families)
	{
		jMetrics.push_back(KJSON
		{
			{ "name"  , Family->m_sName            },
			{ "type"  , TypeName(Family->m_Type)   },
			{ "help"  , Family->m_sHelp            },
			{ "values", Family->JSONValues()       }
		});
	}

	return jMetrics;

} // ToJSON

DEKAF2_NAMESPACE_END
