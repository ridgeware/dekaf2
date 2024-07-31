/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

/// @file ksourcelocation.h
/// provides KSourceLocation as typedef for std::source_location,
/// or as a replacement class for unexisting or buggy std::source_location implementations.
/// The CLang std::source_location implementation is obviously seriously bugged until clang 16,
/// the std::source_location object does not resolve at the call site at all. Starting with clang 16
/// all works as expected.
/// The replacement implementation uses compiler builtins like __builtin_FUNCTION().
/// While resolving at the call site, these do _never_ return object names, return or
/// parameter values. At places where macros can be used, DEKAF2_FUNCTION_NAME
/// should be used - it resolves the full signature.

#include <cstdint>

#ifndef DEKAF2_HAS_INCLUDE
	// __has_include was added with gcc 4.9.2
	#ifdef __has_include
		#define DEKAF2_HAS_INCLUDE(x) __has_include(x)
	#else
		#define __has_include(x) 0
	#endif
#endif

#if DEKAF2_HAS_INCLUDE("kdefinitions.h")
	#include "kdefinitions.h"
#endif

#ifndef DEKAF2_NAMESPACE_BEGIN
#	define DEKAF2_NAMESPACE_BEGIN
#endif

#ifndef DEKAF2_NAMESPACE_END
	#define DEKAF2_NAMESPACE_END
#endif

// __builtin_FUNCTION() __builtin_LINE() and __builtin_FILE() work reliably 
// (at the caller's site) since gcc 4.9.0 and clang 9 and VS16.7 / MSVC 19.27

// starting with gcc 11, <source_location> is found, but it is only activated
// with --std=c++20. Luckily that is also linked to __cpp_lib_source_location >= 201907L,
// so checking for both conditions in one branch is ok

// std::source_location is broken in clang until 16 - never use it before

// we do not test for experimental/source_location as it does not
// give us anything better than what we get through the __builtins
// (even with gcc it did not return object names or parameters)

// formats:
// MSVC since v19.36 / VS17.6 returns: class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > __cdecl B::myfunc(int)
// MSVC since 19.29 / VS16.11 returns: myfunc
// above with C++20 and __cplusplus > 201703L
// MSVC since 19.27 / VS16.7 knows __builtin_FUNCTION() and friends: _MSC_VER > 1926

// GCC and clang, with source_location, return: std::string B::myfunc(int)
// the fallback implementation returns: myfunc

#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L
	#if DEKAF2_HAS_INCLUDE(<version>)
		#include <version>
	#endif
#else
	#if DEKAF2_HAS_INCLUDE(<ciso646>)
		#include <ciso646>
	#endif
#endif

#if (__cplusplus > 201703L || _MSVC_LANG > 201703L) \
	&& (__cpp_lib_source_location >= 201907L) \
	&& (!defined(__clang__) || __clang_major__ > 15) \
	&& __has_include(<source_location>)

	#include <source_location>
	DEKAF2_NAMESPACE_BEGIN
	using KSourceLocation = std::source_location;
	DEKAF2_NAMESPACE_END

	#define DEKAF2_HAS_STD_SOURCE_LOCATION 1
	#define DEKAF2_HAS_BUILTIN_FUNCTION 1
	#define DEKAF2_HAS_BUILTIN_LINE 1
	#define DEKAF2_HAS_BUILTIN_FILE 1

#else

	#if (!defined(__clang__) && (__GNUC__ >= 5 || __GNUC__ == 4 && __GNUC_MINOR__ >= 9 )) \
		|| (__clang__ >= 9) \
		|| (_MSC_VER > 1926)

		#define DEKAF2_HAS_BUILTIN_FUNCTION 1
		#define DEKAF2_HAS_BUILTIN_LINE 1
		#define DEKAF2_HAS_BUILTIN_FILE 1

	#else

		// only gcc 10+ has __has_builtin, so it's pretty useless for our use case
		// and we use the fixed versions for known compilers above. The below
		// is only for other compilers

		#ifndef DEKAF2_HAS_BUILTIN
			#ifdef __has_builtin
				#define DEKAF2_HAS_BUILTIN(x) __has_builtin(x)
			#else
				#define DEKAF2_HAS_BUILTIN(x) 0
			#endif
		#endif

		#if DEKAF2_HAS_BUILTIN(__builtin_FUNCTION)
			#define DEKAF2_HAS_BUILTIN_FUNCTION 1
		#endif

		#if DEKAF2_HAS_BUILTIN(__builtin_LINE)
			#define DEKAF2_HAS_BUILTIN_LINE 1
		#endif

		#if DEKAF2_HAS_BUILTIN(__builtin_FILE)
			#define DEKAF2_HAS_BUILTIN_FILE 1
		#endif

	#endif

	DEKAF2_NAMESPACE_BEGIN

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// modeled after the C++20 std::source_location, returns limited names (no return values, no parameters, no objects, just the pure function names)
	class KSourceLocation
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		KSourceLocation() = default;

		/// get the function name
		constexpr const char* function_name () const noexcept { return m_sFunction; }
		/// get the file name
		constexpr const char* file_name     () const noexcept { return m_sFile;     }
		/// get the line number
		constexpr uint32_t    line          () const noexcept { return m_iLine;     }


		/// get current location in a KSourceLocation object
		static constexpr KSourceLocation current(
			const char* sFunction =
	#if DEKAF2_HAS_BUILTIN_FUNCTION
									__builtin_FUNCTION(),
	#else
									"",
	#endif
			const char* sFile     =
	#if DEKAF2_HAS_BUILTIN_FILE
									__builtin_FILE(),
	#else
									"",
	#endif
			uint32_t    iLine     =
	#if DEKAF2_HAS_BUILTIN_LINE
									__builtin_LINE()
	#else
									0
	#endif
			) noexcept
		{
			return KSourceLocation(sFunction, sFile, iLine);
		}

	//----------
	private:
	//----------

		/// ctor, only indirectly used through current()
		constexpr KSourceLocation(const char* sFunction, const char* sFile, uint32_t iLine) noexcept
		: m_sFunction (sFunction)
		, m_sFile     (sFile)
		, m_iLine     (iLine)
		{
		}

		const char*    m_sFunction { "" };
		const char*    m_sFile     { "" };
		const uint32_t m_iLine     {  0 };

	}; // KSourceLocation

	DEKAF2_NAMESPACE_END

#endif
