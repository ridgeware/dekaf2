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
*/

#pragma once

#if defined __GNUC__
	#define DEKAF2_FUNCTION_NAME __PRETTY_FUNCTION__
#else
	#define DEKAF2_FUNCTION_NAME __FUNCTION__
#endif

#if !defined(DEKAF2_IS_OSX) && defined(__APPLE__) && defined(__MACH__)
	#define DEKAF2_IS_OSX 1
#endif

#if !defined(UNIX) && (defined(unix) || defined(__unix__) || defined(DEKAF2_IS_OSX))
	#define UNIX 1
#endif

#if !defined(DEKAF2_IS_UNIX) && defined(UNIX)
	#define DEKAF2_IS_UNIX
#endif

#if (__cplusplus < 201103L && !DEKAF2_HAS_CPP_11)
	#error "this version of dekaf needs at least a C++11 compiler"
#endif

#ifndef DEKAF2_HAS_CPP_11
	#define DEKAF2_HAS_CPP_11
#endif

#if (__cplusplus >= 201402L && !defined(DEKAF2_HAS_CPP_14))
	#define DEKAF2_HAS_CPP_14
#endif

// this test is a bit bogus (by just testing if the cpp date
// is younger than that of C++14), but it should probably even
// be kept after C++17 defines an official date, as older
// compilers would not know it (but support C++17)
#if (__cplusplus > 201402L && !defined(DEKAF2_HAS_CPP_17))
	#define DEKAF2_HAS_CPP_17
#endif

