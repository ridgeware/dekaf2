/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file kpty.h
/// Open an interactive Unix shell via a pseudo-terminal (PTY), with optional
/// credential-based login and configurable read timeout

#include "bits/kbaseprocess.h"

#ifdef DEKAF2_HAS_PIPES

#include "kstring.h"
#include "kduration.h"
#include "kfdstream.h"
#include "kstreambuf.h"
#include <streambuf>
#include <iostream>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
/// context for the PTY reader callback (timeout-aware)
struct KPTYReaderContext
//-----------------------------------------------------------------------------
{
	int*       pMasterFD { nullptr };
	KDuration* pTimeout  { nullptr };
};

//-----------------------------------------------------------------------------
/// custom streambuf reader for PTY master FDs, with poll()-based timeout
std::streamsize DEKAF2_PUBLIC KPTYReader(void* sBuffer, std::streamsize iCount, void* pContext);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a std::iostream for a PTY master file descriptor with timeout support on reads
class DEKAF2_PUBLIC KPTYIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	using base_type = std::iostream;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KPTYIOStream()
	//-----------------------------------------------------------------------------
		: base_type(&m_StreamBuf)
	{
	}

	//-----------------------------------------------------------------------------
	KPTYIOStream(const KPTYIOStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KPTYIOStream(KPTYIOStream&& other) noexcept
	//-----------------------------------------------------------------------------
	: KPTYIOStream()
	{
		// take over state from other
		m_iMasterFD       = other.m_iMasterFD;
		m_Timeout         = other.m_Timeout;
		other.m_iMasterFD = -1;
	}

	//-----------------------------------------------------------------------------
	KPTYIOStream& operator=(const KPTYIOStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KPTYIOStream& operator=(KPTYIOStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// open the stream on a PTY master file descriptor
	void open(int iMasterFD);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a PTY is associated to this stream
	DEKAF2_NODISCARD
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_iMasterFD >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// invalidate the file descriptor without closing it
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_iMasterFD = -1;
	}

	//-----------------------------------------------------------------------------
	/// get the file descriptor
	DEKAF2_NODISCARD
	int GetDescriptor() const
	//-----------------------------------------------------------------------------
	{
		return m_iMasterFD;
	}

	//-----------------------------------------------------------------------------
	/// set the read timeout
	void SetTimeout(KDuration Timeout)
	//-----------------------------------------------------------------------------
	{
		m_Timeout = Timeout;
	}

	//-----------------------------------------------------------------------------
	/// get the read timeout
	DEKAF2_NODISCARD
	KDuration GetTimeout() const
	//-----------------------------------------------------------------------------
	{
		return m_Timeout;
	}

//----------
protected:
//----------

	int       m_iMasterFD { -1 };
	KDuration m_Timeout   { chrono::seconds(30) };

	detail::KPTYReaderContext m_ReaderContext { &m_iMasterFD, &m_Timeout };

	// see comment in KOutputFDStream about the legality
	// to only construct the KStreamBuf here, but to use it in
	// the constructor before
	KStreamBuf m_StreamBuf { &detail::KPTYReader, &detail::FileDescWriter, &m_ReaderContext, &m_iMasterFD };

};

extern template class KReaderWriter<KPTYIOStream>;

/// PTY reader/writer with KInStream/KOutStream interface
using KPTYStream = KReaderWriter<KPTYIOStream>;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Open an interactive Unix shell via a pseudo-terminal (PTY).
/// Supports credential-based login and configurable read timeout.
/// Inherits std::iostream (via KPTYStream) for stream-based I/O.
class DEKAF2_PUBLIC KPTY : public KBaseProcess, public KPTYStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum LoginMode
	{
		NoLogin,       ///< spawn a shell without login
		Login          ///< spawn login, expects credentials via stream
	};

	//-----------------------------------------------------------------------------
	/// Default Constructor
	KPTY() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Opens a PTY with an interactive shell.
	/// @param Mode NoLogin (default) or Login (expects credentials via stream)
	/// @param sShell shell to use. If empty, uses $SHELL or /bin/sh for NoLogin,
	///        /usr/bin/login for Login.
	/// @param Timeout read timeout (default 30 seconds)
	/// @param Environment additional environment variables for the child
	KPTY(LoginMode Mode,
		 KStringView sShell = {},
		 KDuration Timeout = chrono::seconds(30),
		 const std::vector<std::pair<KString, KString>>& Environment = {})
	//-----------------------------------------------------------------------------
	{
		Open(Mode, sShell, Timeout, Environment);
	}

	//-----------------------------------------------------------------------------
	~KPTY()
	//-----------------------------------------------------------------------------
	{
		Close();
	}

	//-----------------------------------------------------------------------------
	/// Opens a PTY with an interactive shell.
	/// @param Mode NoLogin (default) or Login (expects credentials via stream)
	/// @param sShell shell to use. If empty, uses $SHELL or /bin/sh for NoLogin,
	///        /usr/bin/login for Login.
	/// @param Timeout read timeout (default 30 seconds)
	/// @param Environment additional environment variables for the child
	/// @return true on success
	bool Open(LoginMode Mode = NoLogin,
			  KStringView sShell = {},
			  KDuration Timeout = chrono::seconds(30),
			  const std::vector<std::pair<KString, KString>>& Environment = {});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes the PTY and waits for the child process to terminate.
	/// @param Timeout waits for the given duration, then kills child process.
	///        Default is KDuration::max(), which will wait until child terminates.
	/// @return the exit code received from the child
	int Close(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Terminate the running process. Initially with signal SIGINT, after Timeout
	/// with SIGKILL
	bool Kill(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the PTY window size. Sends SIGWINCH to the child process.
	bool SetWindowSize(uint16_t iRows, uint16_t iCols);
	//-----------------------------------------------------------------------------

//--------
protected:
//--------

	int     m_iMasterFD  { -1 };
	KString m_sSecondaryName;

}; // class KPTY

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_PIPES
