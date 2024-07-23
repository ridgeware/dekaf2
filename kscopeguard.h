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

#include "kdefinitions.h"
#include <functional>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// C++11 implementation of a scope guard - a RAII helper that executes arbitrary callables at end of scope.
/// It is moveable, and has a release() member function.
#if DEKAF2_HAS_CONCEPTS
template<typename T>
concept KScopeGuardCallable = requires(const T& callable) 
{
	// make sure callable does not need arguments and returns void
	// (ideally should also test for noexcept, but that pushes the
	// work to the caller)
	{ callable() } -> std::same_as<void>;
};
template<KScopeGuardCallable F>
#else
template<typename F>
#endif
class DEKAF2_PUBLIC KScopeGuard
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// move construction
	KScopeGuard(KScopeGuard&& other) noexcept
	: m_Callable(std::move(other.m_Callable))
	{
		other.release();
	}

	/// Construct a moveable scope guard from any type of callable with function signature void(void),
	/// e.g. a lambda. At end of life of Guard the callable will be called.
	/// Sample:
	/// @code
	/// auto Guard = KScopeGuard([fd]{ close(fd); });
	/// @endcode
	KScopeGuard(F callable)
	: m_Callable(callable)
	{}

	/// Calls the callable
	~KScopeGuard() noexcept(false)
	{
		if (m_bActive)
		{
			m_Callable();
		}
	}

	KScopeGuard(const KScopeGuard&)            = delete;
	KScopeGuard& operator=(const KScopeGuard&) = delete;
	KScopeGuard& operator=(KScopeGuard&&)      = delete;

	/// disable the exit function
	void release() noexcept
	{
		m_bActive = false;
	}

	// keep a compatibility alias until 06/2024
	/// DEPRECATED, please use release()
	void reset() noexcept { release(); }

//----------
private:
//----------

	F m_Callable;
	bool m_bActive { true };

}; // KScopeGuard

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// this is a non-moveable scope guard
#if DEKAF2_HAS_CONCEPTS
template<KScopeGuardCallable F>
#else
template<typename F>
#endif
class DEKAF2_PUBLIC KSimpleScopeGuard
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct a scope guard from any type of callable with function signature void(void), e.g. a lambda
	/// At end of life of Guard the callable will be called.
	/// Sample:
	/// @code
	/// auto Guard = KSimpleScopeGuard([fd]{ close(fd); });
	/// // or
	/// KAtScopeEnd( close(fd) );
	/// @endcode
	KSimpleScopeGuard(F callable)
	: m_Callable(callable)
	{}

	/// Calls the callable
	~KSimpleScopeGuard() noexcept(false)
	{
		m_Callable();
	}

	KSimpleScopeGuard(const KSimpleScopeGuard&)            = delete;
	KSimpleScopeGuard(KSimpleScopeGuard&&)                 = delete;
	KSimpleScopeGuard& operator=(const KSimpleScopeGuard&) = delete;
	KSimpleScopeGuard& operator=(KSimpleScopeGuard&&)      = delete;

//----------
private:
//----------

	F m_Callable;

}; // KSimpleScopeGuard

#define DEKAF2_AtScopeEnd_Inner(lambdaName, guardName, ...) \
	auto lambdaName = [&]() { __VA_ARGS__; };               \
	KSimpleScopeGuard<decltype(lambdaName)> guardName(lambdaName);

#define DEKAF2_AtScopeEnd_Outer(counter, ...) \
	DEKAF2_AtScopeEnd_Inner(DEKAF2_concat(DEKAF2_ScopeGuard_lambda, counter),   \
	                        DEKAF2_concat(DEKAF2_ScopeGuard_instance, counter), \
	                        __VA_ARGS__)

#define KAtScopeEnd(...) DEKAF2_AtScopeEnd_Outer(__COUNTER__, __VA_ARGS__)

DEKAF2_NAMESPACE_END
