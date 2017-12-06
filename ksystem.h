/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
*/

#pragma once

/// @file ksystem.h
/// general system utilities for dekaf2

#include <cstdlib>
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p sDefault if not found.
inline const char* kGetEnv (const char* sEnvVar, const char* sDefault = "")
//-----------------------------------------------------------------------------
{
	const char* sValue = ::getenv(sEnvVar);
	if (sValue)
	{
		return (sValue);
	}
	else
	{
		return (sDefault);
	}

} // kGetEnv

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p sDefault if not found.
inline const char* kGetEnv (const KString& sEnvVar, const char* sDefault = "")
//-----------------------------------------------------------------------------
{
	return kGetEnv(sEnvVar.c_str(), sDefault);
}

//-----------------------------------------------------------------------------
/// Set environment variable.
inline void kSetEnv (const char* sEnvVar, const char* sValue)
//-----------------------------------------------------------------------------
{
	if (!::setenv(sEnvVar, sValue, true))
	{
		kWarning("cannot set {} = {}", sEnvVar, sValue);
	}
}

//-----------------------------------------------------------------------------
/// Set environment variable.
inline void kSetEnv (const KString& sEnvVar, const KString& sValue)
//-----------------------------------------------------------------------------
{
	kSetEnv(sEnvVar.c_str(), sValue.c_str());
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
inline void kUnsetEnv (const char* sEnvVar)
//-----------------------------------------------------------------------------
{
	if (!::unsetenv(sEnvVar))
	{
		kWarning("cannot unset {}", sEnvVar);
	}
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
inline void kUnsetEnv (const KString& sEnvVar)
//-----------------------------------------------------------------------------
{
	kUnsetEnv(sEnvVar.c_str());
}

} // end of namespace dekaf2

