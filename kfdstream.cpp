/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/


#include "kfdstream.h"
#include "klog.h"
#include <sys/stat.h>
#ifdef DEKAF2_IS_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
std::streamsize FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	if (filedesc)
	{
		int fd = *static_cast<int*>(filedesc);

		if (fd >= 0)
		{
			do
			{
#ifdef DEKAF2_IS_WINDOWS
				iRead = _read(fd, sBuffer, static_cast<uint32_t>(iCount));
#else
				iRead = ::read(fd, sBuffer, static_cast<size_t>(iCount));
#endif
			}
			while (iRead == -1 && errno == EINTR);
			// we use these readers and writers in pipes and shells
			// which may die and generate a SIGCHLD, which interrupts
			// file reads and writes..
		}

		if (iRead < 0)
		{
			// do some logging
			kWarning("cannot read from file: {} - requested {}, got {} bytes",
			         strerror(errno),
			         iCount,
			         iRead);
		}
	}

	return iRead;

} // FileDescReader

//-----------------------------------------------------------------------------
std::streamsize FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote { 0 };

	if (filedesc)
	{
		int fd = *static_cast<int*>(filedesc);

		if (fd >= 0)
		{
			do
			{
#ifdef DEKAF2_IS_WINDOWS
				iWrote = _write(fd, sBuffer, static_cast<uint32_t>(iCount));
#else
				iWrote = ::write(fd, sBuffer, static_cast<size_t>(iCount));
#endif
			}
			while (iWrote == -1 && errno == EINTR);
			// we use these readers and writers in pipes and shells
			// which may die and generate a SIGCHLD, which interrupts
			// file reads and writes..
		}

		if (iWrote != iCount)
		{
			// do some logging
			kWarning("cannot write to file: {}", strerror(errno));
		}
	}

	return iWrote;

} // FileDescWriter

//-----------------------------------------------------------------------------
std::streamsize FilePtrReader(void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	if (fileptr)
	{
		FILE** fp = static_cast<FILE**>(fileptr);
		if (fp && *fp)
		{
			do
			{
				iRead = static_cast<std::streamsize>(std::fread(sBuffer, 1, static_cast<size_t>(iCount), *fp));
			}
			while (iRead == -1 && errno == EINTR);
			// we use these readers and writers in pipes and shells
			// which may die and generate a SIGCHLD, which interrupts
			// file reads and writes..

			if (iRead < 0)
			{
				// do some logging
				kWarning("cannot read from file: {} - requested {}, got {} bytes",
				         strerror(errno),
				         iCount,
				         iRead);
			}
		}
	}

	return iRead;

} // FilePtrReader

//-----------------------------------------------------------------------------
std::streamsize FilePtrWriter(const void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote { 0 };

	if (fileptr)
	{
		FILE** fp = static_cast<FILE**>(fileptr);
		if (fp && *fp)
		{
			do
			{
				iWrote = static_cast<std::streamsize>(std::fwrite(sBuffer, 1, static_cast<size_t>(iCount), *fp));
			}
			while (iWrote == -1 && errno == EINTR);
			// we use these readers and writers in pipes and shells
			// which may die and generate a SIGCHLD, which interrupts
			// file reads and writes..

			if (iWrote != iCount)
			{
				// do some logging
				kWarning("cannot write to file: {}", strerror(errno));
			}
		}
	}

	return iWrote;

} // FilePtrWriter

} // end of namespace detail



//-----------------------------------------------------------------------------
void KInputFDStream::open(int iFileDesc)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FileDesc = iFileDesc;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInputFDStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FileDesc >= 0)
	{
		if (::close(m_FileDesc))
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FileDesc = -1;
	}

} // close


//-----------------------------------------------------------------------------
void KInputFPStream::open(FILE* iFilePtr)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FilePtr = iFilePtr;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInputFPStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FilePtr != nullptr)
	{
		if (std::fclose(m_FilePtr))
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FilePtr = nullptr;
	}

} // close


//-----------------------------------------------------------------------------
void KOutputFDStream::open(int iFileDesc)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it..

	m_FileDesc = iFileDesc;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KOutputFDStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FileDesc >= 0)
	{
		base_type::flush();
		if (::close(m_FileDesc))
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FileDesc = -1;
	}

} // close


//-----------------------------------------------------------------------------
void KOutputFPStream::open(FILE* iFilePtr)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it..

	m_FilePtr = iFilePtr;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KOutputFPStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FilePtr != nullptr)
	{
		base_type::flush();
		if (::fclose(m_FilePtr))
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FilePtr = nullptr;
	}

} // close


//-----------------------------------------------------------------------------
void KInOutFDStream::open(int iFileDescR, int iFileDescW)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FileDescR = iFileDescR;

	if (iFileDescW < 0)
	{
		m_FileDescW = m_FileDescR;
	}
	else
	{
		m_FileDescW = iFileDescW;
	}

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInOutFDStream::close()
//-----------------------------------------------------------------------------
{
	bool bSameFD = m_FileDescR == m_FileDescW;

	if (m_FileDescR >= 0)
	{
		if (::close(m_FileDescR))
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FileDescR = -1;
	}

	if (!bSameFD)
	{
		if (m_FileDescW >= 0)
		{
			if (::close(m_FileDescW))
			{
				kWarning("Cannot close file: {}", strerror(errno));
			}
		}
	}
	m_FileDescW = -1;

} // close

template class KWriter<KOutputFDStream>;
template class KWriter<KOutputFPStream>;
template class KReader<KInputFDStream>;
template class KReader<KInputFPStream>;
template class KReaderWriter<KInOutFDStream>;

} // end of namespace dekaf2

