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
#include "../kstring.h"
#include "../klog.h"
#include "kasio.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename StreamType>
struct KAsioStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	KAsioStream(int _iSecondsTimeout = 15)
	//-----------------------------------------------------------------------------
	: IOService       { 1 }
	, Socket          { IOService }
	, Timer           { IOService }
	, iSecondsTimeout { _iSecondsTimeout }
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
		if (Socket.is_open())
		{
			boost::system::error_code ec;
			Socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
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
			Socket.close(ec);
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
		uint16_t buffer;

		Socket.receive(boost::asio::buffer(&buffer, 1), Socket.message_peek, ec);

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
		Timer.expires_from_now(boost::posix_time::seconds(iSecondsTimeout));
	}

	//-----------------------------------------------------------------------------
	void ClearTimer()
	//-----------------------------------------------------------------------------
	{
		Timer.expires_at(boost::posix_time::pos_infin);
	}

	//-----------------------------------------------------------------------------
	void CheckTimer()
	//-----------------------------------------------------------------------------
	{
		if (Timer.expires_at() <= boost::asio::deadline_timer::traits_type::now())
		{
			boost::system::error_code ignored_ec;
			Socket.close(ignored_ec);
			Timer.expires_at(boost::posix_time::pos_infin);
			kDebug(2, "Connection timeout ({} seconds): {}",
				   iSecondsTimeout, sEndpoint);
		}

		Timer.async_wait(std::bind(&KAsioStream<StreamType>::CheckTimer, this));
	}

	//-----------------------------------------------------------------------------
	void RunTimed()
	//-----------------------------------------------------------------------------
	{
		ResetTimer();

		ec = boost::asio::error::would_block;
		do
		{
			IOService.run_one();
		}
		while (ec == boost::asio::error::would_block);

		ClearTimer();
	}

	boost::asio::io_service IOService;
	StreamType Socket;
	KString sEndpoint;
	boost::asio::deadline_timer Timer;
	boost::system::error_code ec;
	int iSecondsTimeout;

}; // KAsioStream

DEKAF2_NAMESPACE_END
