/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

/// @file kasio.h
/// provides asio include files and basic asio routines

#ifdef __clang__
	// clang erroneously warns 'allocator<void>' being deprecated - which is not
	// true, only explicit specializations of allocator<void> are deprecated in C++17
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef DEKAF2_IS_MSC
	// doesn't find builtin on at least VS 2022
	#define BOOST_DISABLE_CURRENT_LOCATION
#endif

// define BOOST_ASIO_NO_DEPRECATED only for testing!
// ASIO unfortunately deprecates functions continuously
// and the dekaf2 code needs to get adapted to it then,
// because deprecated functions actually get deleted
// after some time.
//#define BOOST_ASIO_NO_DEPRECATED 1

// asio misses to include <utility> (std::exchange) in asio/awaitable.hpp, so we
// load it here explicitly
#include <utility>

#include <boost/asio.hpp>

#if (BOOST_VERSION < 106600)
	#define DEKAF2_CLASSIC_ASIO 1
#endif

#if (BOOST_VERSION >= 107300)
	#define DEKAF2_HAS_ASIO_CONTEXT_FROM_OPENSSL_CONTEXT 1
#endif

#if DEKAF2_CLASSIC_ASIO
	#include <boost/asio/io_service.hpp>
#else
	#include <boost/asio/io_context.hpp>
	// instead of rewriting all io_service with io_context we use this typedef
	namespace boost { namespace asio { using io_service = io_context; }}
#endif

#include <boost/asio/basic_socket_iostream.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/deadline_timer.hpp>

#ifdef __clang__
	#pragma clang diagnostic pop
#endif
