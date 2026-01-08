/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

/// @file kipaddress.h
/// ip address checks

#include "kstringview.h"

DEKAF2_NAMESPACE_BEGIN

/// checks if an IP address is a IPv6 address like '[a0:ef::c425:12]'
/// @param sAddress the string to test
/// @param bNeedsBraces if true, address has to be in square braces [ ], if false they must not be present
/// @return true if sAddress holds an IPv6 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv6Address(KStringView sAddress, bool bNeedsBraces);

/// checks if an IP address is a IPv4 address like '1.2.3.4'
/// @param sAddress the string to test
/// @return true if sAddress holds an IPv4 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv4Address(KStringView sAddress);

/// Return true if the string represents a private IP address, both IPv4 and IPv6
/// @param sIP the IP address
/// @param bExcludeDocker if set to true, addresses of the 172.x.x.x range
/// are not treated as private IPs, as these are often the result of docker TCP proxying
/// @return true if sIP is a private IP
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsPrivateIP(KStringView sIP, bool bExcludeDocker = true);

DEKAF2_NAMESPACE_END
