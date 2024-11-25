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

#include "kerror.h"
#include "klog.h"
#include <boost/system/error_code.hpp>
#include <errno.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KErrorBase::SetError(const KErrorBase& Error) const
//-----------------------------------------------------------------------------
{
	// keeps this classes members as they are, but copies over all from the base class
	const_cast<base*>(static_cast<const base*>(this))->operator=(Error);
	return false;
}

//-----------------------------------------------------------------------------
bool KErrorBase::SetError(KStringViewZ sError, uint16_t iErrorCode, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	if (!sError.empty())
	{
		if (DEKAF2_UNLIKELY(1 <= KLog::s_iThreadLogLevel))
		{
			KLog::getInstance().debug_fun(1, sFunction, sError);
		}
	}

	SetError(KErrorBase(sError, iErrorCode));

	if (m_bThrow && HasError())
	{
		throw base(*this);
	}

	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KErrorBase::SetError(const boost::system::error_code& ec, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	return SetError(ec.message(), ec.value(), sFunction);
}

//-----------------------------------------------------------------------------
bool KErrorBase::SetErrnoError(KStringView sPrefix, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	return SetError(kFormat("{}{}", sPrefix, strerror(errno)), errno, sFunction);
}

//-----------------------------------------------------------------------------
void KErrorBase::ClearError()
//-----------------------------------------------------------------------------
{
	clear();
}

#if 0
// test for debugging ..
static_assert(std::is_nothrow_move_constructible<std::runtime_error>::value, "std::runtime_error is not nothrow move constructible, but should be");
#endif

static_assert(!std::is_nothrow_move_constructible<std::runtime_error>::value || std::is_nothrow_move_constructible<KErrorBase>::value,
			  "KErrorBase is intended to be nothrow move constructible, but is not!");


DEKAF2_NAMESPACE_END
