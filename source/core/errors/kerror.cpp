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

#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <boost/system/error_code.hpp>
#include <errno.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KErrorBase::SetThrowOnError(bool bYes)
//-----------------------------------------------------------------------------
{
	m_State.SetFlag(bYes);

	if (bYes && HasError())
	{
		throw KError(m_State);
	}

} // SetThrowOnError

//-----------------------------------------------------------------------------
bool KErrorBase::SetError(const KErrorBase& Error) const
//-----------------------------------------------------------------------------
{
	// keeps the throw flag of this class as it is, and takes over the error
	m_State.SetError(Error.m_State);
	return false;
}

//-----------------------------------------------------------------------------
bool KErrorBase::SetError(KStringViewZ sError, uint16_t iErrorCode, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_WITH_KLOG
	if (!sError.empty())
	{
		if (kWouldLog(1))
		{
			KLog::getInstance().debug_fun(1, sFunction, sError);
		}
	}
#endif

	SetError(KErrorBase(sError, iErrorCode));

	if (WouldThrowOnError() && HasError())
	{
		throw KError(m_State);
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
	m_State.clear();
}

static_assert(std::is_nothrow_move_constructible<KErrorBase>::value,
			  "KErrorBase is intended to be nothrow move constructible, but is not!");

static_assert(sizeof(KErrorBase) == sizeof(void*),
			  "KErrorBase is intended to have the size of one pointer, but has not!");

// std::exception is one pointer with libstdc++ and libc++, and three with the library of MSVC
static_assert(sizeof(KException) == sizeof(std::exception) + sizeof(void*),
			  "KException is intended to add one pointer to std::exception, but does not!");


DEKAF2_NAMESPACE_END
