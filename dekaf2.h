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

/// @file dekaf2.h
/// basic initialization of the library

#include <atomic>
#include <folly/CpuId.h>
#include "kconfiguration.h"
#include "kstring.h"

/// @namespace dekaf2 The basic dekaf2 library namespace. All functions,
/// variables and classes are prefixed with this namespace.
namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic initialization of the library.
class Dekaf
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Dekaf();
	Dekaf(const Dekaf&) = delete;
	Dekaf(Dekaf&&) = delete;
	Dekaf& operator=(const Dekaf&) = delete;
	Dekaf& operator=(Dekaf&&) = delete;

	//---------------------------------------------------------------------------
	/// Switch library to multi threaded mode.
	/// Set directly after program start if run in multithreading,
	/// otherwise libs risk to be wrongly initialized when functionality is used
	/// that relies on this setting.
	void SetMultiThreading()
	//---------------------------------------------------------------------------
	{
		m_bIsMultiThreading = true;
	}

	//---------------------------------------------------------------------------
	/// Shall we be prepared for multithrading?
	inline bool GetMultiThreading() const
	//---------------------------------------------------------------------------
	{
		return m_bIsMultiThreading.load(std::memory_order_relaxed);
	}

	//---------------------------------------------------------------------------
	/// Set application name. Used for logging.
	void SetName(const KString& sName)
	//---------------------------------------------------------------------------
	{
		m_sName = sName;
	}

	//---------------------------------------------------------------------------
	/// Get application name as provided by user.
	const KString& GetName() const
	//---------------------------------------------------------------------------
	{
		return m_sName;
	}

	//---------------------------------------------------------------------------
	/// Set the unicode locale. If empty defaults to the locale set by the current user.
	bool SetUnicodeLocale(KString name = KString{});
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get the unicode locale.
	const KString& GetUnicodeLocale()
	//---------------------------------------------------------------------------
	{
		return m_sLocale;
	}

	//---------------------------------------------------------------------------
	/// Get the CPU ID and extensions
	const folly::CpuId& GetCpuId() const
	//---------------------------------------------------------------------------
	{
		return m_CPUID;
	}

//----------
private:
//----------

	std::atomic_bool m_bIsMultiThreading{false};
	KString m_sName;
	KString m_sLocale;
	folly::CpuId m_CPUID;

}; // Dekaf

//---------------------------------------------------------------------------
/// Get unique instance of class Dekaf()
class Dekaf& Dekaf();
//---------------------------------------------------------------------------

} // end of namespace dekaf2
