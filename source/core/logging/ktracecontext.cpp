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

#include <dekaf2/core/logging/ktracecontext.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/crypto/encoding/khex.h>
#include <dekaf2/crypto/random/krandom.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

// 00-<trace id>-<parent id>-<flags>
constexpr std::size_t TraceParentSize      { 55 };
constexpr std::size_t TraceIDBytes         { 16 };
constexpr std::size_t SpanIDBytes          {  8 };
constexpr std::size_t MaxTraceStateMembers { 32 };

thread_local KTraceContext s_CurrentContext;

//-----------------------------------------------------------------------------
// the ids and flags are lowercase hex digits only
bool IsLowerHex(KStringView sHex)
//-----------------------------------------------------------------------------
{
	for (auto ch : sHex)
	{
		if (!KASCII::kIsDigit(ch) && (ch < 'a' || ch > 'f'))
		{
			return false;
		}
	}

	return true;

} // IsLowerHex

//-----------------------------------------------------------------------------
// an id of zeros only is not valid
bool IsAllZero(KStringView sHex)
//-----------------------------------------------------------------------------
{
	return sHex.find_first_not_of('0') == KStringView::npos;

} // IsAllZero

//-----------------------------------------------------------------------------
KString RandomID(std::size_t iBytes)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		auto sID = kHex(kGetRandom(iBytes));

		if (!IsAllZero(sID))
		{
			return sID;
		}
	}

} // RandomID

//-----------------------------------------------------------------------------
// printable ASCII, and at most 32 members in a comma separated list
bool IsValidTraceState(KStringView sTraceState)
//-----------------------------------------------------------------------------
{
	std::size_t iMembers  { 0 };
	bool        bInMember { false };

	for (auto ch : sTraceState)
	{
		auto iChar = static_cast<unsigned char>(ch);

		if (ch == ',')
		{
			bInMember = false;
		}
		else if ((iChar < 0x20 || iChar > 0x7e) && ch != '\t')
		{
			return false;
		}
		else if (!bInMember && ch != ' ' && ch != '\t')
		{
			bInMember = true;

			if (++iMembers > MaxTraceStateMembers)
			{
				return false;
			}
		}
	}

	return true;

} // IsValidTraceState

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KTraceContext KTraceContext::Start()
//-----------------------------------------------------------------------------
{
	KTraceContext Context;

	Context.m_sTraceID = RandomID(TraceIDBytes);
	Context.m_sSpanID  = RandomID(SpanIDBytes);
	Context.m_iFlags   = Sampled;

	return Context;

} // Start

//-----------------------------------------------------------------------------
bool KTraceContext::Parse(KStringView sTraceParent, KStringView sTraceState)
//-----------------------------------------------------------------------------
{
	clear();

	if (sTraceParent.size() < TraceParentSize)
	{
		return false;
	}

	auto sVersion = sTraceParent.substr(0, 2);

	if (!IsLowerHex(sVersion) || sVersion == "ff")
	{
		return false;
	}

	if (sVersion == "00")
	{
		if (sTraceParent.size() != TraceParentSize)
		{
			return false;
		}
	}
	else if (sTraceParent.size() > TraceParentSize && sTraceParent[TraceParentSize] != '-')
	{
		// a later version may append fields, which are not read
		return false;
	}

	if (sTraceParent[2] != '-' || sTraceParent[35] != '-' || sTraceParent[52] != '-')
	{
		return false;
	}

	auto sTraceID  = sTraceParent.substr( 3, TraceIDBytes * 2);
	auto sParentID = sTraceParent.substr(36, SpanIDBytes  * 2);
	auto sFlags    = sTraceParent.substr(53, 2);

	if (!IsLowerHex(sTraceID)  || IsAllZero(sTraceID)  ||
	    !IsLowerHex(sParentID) || IsAllZero(sParentID) ||
	    !IsLowerHex(sFlags))
	{
		return false;
	}

	m_sTraceID  = sTraceID;
	m_sParentID = sParentID;
	m_sSpanID   = RandomID(SpanIDBytes);
	m_iFlags    = static_cast<uint8_t>(kFromHexChar(sFlags[0]) << 4 | kFromHexChar(sFlags[1]));

	if (IsValidTraceState(sTraceState))
	{
		m_sTraceState = sTraceState;
	}
	else
	{
		kDebug(2, "dropping a tracestate that is not valid");
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
KString KTraceContext::Serialize() const
//-----------------------------------------------------------------------------
{
	if (empty())
	{
		return KString{};
	}

	return kFormat("00-{}-{}-{:02x}", m_sTraceID, m_sSpanID, m_iFlags);

} // Serialize

//-----------------------------------------------------------------------------
void KTraceContext::clear()
//-----------------------------------------------------------------------------
{
	m_sTraceID.clear();
	m_sSpanID.clear();
	m_sParentID.clear();
	m_sTraceState.clear();
	m_iFlags = 0;

} // clear

//-----------------------------------------------------------------------------
const KTraceContext& KTraceContext::Current()
//-----------------------------------------------------------------------------
{
	return s_CurrentContext;

} // Current

//-----------------------------------------------------------------------------
KTraceContext::Scope::Scope(KTraceContext Context)
//-----------------------------------------------------------------------------
: m_Previous(std::move(s_CurrentContext))
{
	s_CurrentContext = std::move(Context);

} // Scope

//-----------------------------------------------------------------------------
KTraceContext::Scope::~Scope()
//-----------------------------------------------------------------------------
{
	s_CurrentContext = std::move(m_Previous);

} // ~Scope

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr uint8_t KTraceContext::Sampled;
#endif

DEKAF2_NAMESPACE_END
