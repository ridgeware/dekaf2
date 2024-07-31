/*
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

/// @file kerror.h
/// provides dekaf2 error base class

#include "kexception.h"
#include "ksourcelocation.h"

namespace boost { namespace system {

class error_code;

}}

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2 error base class
class DEKAF2_PUBLIC KErrorBase : private KError
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base = KError;

//----------
public:
//----------

	KErrorBase() = default;

	using base::base;

	/// should we throw on errors, or should they only be noted in GetError() (on construction, throwing is off)
	void SetThrowOnError(bool bYes) { m_bThrow = bYes; }

	/// returns true if this class would throw on error
	DEKAF2_NODISCARD
	bool WouldThrowOnError() const { return m_bThrow; }

	/// Returns the cause of an error if the return string is non-empty. Otherwise no error occured.
	DEKAF2_NODISCARD
	KStringViewZ GetLastError() const { return message(); }

	/// returns the uint16_t error code of this error (0 if no error)
	DEKAF2_NODISCARD
	uint16_t GetLastErrorCode() const { return value(); }

	DEKAF2_NODISCARD
	KStringViewZ Error() const { return GetLastError(); }

	/// get a copy of this error for propagation into other classes
	DEKAF2_NODISCARD
	const KErrorBase& CopyLastError() const { return *this; }

	/// returns true if an error is set
	DEKAF2_NODISCARD
	bool HasError() const { return value(); }

	/// clear the error, if any
	void ClearError();

//----------
protected:
//----------

	/// copy an existing error into this class. Only bThrow will remain from this class.
	bool SetError(const KErrorBase& Error) const;
	/// set an error description, always returns false
	bool SetError(KStringViewZ sError, uint16_t iErrorCode = -1, KStringView sFunction = KSourceLocation::current().function_name()) const;
	/// set an error from a boost error code
	bool SetError(const boost::system::error_code& ec,           KStringView sFunction = KSourceLocation::current().function_name()) const;
	/// set an error description according to latest errno
	bool SetErrnoError(KStringView sPrefix = "os error: ",       KStringView sFunction = KSourceLocation::current().function_name()) const;

	// throw when setting error string?
	bool m_bThrow { false };

//----------
private:
//----------

}; // KErrorBase

DEKAF2_NAMESPACE_END
