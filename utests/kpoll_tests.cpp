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

#include "catch.hpp"

#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/system/os/ksystem.h>
#include <thread>

using namespace dekaf2;

TEST_CASE("KPoll")
{
	SECTION("interruptor wakes a poll")
	{
		KPollInterruptor Interruptor;

		// valid on ALL platforms - on Windows through a loopback socket pair
		REQUIRE ( Interruptor.IsValid() );

		// quiet after construction
		CHECK ( kPoll(Interruptor.GetFD(), POLLIN, KDuration()) == 0 );

		// a wake makes the fd readable, a clear drains it
		Interruptor.Wake();
		CHECK ( (kPoll(Interruptor.GetFD(), POLLIN, chrono::seconds(5)) & POLLIN) == POLLIN );
		Interruptor.Clear();
		CHECK ( kPoll(Interruptor.GetFD(), POLLIN, KDuration()) == 0 );

		// a wake from another thread unblocks a waiting poll well before its timeout
		std::thread Waker([&Interruptor]()
		{
			kSleep(chrono::milliseconds(50));
			Interruptor.Wake();
		});

		KStopTime Took;
		CHECK ( (kPoll(Interruptor.GetFD(), POLLIN, chrono::seconds(10)) & POLLIN) == POLLIN );
		CHECK ( Took.elapsed() < chrono::seconds(5) );

		Waker.join();
	}
}
