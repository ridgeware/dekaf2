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

/*
 * A C++11 implementation of scope guards
 *
 */

#pragma once

/// @file kscopeguard.h
/// Ad-hoc definition of RAII exit actions

#include "bits/kcppcompat.h"
#include <functional>
#include <utility>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// C++11 implementation of a scope guard - a RAII helper that executes arbitrary callables at end of scope
class DEKAF2_PUBLIC KScopeGuard
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KScopeGuard() = default;
	KScopeGuard(const KScopeGuard&) = delete;

	/// move construction
	KScopeGuard(KScopeGuard&& other) noexcept;

	/// Construct a scope guard from any type of callable with function signature void(void), e.g. a lambda
	/// At end of life of Guard the callable will be called, if not either been trigger() ed or dismiss() ed before.
	/// Sample:
	/// @code
	/// KScopeGuard Guard = [fd]{ close(fd); };
	/// @endcode
	template<class Func>
	KScopeGuard(Func callable)
	: m_Callable(std::move(callable))
	{}

	/// Calls the callable if still valid
	~KScopeGuard() noexcept
	{
		call();
	}

	/// move assignment
	KScopeGuard& operator=(KScopeGuard&& other) noexcept;

	/// Call the callable immediately if still valid, invalidates it after execution
	void trigger() noexcept
	{
		call();
	}

	/// Reset (delete) the callable so that it will not be executed at end of scope
	void reset() noexcept
	{
		m_Callable = nullptr;
	}

//----------
private:
//----------

	void call() noexcept;

	std::function<void()> m_Callable;

}; // KScopeGuard

} // end of namespace dekaf2

