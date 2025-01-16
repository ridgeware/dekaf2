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

#include "../kdefinitions.h"
#include "../kduration.h"
#include "../kstring.h"
#include "../klog.h"
#include "kasio.h"

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
	static void SocketPeek(StreamType& Socket, boost::system::error_code& ec)
		{ uint16_t buffer; Socket.receive(boost::asio::buffer(&buffer, 1), Socket.message_peek, ec); }
};

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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
				return false;
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
			Traits::SocketClose(Socket, ignored_ec);
			ClearTimer();
			kDebug(2, "Connection timeout ({}): {}",
				   Timeout, sEndpoint);
		}

		Timer.async_wait(std::bind(&KAsioStream<StreamType, Traits>::CheckTimer, this));
	}

	//-----------------------------------------------------------------------------
	void RunTimed()
	//-----------------------------------------------------------------------------
	{
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

		catch (const boost::system::error_code& local_ec)
		{
			kDebug(1, "Stream error: {}", local_ec.message());
		}

		ClearTimer();
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

}; // KAsioStream

DEKAF2_NAMESPACE_END
