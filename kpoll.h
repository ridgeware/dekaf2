/*
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

/// @file kpoll.h
/// Maintaining a list of file descriptors and associated actions to call when the file descriptor creates an event

#include "bits/kcppcompat.h"

// there is no poll on windows, so these classes remain non-functional currently
#ifndef DEKAF2_IS_WINDOWS

#include "kthreadsafe.h"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cinttypes>
#include <poll.h>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Maintaining a list of file descriptors and associated actions to call when the file descriptor creates an event
class KPoll
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the callback 
	using CallbackT = std::function<void(int, uint16_t, std::size_t)>;

	struct Parameters
	{
		CallbackT   Callback;              ///< the callback function
		std::size_t iParameter { 0 };      ///< arbitrary parameter to pass in callback
		uint16_t    iEvents    { 0 };      ///< the file desc events to watch for
		bool        bOnce      { false };  ///< trigger only once, or repeatedly?
	};

	KPoll(uint32_t iMilliseconds = 100, bool bAutoStart = true)
	: m_iTimeout(iMilliseconds)
	, m_bAutoStart(bAutoStart)
	{
	}

	virtual ~KPoll();

	/// add a file descriptor to watch
	void Add(int fd, Parameters Parms);
	/// remove a file descriptor from watch
	void Remove(int fd);

	/// start the watcher
	void Start();
	/// stop the watcher
	void Stop();

//----------
protected:
//----------

	void BuildPollVec(std::vector<pollfd>& fds);
	void Triggered(int fd, uint16_t events);
	virtual void Watch();

	KThreadSafe<std::unordered_map<int, Parameters>> m_FileDescriptors;

	uint32_t         m_iTimeout   {   100 };
	std::atomic_bool m_bModified  { false };
	std::atomic_bool m_bStop      { false };

//----------
private:
//----------

	std::unique_ptr<std::thread> m_Thread;

	bool             m_bAutoStart {  true };

}; // KPoll

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Watch sockets for disconnects - no need to set an event mask. Adapts to different trigger
/// conditions on MacOS and Linux
class KSocketWatch : public KPoll
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using KPoll::KPoll;

//----------
protected:
//----------

	virtual void Watch() override final;

}; // KSocketWatch

#else // DEKAF2_IS_WINDOWS

namespace dekaf2 {

/// non-functional version for Windows
class KPoll
{

//----------
public:
//----------

	/// the callback
	using CallbackT = std::function<void(int, uint16_t, std::size_t)>;

	struct Parameters
	{
		CallbackT   Callback;
		std::size_t iParameter { 0 };
		uint16_t    iEvents    { 0 };
		bool        bOnce      { false };
	};

	KPoll(uint32_t iMilliseconds = 100, bool bAutoStart = true) {}
	void Add(int fd, Parameters Parms) {}
	void Remove(int fd) {}
	void Start() {}
	void Stop() {}

}; // KPoll

using KSocketWatch = KPoll;

#endif // DEKAF2_IS_WINDOWS

} // end of namespace dekaf2
