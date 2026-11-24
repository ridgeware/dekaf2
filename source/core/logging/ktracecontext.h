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

/// @file ktracecontext.h
/// the trace context of a request (W3C Trace Context): a trace id that a request
/// carries through the services that serve it, and the context of the current thread

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstdint>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_logging
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The trace context of a request, after the W3C Trace Context: a trace id that
/// stays the same through all services that serve one request, the span id of the
/// own part of the work, and the span id of the caller (the parent id). It travels
/// in the HTTP header traceparent, as 00-<trace id>-<span id>-<flags>, and the
/// header tracestate carries additional data of tracing systems.
///
/// The context of the current thread (Current(), set by a Scope) is written into
/// the log lines of KLog, and Bind() carries it into work that runs in another
/// thread.
///
/// @code
/// KTraceContext Context;
///
/// if (!Context.Parse(Headers.Get(KHTTPHeader::TRACEPARENT), Headers.Get(KHTTPHeader::TRACESTATE)))
/// {
///     Context = KTraceContext::Start();
/// }
///
/// KTraceContext::Scope Trace(std::move(Context));
///
/// // all log lines of this thread carry the trace id now
/// Pool.Post(KTraceContext::Bind([]() { kDebug(1, "this one, too"); }));
/// @endcode
class DEKAF2_PUBLIC KTraceContext
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the flag "sampled" of the trace flags: the caller may have recorded the trace
	static constexpr uint8_t Sampled { 0x01 };

	//-----------------------------------------------------------------------------
	KTraceContext() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return a new trace: random trace id and span id, no parent id, and the flag Sampled
	DEKAF2_NODISCARD
	static KTraceContext Start();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Continue the trace of a caller: the trace id and the flags stay, the span id of
	/// the caller becomes the parent id, and the own span id is new.
	/// @param sTraceParent the value of the header traceparent
	/// @param sTraceState the value of the header tracestate - it is dropped if it is not
	/// valid, and does not make the trace invalid
	/// @return false if sTraceParent is not valid, the context is empty then
	bool Parse(KStringView sTraceParent, KStringView sTraceState = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the value of the header traceparent for a request to another service:
	/// version 00, the trace id, the own span id, and the flags - empty for an empty context
	DEKAF2_NODISCARD
	KString Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the trace id, 32 lowercase hex digits, empty for an empty context
	DEKAF2_NODISCARD
	const KString& GetTraceID() const
	//-----------------------------------------------------------------------------
	{
		return m_sTraceID;
	}

	//-----------------------------------------------------------------------------
	/// @return the own span id, 16 lowercase hex digits, empty for an empty context
	DEKAF2_NODISCARD
	const KString& GetSpanID() const
	//-----------------------------------------------------------------------------
	{
		return m_sSpanID;
	}

	//-----------------------------------------------------------------------------
	/// @return the span id of the caller, 16 lowercase hex digits, empty for a new trace
	DEKAF2_NODISCARD
	const KString& GetParentID() const
	//-----------------------------------------------------------------------------
	{
		return m_sParentID;
	}

	//-----------------------------------------------------------------------------
	/// @return the trace flags
	DEKAF2_NODISCARD
	uint8_t GetFlags() const
	//-----------------------------------------------------------------------------
	{
		return m_iFlags;
	}

	//-----------------------------------------------------------------------------
	/// @return the value of the header tracestate, as it arrived, or empty
	DEKAF2_NODISCARD
	const KString& GetTraceState() const
	//-----------------------------------------------------------------------------
	{
		return m_sTraceState;
	}

	//-----------------------------------------------------------------------------
	/// @return true if the context has no trace
	DEKAF2_NODISCARD
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_sTraceID.empty();
	}

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the context of the current thread, an empty one if none is set. The
	/// reference changes its content with the next Scope of the thread.
	DEKAF2_NODISCARD
	static const KTraceContext& Current();
	//-----------------------------------------------------------------------------

	/// sets a context as the context of the current thread, see below
	class Scope;

	//-----------------------------------------------------------------------------
	/// @param Function a callable that shall run in another thread
	/// @return a callable that runs Function with the context that the current thread
	/// has now, and with the same arguments
	template<class Callable>
	DEKAF2_NODISCARD
	static auto Bind(Callable Function);
	//-----------------------------------------------------------------------------

//------
private:
//------

	KString m_sTraceID;
	KString m_sSpanID;
	KString m_sParentID;
	KString m_sTraceState;
	uint8_t m_iFlags { 0 };

}; // KTraceContext

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Sets a context as the context of the current thread, and sets the previous one
/// again when it ends
class DEKAF2_PUBLIC KTraceContext::Scope
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// @param Context the context of the current thread, an empty one for none
	explicit Scope(KTraceContext Context);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~Scope();
	//-----------------------------------------------------------------------------

	Scope(const Scope&)            = delete;
	Scope& operator=(const Scope&) = delete;

//------
private:
//------

	KTraceContext m_Previous;

}; // KTraceContext::Scope

//-----------------------------------------------------------------------------
template<class Callable>
auto KTraceContext::Bind(Callable Function)
//-----------------------------------------------------------------------------
{
	return [Context = Current(), Function = std::move(Function)](auto&&... Args) mutable
	{
		Scope Trace(Context);
		return Function(std::forward<decltype(Args)>(Args)...);
	};

} // Bind

/// @}

DEKAF2_NAMESPACE_END
