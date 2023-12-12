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

/// @file kread.h
/// read signal safe from a file or device

#include "kdefinitions.h"
#include <cstdio>
#include <cstdint>
#include <istream>

DEKAF2_NAMESPACE_BEGIN

/// Reads iCount chars from file descriptor into sBuffer, even on growing pipes or other unseekable inputs
DEKAF2_PUBLIC
std::size_t kRead(int fd, void* sBuffer, std::size_t iCount);

/// Reads iCount chars from FILE* into sBuffer, even on growing pipes or other unseekable inputs
DEKAF2_PUBLIC
std::size_t kRead(FILE* fp, void* sBuffer, std::size_t iCount);

/// Reads iCount chars from Stream into sBuffer
DEKAF2_PUBLIC
std::size_t kRead(std::istream& Stream, void* sBuffer, std::size_t iCount);

/// Read a character. Returns std::istream::traits_type::eof() (== -1) if no input available
DEKAF2_PUBLIC
std::istream::int_type kRead(std::istream& Stream);

/// Read a character. Returns 0/false if no input available
DEKAF2_PUBLIC
std::size_t kRead(std::istream& Stream, char& ch);

/// Un-read iCount characters. Returns count of characters that could not get unread (so, 0 on success)
DEKAF2_PUBLIC
std::size_t kUnRead(std::istream& Stream, std::size_t iCount = 1);

DEKAF2_NAMESPACE_END
