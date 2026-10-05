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
/// Open an interactive shell via a pseudo-terminal (PTY, on Windows a pseudo console),
/// with optional credential-based login and configurable read timeout

#include <dekaf2/system/process/bits/kbaseprocess.h>

#ifdef DEKAF2_HAS_PTY

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/io/streams/kfdstream.h>
#include <dekaf2/io/streams/kstreambuf.h>
#include <streambuf>
#include <iostream>
#ifdef DEKAF2_IS_WINDOWS
	#include <atomic>
	#include <mutex>
	#include <thread>
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup system_process
/// @{

namespace detail {

//-----------------------------------------------------------------------------
/// context for the PTY reader callback (timeout-aware)
struct KPTYReaderContext
//-----------------------------------------------------------------------------
{
	int*       pReadFD  { nullptr };
	KDuration* pTimeout { nullptr };
};

//-----------------------------------------------------------------------------
/// custom streambuf reader for PTY master FDs, with a timeout
std::streamsize DEKAF2_PUBLIC KPTYReader(void* sBuffer, std::streamsize iCount, void* pContext);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a std::iostream for a PTY master file descriptor with timeout support on reads.
/// On Windows, the pseudo console has two pipes instead of the one master: one for
/// reading its output, and one for writing its input.
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
		m_iReadFD        = other.m_iReadFD;
		m_iWriteFD       = other.m_iWriteFD;
		m_Timeout        = other.m_Timeout;
		other.m_iReadFD  = -1;
		other.m_iWriteFD = -1;
	}

	//-----------------------------------------------------------------------------
	KPTYIOStream& operator=(const KPTYIOStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KPTYIOStream& operator=(KPTYIOStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// open the stream on a PTY master file descriptor
	void open(int iMasterFD)
	//-----------------------------------------------------------------------------
	{
		open(iMasterFD, iMasterFD);
	}

	//-----------------------------------------------------------------------------
	/// open the stream on a file descriptor for reading and one for writing
	void open(int iReadFD, int iWriteFD);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a PTY is associated to this stream
	DEKAF2_NODISCARD
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_iReadFD >= 0;
	}

	//-----------------------------------------------------------------------------
	/// close the stream
	void close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// invalidate the file descriptors without closing them
	void Cancel()
	//-----------------------------------------------------------------------------
	{
		m_iReadFD  = -1;
		m_iWriteFD = -1;
	}

	//-----------------------------------------------------------------------------
	/// get the file descriptor for reading
	DEKAF2_NODISCARD
	int GetDescriptor() const
	//-----------------------------------------------------------------------------
	{
		return m_iReadFD;
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

	int       m_iReadFD  { -1 };
	int       m_iWriteFD { -1 };
	KDuration m_Timeout  { chrono::seconds(30) };

	detail::KPTYReaderContext m_ReaderContext { &m_iReadFD, &m_Timeout };

	// see comment in KOutputFDStream about the legality
	// to only construct the KStreamBuf here, but to use it in
	// the constructor before
	KStreamBuf m_StreamBuf { &detail::KPTYReader, &detail::FileDescWriter, &m_ReaderContext, &m_iWriteFD };

};

extern template class KReaderWriter<KPTYIOStream>;

/// PTY reader/writer with KInStream/KOutStream interface
using KPTYStream = KReaderWriter<KPTYIOStream>;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Open an interactive shell via a pseudo-terminal (PTY).
/// Supports credential-based login and configurable read timeout.
/// Inherits std::iostream (via KPTYStream) for stream-based I/O.
///
/// On Windows, the shell runs in a pseudo console (ConPTY, since Windows 10 1809),
/// whose output contains the VT sequences with which it renders its screen. Enter is
/// "\r", as from a terminal. The pseudo console stays open after the shell has ended:
/// a read then ends with the timeout, not with the end of the output.
///
/// In Login mode on Windows, KPTY asks for the user name and the password itself, as
/// /usr/bin/login does, and checks them against the Windows account: a plain name is
/// a local account, DOMAIN\\user and user@domain are domain accounts. The shell then
/// runs as this user, in the profile directory. A user other than the one of this
/// process needs a service under LocalSystem - without its privileges, only the user
/// of this process can log in.
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
	///        /usr/bin/login for Login. On Windows the command line to execute, and
	///        if empty or /bin/sh the command interpreter (%COMSPEC%, cmd.exe) - in
	///        Login mode the shell after the login.
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
	///        /usr/bin/login for Login. On Windows the command line to execute, and
	///        if empty or /bin/sh the command interpreter (%COMSPEC%, cmd.exe) - in
	///        Login mode the shell after the login.
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
	/// with SIGKILL. On Windows the shell is terminated right away, together with
	/// all processes it started.
	bool Kill(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the PTY window size. Sends SIGWINCH to the child process.
	bool SetWindowSize(uint16_t iRows, uint16_t iCols);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_IS_WINDOWS
	// in Login mode, the shell starts only after the login, which runs in a thread of
	// this class - these methods of KBaseProcess cover it

	//-----------------------------------------------------------------------------
	/// Checks if the shell is running - in Login mode also while the login runs before it
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Waits up to Timeout for the end of the shell - in Login mode also for the end
	/// of a failed login
	bool Wait(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Terminates the shell, or the login before it, see KBaseProcess::Terminate()
	bool Terminate();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the process ID of the shell, 0 while the login runs
	pid_t GetProcessID();
	//-----------------------------------------------------------------------------
#endif

//--------
protected:
//--------

#ifdef DEKAF2_IS_WINDOWS
	int     m_iInputFD       { -1 };      // our end of the input pipe of the pseudo console
	int     m_iOutputFD      { -1 };      // our end of the output pipe of the pseudo console
	void*   m_hPseudoConsole { nullptr }; // HPCON of the pseudo console
	void*   m_hUserToken     { nullptr }; // token of the logged in user, if the shell runs for another user
	void*   m_hUserProfile   { nullptr }; // the loaded profile of that user
	uint16_t m_iRows         { 25 };      // the size of the pseudo console, guarded by m_LoginMutex
	uint16_t m_iCols         { 80 };

	std::thread       m_LoginThread;
	std::mutex        m_LoginMutex;               // for the start of the shell after the login
	std::atomic<bool> m_bLoginActive    { false }; // the login runs, the shell is not started yet
	std::atomic<bool> m_bCancelLogin    { false };
	int               m_iLoginExitCode  { 0 };     // written by the login thread, read after the join
	bool              m_bLoginStartedShell { false };

//--------
private:
//--------

	//-----------------------------------------------------------------------------
	/// starts the shell in a new pseudo console on the given ends of its pipes, which
	/// it takes over - with the login running, the caller holds m_LoginMutex
	uint32_t StartShell(void* hInputRead, void* hOutputWrite, KStringView sCommandLine,
	                    const std::vector<std::pair<KString, KString>>& Environment,
	                    KStringViewZ sWorkingDirectory = KStringViewZ{},
	                    void* hUserToken = nullptr);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the login, which runs in m_LoginThread, and starts the shell after a successful
	/// login - it takes over the given ends of the pipes
	void RunLogin(void* hInputRead, void* hOutputWrite, KString sCommandLine,
	              std::vector<std::pair<KString, KString>> Environment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// joins the login thread once it has ended (or with bWait at once), and takes
	/// over its exit code if it started no shell
	void FinishLogin(bool bWait);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// unloads the profile of a logged in user, and closes the token
	void ReleaseUser();
	//-----------------------------------------------------------------------------
#else
	int     m_iMasterFD  { -1 };
	KString m_sSecondaryName;
#endif

}; // class KPTY


/// @}

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_PTY
