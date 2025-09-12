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
#include "kcompatibility.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
std::streamsize FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(filedesc == nullptr))
	{
		return 0;
	}

	int fd = *static_cast<int*>(filedesc);

	return static_cast<std::streamsize>(kRead(fd, sBuffer, static_cast<size_t>(iCount)));

} // FileDescReader

//-----------------------------------------------------------------------------
std::streamsize FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{

	if (DEKAF2_UNLIKELY(filedesc == nullptr))
	{
		return 0;
	}

	int fd = *static_cast<int*>(filedesc);

	return static_cast<std::streamsize>(kWrite(fd, sBuffer, static_cast<size_t>(iCount)));

} // FileDescWriter

//-----------------------------------------------------------------------------
std::streamsize FilePtrReader(void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(fileptr == nullptr))
	{
		return 0;
	}

	FILE** fp = static_cast<FILE**>(fileptr);

	return static_cast<std::streamsize>(kRead(*fp, sBuffer, static_cast<size_t>(iCount)));

} // FilePtrReader

//-----------------------------------------------------------------------------
std::streamsize FilePtrWriter(const void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(fileptr == nullptr))
	{
		return 0;
	}

	FILE** fp = static_cast<FILE**>(fileptr);

	return static_cast<std::streamsize>(kWrite(*fp, sBuffer, static_cast<size_t>(iCount)));

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
#if DEKAF2_IS_WINDOWS
		if (_close(m_FileDesc))
#else
		if (::close(m_FileDesc))
#endif
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

#if DEKAF2_IS_WINDOWS
		if (_close(m_FileDesc))
#else
		if (::close(m_FileDesc))
#endif
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
#if DEKAF2_IS_WINDOWS
		if (_close(m_FileDescR))
#else
		if (::close(m_FileDescR))
#endif
		{
			kWarning("Cannot close file: {}", strerror(errno));
		}
		m_FileDescR = -1;
	}

	if (!bSameFD)
	{
		if (m_FileDescW >= 0)
		{
#if DEKAF2_IS_WINDOWS
			if (_close(m_FileDescW))
#else
			if (::close(m_FileDescW))
#endif
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

DEKAF2_NAMESPACE_END
