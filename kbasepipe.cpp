#include "kbasepipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KBasePipe::KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KBasePipe::~KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Destructor

//-----------------------------------------------------------------------------
bool KBasePipe::IsRunning()
//-----------------------------------------------------------------------------
{

	bool bResponse = false;

	// sets m_iReadChildStatus if iPid is not zero
	wait();

	// Did we fail to get a status?
	if (-2 == m_iChildStatus)
	{
		m_iExitCode = -1;
		return bResponse;
	}

	// Do we have an exit status code to interpret?
	if (false == m_bChildStatusValid)
	{
		bResponse = true;
		return bResponse;
	}

	// did the called function "exit" normally?
	if (WIFEXITED(m_iChildStatus))
	{
		m_iExitCode = WEXITSTATUS(m_iChildStatus);
		return bResponse;
	}

	return bResponse;

} // IsRunning

//-----------------------------------------------------------------------------
bool KBasePipe::WaitForFinished(int msecs)
//-----------------------------------------------------------------------------
{
	if (msecs >= 0)
	{
		int counter = 0;
		while (IsRunning())
		{
			usleep(1000);
			++counter;

			if (counter == msecs)
				return false;
		}
		return true;
	}
	return false;
} // WaitForFinished

//-----------------------------------------------------------------------------
bool KBasePipe::splitArgsInPlace(KString& argString, CharVec& argVector)
//-----------------------------------------------------------------------------
{
	argVector.push_back(&argString[0]);
	for (size_t i = 0; i < argString.size(); ++i)
	{
		if (argString[i] == ' ')
		{
			argString[i] = '\0';
			if ((argString.size() > i + 1) && (argString[i+1] == '"'))
			{
				argString[i+1] = '\0';
				argVector.push_back(&argString[i+2]);
				do
				{
					++i;
				} while (argString[i] != '"');
				argString[i] = '\0'; // null terminate spaced region
			}
			else
			{
				argVector.push_back(&argString[i + 1]);
			}
		}
	}
	argVector.push_back(nullptr); // null terminate
	return !argVector.empty();
} // splitArgs

//-----------------------------------------------------------------------------
bool KBasePipe::wait()
//-----------------------------------------------------------------------------
{
	int iStatus = 0;

	pid_t iPid;

	// status can only be read ONCE
	if (true == m_bChildStatusValid)
	{
		// status has already been set. do not read it again, you might get an invalid status.
		return true;
	} // end status is already set

	iPid = waitpid(m_pid, &iStatus, WNOHANG);

	// is the status valid?
	if (0 < iPid)
	{
		// save the status
		m_iChildStatus = iStatus;
		m_bChildStatusValid = true;
		return true;
	}

	if ((iPid == -1) && (errno != EINTR))
	{
		kDebug(0, "KBasePipe::wait got an invalid status iPid = -1. Errno {} : {}", errno, strerror(errno));
		m_iChildStatus = -2;
		m_bChildStatusValid = true;
		return true;
	}

	return false;
} // wait

} // end namespace dekaf2
