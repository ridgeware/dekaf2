/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kasiostream.h
/// provides asio stream abstraction with deadline timer

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kcrashexit.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/net/tcp/bits/kasio.h>

#include <atomic>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

template<typename StreamType>
struct KAsioTraits
{
	static bool SocketIsOpen(StreamType& Socket)
		{ return Socket.is_open(); }
	static void SocketShutdown(StreamType& Socket, boost::system::error_code& ec)
		{ Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec); }
	static void SocketClose(StreamType& Socket, boost::system::error_code& ec)
		{ Socket.close(ec); }
	static void SocketCancel(StreamType& Socket, boost::system::error_code& ec)
		{ Socket.cancel(ec); }
	static void SocketPeek(StreamType& Socket, boost::system::error_code& ec)
		{ uint16_t buffer; Socket.receive(boost::asio::buffer(&buffer, 1), Socket.message_peek, ec); }
};

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// asio stream with deadline timer, the transport under the socket stream
/// classes.
///
/// Reads and writes share one io_service and one completion state, so a
/// stream may be driven by ONE thread at any point in time (the thread may
/// change, with synchronization on the handover) - concurrent operations
/// consume each other's completions and corrupt each other's stack frames.
/// Concurrent use trips an assert in RunTimed() in debug builds, and a
/// kWarning in release builds. The one sanctioned cross-thread call is
/// closing the socket to unblock a pending operation.
template<typename StreamType, typename Traits = detail::KAsioTraits<StreamType>>
struct KAsioStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	// the constructor for non-tls-sockets
	KAsioStream(KDuration Timeout)
	//-----------------------------------------------------------------------------
	: IOService { 1 }
	, Socket    { IOService }
	, Timer     { IOService }
	, Timeout   { Timeout }
	{
		ClearTimer();
		CheckTimer();
	}

	//-----------------------------------------------------------------------------
	// the constructor for tls-sockets (or anything else needing a context)
	template<typename Context>
	KAsioStream(Context& context, KDuration Timeout)
	//-----------------------------------------------------------------------------
	: IOService { 1 }
	, Socket    { IOService , context.GetContext() }
	, Timer     { IOService }
	, Timeout   { Timeout }
	{
		ClearTimer();
		CheckTimer();
	}

	//-----------------------------------------------------------------------------
	~KAsioStream()
	//-----------------------------------------------------------------------------
	{
		Disconnect();

	} // dtor

	//-----------------------------------------------------------------------------
	/// disconnect the stream
	bool Disconnect()
	//-----------------------------------------------------------------------------
	{
		if (Traits::SocketIsOpen(Socket))
		{
			boost::system::error_code ec;

			Traits::SocketShutdown(Socket, ec);

			if (ec)
			{
				// do not display the shutdown error message when the socket has
				// already been disconnected
				if (ec.value() != boost::asio::error::not_connected)
				{
					kDebug(2, "error shutting down socket: {}", ec.message());
				}
			}

			Traits::SocketClose(Socket, ec);

			if (ec)
			{
				kDebug(2, "error closing socket: {}", ec.message());
				return false;
			}

			kDebug(2, "disconnected from: {}", sEndpoint);
		}

		return true;

	} // Disconnect

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	bool IsDisconnected()
	//-----------------------------------------------------------------------------
	{
		Traits::SocketPeek(Socket, ec);

		if (ec == boost::asio::error::would_block)
		{
			// open but no data
			ec.clear();
			return false;
		}
		else if (!ec)
		{
			// open and data
			return false;
		}

		// ec == boost::asio::error::eof would signal a closed socket,
		// but we treat all other errors as disconnected as well
		return true;

	} // IsDisconnected

	//-----------------------------------------------------------------------------
	void ResetTimer()
	//-----------------------------------------------------------------------------
	{
#if DEKAF2_CLASSIC_ASIO
		Timer.expires_from_now(boost::posix_time::milliseconds(Timeout.milliseconds().count()));
#else
		Timer.expires_after(Timeout.milliseconds());
#endif
	}

	//-----------------------------------------------------------------------------
	void ClearTimer()
	//-----------------------------------------------------------------------------
	{
#if DEKAF2_CLASSIC_ASIO
		Timer.expires_at(boost::posix_time::pos_infin);
#else
		Timer.expires_at(chrono::steady_clock::now() + chrono::years(10));
#endif
	}

	//-----------------------------------------------------------------------------
	void CheckTimer()
	//-----------------------------------------------------------------------------
	{
#if DEKAF2_CLASSIC_ASIO
		if (Timer.expires_at() <= boost::asio::deadline_timer::traits_type::now())
#else
		if (Timer.expiry() <= chrono::steady_clock::now())
#endif
		{
			boost::system::error_code ignored_ec;

			if (bCancelOnTimeout)
			{
				// cancel the pending operations, but keep the connection open -
				// they return with an aborted error, and the caller may retry
				Traits::SocketCancel(Socket, ignored_ec);
				kDebug(3, "operation timeout ({}), canceled: {}",
					   Timeout, sEndpoint);
			}
			else
			{
				Traits::SocketClose(Socket, ignored_ec);
				kDebug(2, "Connection timeout ({}): {}",
					   Timeout, sEndpoint);
			}

			ClearTimer();
		}

		Timer.async_wait(std::bind(&KAsioStream<StreamType, Traits>::CheckTimer, this));
	}

	//-----------------------------------------------------------------------------
	void RunTimed()
	//-----------------------------------------------------------------------------
	{
		// a tripwire, not a lock: overlapping operations mean two threads on
		// one stream, which corrupts the shared completion state (see the
		// class docs) - make that loud instead of silently losing data. The
		// exchange costs a few nanoseconds against the syscalls it guards,
		// so it stays active in release builds, where it warns instead of
		// crashing
		if (DEKAF2_UNLIKELY(bInRun.exchange(true)))
		{
#ifdef NDEBUG
			kWarning("concurrent operations on a socket stream to {} - streams may only be used by one thread at a time", sEndpoint);
#else
			detail::kFailedAssert("concurrent operations on a socket stream - streams may only be used by one thread at a time");
#endif
		}

		ResetTimer();

		try
		{
			ec = boost::asio::error::would_block;
			do
			{
				IOService.run_one();
			}
			while (ec == boost::asio::error::would_block);
		}

		catch (const boost::system::system_error& e)
		{
			kDebug(1, "Stream error: {}", e.code().message());
		}

		ClearTimer();

		if (bCancelOnTimeout && ec == boost::asio::error::operation_aborted)
		{
			// a canceled operation is a repeatable timeout, not a stream
			// error - it must neither fail Good() nor block later writes
			ec.clear();
		}

		bInRun = false;
	}

	boost::asio::io_service     IOService;
	StreamType                  Socket;
	KString                     sEndpoint;
#if DEKAF2_CLASSIC_ASIO
	boost::asio::deadline_timer Timer;
#else
	boost::asio::steady_timer   Timer;
#endif
	boost::system::error_code   ec;
	KDuration                   Timeout;
	/// cancel operations on timeout instead of closing the connection
	bool                        bCancelOnTimeout { false };
	/// tripwire against concurrent operations, see RunTimed()
	std::atomic<bool>           bInRun           { false };

}; // KAsioStream

DEKAF2_NAMESPACE_END
