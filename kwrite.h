/*
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
*/

#pragma once

/// @file kwrite.h
/// file/stream write atoms: signal safe and non-locale dependant

#include "kdefinitions.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ostream>

DEKAF2_NAMESPACE_BEGIN

/// Writes iCount chars from sBuffer into file descriptor, signal safe
DEKAF2_PUBLIC
std::size_t kWrite(int fd, const void* sBuffer, std::size_t iCount);

/// Writes iCount chars from sBuffer into FILE*, signal safe
DEKAF2_PUBLIC
std::size_t kWrite(FILE* fp, const void* sBuffer, std::size_t iCount);

/// Writes iCount chars from sBuffer into std::ostream, not touching the locale sentinel
DEKAF2_PUBLIC
std::size_t kWrite(std::ostream& Stream, const void* sBuffer, std::size_t iCount);

/// Writes chars from string literal into std::ostream, not touching the locale sentinel
DEKAF2_PUBLIC inline
std::size_t kWrite(std::ostream& Stream, const char* sBuffer)
{
	return kWrite(Stream, sBuffer, std::strlen(sBuffer));
}

/// Writes one char into std::ostream, not touching the locale sentinel
DEKAF2_PUBLIC
std::size_t kWrite(std::ostream& Stream, char ch);

DEKAF2_NAMESPACE_END