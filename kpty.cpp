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

#include "kpty.h"

#ifdef DEKAF2_HAS_PIPES

#include "klog.h"
#include <unistd.h>
#include <poll.h>
#include <cerrno>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
std::streamsize KPTYReader(void* sBuffer, std::streamsize iCount, void* pContext)
//-----------------------------------------------------------------------------
{
	auto* pCtx = static_cast<KPTYReaderContext*>(pContext);

	if (!pCtx || !pCtx->pMasterFD)
	{
		return 0;
	}

	int fd = *pCtx->pMasterFD;

	if (fd < 0)
	{
		return 0;
	}

	// if a timeout is configured, wait for data with poll()
	if (pCtx->pTimeout)
	{
		auto iMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(*pCtx->pTimeout).count();

		struct pollfd pfd;
		pfd.fd      = fd;
		pfd.events  = POLLIN;
		pfd.revents = 0;

		int iReady;

		do
		{
			iReady = ::poll(&pfd, 1, static_cast<int>(iMilliseconds));
		}
		while (iReady == -1 && errno == EINTR);

		if (iReady <= 0)
		{
			// timeout or error
			return 0;
		}

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			// error condition on the fd
			if (!(pfd.revents & POLLIN))
			{
				return 0;
			}
		}
	}

	auto iRead = ::read(fd, sBuffer, static_cast<size_t>(iCount));

	if (iRead < 0)
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			return 0;
		}
		return -1;
	}

	return iRead;

} // KPTYReader

} // end of namespace detail

//-----------------------------------------------------------------------------
void KPTYIOStream::open(int iMasterFD)
//-----------------------------------------------------------------------------
{
	m_iMasterFD = iMasterFD;

	if (m_iMasterFD >= 0)
	{
		clear();
	}

} // open

//-----------------------------------------------------------------------------
void KPTYIOStream::close()
//-----------------------------------------------------------------------------
{
	if (m_iMasterFD >= 0)
	{
		::close(m_iMasterFD);
		m_iMasterFD = -1;
	}

} // close

template class KReaderWriter<KPTYIOStream>;

//-----------------------------------------------------------------------------
bool KPTY::Open(LoginMode Mode,
				KStringView sShell,
				KDuration Timeout,
				const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	if (!KBasePTY::Open(sShell, Mode, Environment))
	{
		return false;
	}

	KPTYIOStream::SetTimeout(Timeout);
	KPTYIOStream::open(KBasePTY::m_iMasterFD);

	return KPTYStream::good();

} // Open

//-----------------------------------------------------------------------------
int KPTY::Close(int iWaitMilliseconds)
//-----------------------------------------------------------------------------
{
	// invalidate the stream - we close the FD in KBasePTY::Close
	KPTYStream::Cancel();

	return KBasePTY::Close(iWaitMilliseconds);

} // Close

DEKAF2_NAMESPACE_END

#endif
