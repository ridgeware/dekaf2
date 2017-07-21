/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
//
// For documentation, try: http://www.ridgeware.com/home/dekaf/
//
//-----------------------------------------------------------------------------//
*/

#pragma once

#include <mutex>
#include <vector>
#include <condition_variable>
#include <map>
#include <csignal>
#include "kparallel.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSignals
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	typedef std::function<void(int)> std_func_t;
	typedef void (*signal_func_t)(int);

	KSignals(bool bStartHandlerThread = true);
	~KSignals();

	void IgnoreAllSignals();
	void SetAllSignalHandlers(std_func_t func, bool bAsThread = false);
	void SetSignalHandler(int iSignal, signal_func_t func, bool bAsThread = false);
	void SetSignalHandler(int iSignal, std_func_t func, bool bAsThread = false);
	void IgnoreSignalHandler(int iSignal)
	{
		IntDelSignalHandler(iSignal, SIG_IGN);
	}
	void DefaultSignalHandler(int iSignal)
	{
		IntDelSignalHandler(iSignal, SIG_DFL);
	}
	void WaitForSignalHandlersSet();

//----------
private:
//----------
	void PushSigsToSet(int iSignal, signal_func_t func);
	/// only call with func == SIG_IGN or SIG_DFL
	void IntDelSignalHandler(int iSignal, signal_func_t func);
	void IntSetSignalHandler(int iSignal, signal_func_t func);
	void WaitForSignals();

	static void LookupFunc(int signal);

	struct sigset_t
	{
		int iSignal;
		signal_func_t func;
	};

	struct sigmap_t
	{
		std_func_t func;
		bool bAsThread;
	};

	static const std::vector<int> m_AllSigs;
	static const std::vector<int> m_SettableSigs;

	static std::mutex s_SigSetMutex;
	static std::map<int, sigmap_t> s_SigFuncs;
	static KRunThreads m_Threads;

	typedef std::unique_lock<std::mutex> CondVarLock;
	std::mutex m_CondVarMutex;
	std::condition_variable m_CondVar;
	std::vector<sigset_t> m_SigsToSet;
};

const char* kTranslateSignal (int iSignalNum, bool bConcise = true);


} // end of namespace dekaf2

