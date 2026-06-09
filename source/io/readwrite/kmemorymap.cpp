/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
// |\|   Software without restriction, including without limitation        |\|
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

#include <dekaf2/io/readwrite/kmemorymap.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <utility>

#if DEKAF2_IS_WINDOWS
	#include <windows.h>
	#include <dekaf2/core/strings/kutf.h> // UTF-8 -> UTF-16 for the Windows W API
	#define DEKAF2_KMEMORYMAP_USE_WINDOWS 1
#elif DEKAF2_HAS_INCLUDE(<sys/mman.h>)
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#define DEKAF2_KMEMORYMAP_USE_POSIX 1
	#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
		#define MAP_ANONYMOUS MAP_ANON
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
KMemoryRegion::KMemoryRegion (KMemoryRegion&& other) noexcept
//-----------------------------------------------------------------------------
{
	*this = std::move (other);

} // move ctor

//-----------------------------------------------------------------------------
KMemoryRegion& KMemoryRegion::operator= (KMemoryRegion&& other) noexcept
//-----------------------------------------------------------------------------
{
	if (this != &other)
	{
		Close ();

		m_iSize       = other.m_iSize;
		m_pMapBase    = other.m_pMapBase;
		m_pMapHandle  = other.m_pMapHandle;
		m_pFileHandle = other.m_pFileHandle;
		m_sFallback   = std::move (other.m_sFallback);
		m_bWritable   = other.m_bWritable;
		m_bPrivate    = other.m_bPrivate;
		m_bAnonymous  = other.m_bAnonymous;

		// the range either points at the mapping base or into the fallback buffer -
		// the latter address may change with the move, so recompute it
		m_pData = (m_pMapBase != nullptr) ? static_cast<const char*>(m_pMapBase)
		        : (m_iSize != 0)          ? m_sFallback.data()
		        :                           nullptr;

		other.m_pData       = nullptr;
		other.m_iSize       = 0;
		other.m_pMapBase    = nullptr;
		other.m_pMapHandle  = nullptr;
		other.m_pFileHandle = nullptr;
		other.m_bWritable   = false;
		other.m_bPrivate    = false;
		other.m_bAnonymous  = false;
	}

	return *this;

} // move assignment

//-----------------------------------------------------------------------------
void* KMemoryRegion::MutableBase ()
//-----------------------------------------------------------------------------
{
	if (!m_bWritable)
	{
		return nullptr;
	}

	if (m_pMapBase != nullptr)
	{
		return m_pMapBase;
	}

	// anonymous mapping that fell back to a heap buffer
	return m_iSize ? m_sFallback.data() : nullptr;

} // MutableBase

//-----------------------------------------------------------------------------
bool KMemoryRegion::MapFileIntoMemory (KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	m_sFallback = kReadAll (sFilename);

	if (m_sFallback.empty())
	{
		return SetError (kFormat ("cannot read file '{}'", sFilename));
	}

	m_pData      = m_sFallback.data();
	m_iSize      = m_sFallback.size();
	m_bWritable  = false;
	m_bPrivate   = false;
	m_bAnonymous = false;

	kDebug (2, "read {} bytes of '{}' into memory", m_iSize, sFilename);
	return true;

} // MapFileIntoMemory

//-----------------------------------------------------------------------------
bool KMemoryRegion::AcquireFile (KStringViewZ sFilename, bool bWritable, bool bPrivate)
//-----------------------------------------------------------------------------
{
	Close ();
	ClearError ();

#if defined(DEKAF2_KMEMORYMAP_USE_POSIX)

	int iFD = ::open (sFilename.c_str(), bWritable ? O_RDWR : O_RDONLY);

	if (iFD < 0)
	{
		if (!bWritable)
		{
			return MapFileIntoMemory (sFilename);
		}
		return SetErrnoError (kFormat ("cannot open file '{}': ", sFilename));
	}

	struct stat St;

	if (::fstat (iFD, &St) != 0)
	{
		SetErrnoError (kFormat ("cannot stat file '{}': ", sFilename));
		::close (iFD);
		return false;
	}

	if (St.st_size <= 0)
	{
		::close (iFD);
		return SetError (kFormat ("file '{}' is empty", sFilename));
	}

	auto  iSize  = static_cast<std::size_t>(St.st_size);
	int   iProt  = PROT_READ | (bWritable ? PROT_WRITE : 0);
	int   iFlags = (bWritable && !bPrivate) ? MAP_SHARED : MAP_PRIVATE;
	void* pMap   = ::mmap (nullptr, iSize, iProt, iFlags, iFD, 0);

	// the mapping stays valid after the descriptor is closed
	::close (iFD);

	if (pMap == MAP_FAILED)
	{
		if (!bWritable)
		{
			kDebug (2, "mmap() failed for '{}', falling back to reading into memory", sFilename);
			return MapFileIntoMemory (sFilename);
		}
		return SetErrnoError (kFormat ("cannot map file '{}': ", sFilename));
	}

	m_pMapBase   = pMap;
	m_pData      = static_cast<const char*>(pMap);
	m_iSize      = iSize;
	m_bWritable  = bWritable;
	m_bPrivate   = bPrivate;
	m_bAnonymous = false;

	kDebug (2, "mapped {} bytes of '{}' ({})", m_iSize, sFilename,
	        bWritable ? (bPrivate ? "rw-private" : "rw-shared") : "ro");
	return true;

#elif defined(DEKAF2_KMEMORYMAP_USE_WINDOWS)

	auto wsPath = kutf::Convert<std::wstring>(sFilename);

	DWORD  iAccess = GENERIC_READ | (bWritable ? GENERIC_WRITE : 0u);
	HANDLE hFile   = ::CreateFileW (wsPath.c_str(), iAccess, FILE_SHARE_READ,
	                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		if (!bWritable)
		{
			return MapFileIntoMemory (sFilename);
		}
		return SetError (kFormat ("cannot open file '{}'", sFilename));
	}

	LARGE_INTEGER iFileSize;

	if (!::GetFileSizeEx (hFile, &iFileSize) || iFileSize.QuadPart <= 0)
	{
		::CloseHandle (hFile);
		return SetError (kFormat ("file '{}' is empty or cannot be sized", sFilename));
	}

	DWORD  iProtect = bWritable ? (bPrivate ? PAGE_WRITECOPY : PAGE_READWRITE) : PAGE_READONLY;
	HANDLE hMapping = ::CreateFileMappingW (hFile, nullptr, iProtect, 0, 0, nullptr);

	if (hMapping == nullptr)
	{
		::CloseHandle (hFile);
		if (!bWritable)
		{
			kDebug (2, "CreateFileMapping() failed for '{}', falling back to reading into memory", sFilename);
			return MapFileIntoMemory (sFilename);
		}
		return SetError (kFormat ("cannot create mapping for '{}'", sFilename));
	}

	DWORD iMapAccess = bWritable ? (bPrivate ? FILE_MAP_COPY : FILE_MAP_WRITE) : FILE_MAP_READ;
	void* pView      = ::MapViewOfFile (hMapping, iMapAccess, 0, 0, 0);

	if (pView == nullptr)
	{
		::CloseHandle (hMapping);
		::CloseHandle (hFile);
		if (!bWritable)
		{
			kDebug (2, "MapViewOfFile() failed for '{}', falling back to reading into memory", sFilename);
			return MapFileIntoMemory (sFilename);
		}
		return SetError (kFormat ("cannot map view of '{}'", sFilename));
	}

	m_pFileHandle = hFile;
	m_pMapHandle  = hMapping;
	m_pMapBase    = pView;
	m_pData       = static_cast<const char*>(pView);
	m_iSize       = static_cast<std::size_t>(iFileSize.QuadPart);
	m_bWritable   = bWritable;
	m_bPrivate    = bPrivate;
	m_bAnonymous  = false;

	kDebug (2, "mapped {} bytes of '{}' ({})", m_iSize, sFilename,
	        bWritable ? (bPrivate ? "rw-private" : "rw-shared") : "ro");
	return true;

#else

	if (bWritable)
	{
		return SetError ("writable memory mapping is not available on this platform");
	}
	return MapFileIntoMemory (sFilename);

#endif

} // AcquireFile

//-----------------------------------------------------------------------------
bool KMemoryRegion::AcquireAnonymous (std::size_t iSize, bool bPrivate)
//-----------------------------------------------------------------------------
{
	Close ();
	ClearError ();

	if (iSize == 0)
	{
		return SetError ("cannot create an anonymous mapping of size 0");
	}

#if defined(DEKAF2_KMEMORYMAP_USE_POSIX) && defined(MAP_ANONYMOUS)

	int   iFlags = MAP_ANONYMOUS | (bPrivate ? MAP_PRIVATE : MAP_SHARED);
	void* pMap   = ::mmap (nullptr, iSize, PROT_READ | PROT_WRITE, iFlags, -1, 0);

	if (pMap != MAP_FAILED)
	{
		m_pMapBase   = pMap;
		m_pData      = static_cast<const char*>(pMap);
		m_iSize      = iSize;
		m_bWritable  = true;
		m_bPrivate   = bPrivate;
		m_bAnonymous = true;

		kDebug (2, "created anonymous mapping of {} bytes ({})", m_iSize, bPrivate ? "private" : "shared");
		return true;
	}

	kDebug (2, "anonymous mmap() of {} bytes failed, falling back to a heap buffer", iSize);

#elif defined(DEKAF2_KMEMORYMAP_USE_WINDOWS)

	ULARGE_INTEGER iMapSize;
	iMapSize.QuadPart = iSize;

	// INVALID_HANDLE_VALUE backs the mapping by the system paging file (anonymous)
	HANDLE hMapping = ::CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
	                                        iMapSize.HighPart, iMapSize.LowPart, nullptr);

	if (hMapping != nullptr)
	{
		DWORD iMapAccess = bPrivate ? FILE_MAP_COPY : FILE_MAP_WRITE;
		void* pView      = ::MapViewOfFile (hMapping, iMapAccess, 0, 0, iSize);

		if (pView != nullptr)
		{
			m_pMapHandle = hMapping;
			m_pMapBase   = pView;
			m_pData      = static_cast<const char*>(pView);
			m_iSize      = iSize;
			m_bWritable  = true;
			m_bPrivate   = bPrivate;
			m_bAnonymous = true;

			kDebug (2, "created anonymous mapping of {} bytes ({})", m_iSize, bPrivate ? "private" : "shared");
			return true;
		}

		::CloseHandle (hMapping);
	}

	kDebug (2, "anonymous CreateFileMapping of {} bytes failed, falling back to a heap buffer", iSize);

#endif

	// fallback: anonymous memory is just RAM, so a private heap buffer serves in-process
	m_sFallback.clear();
	m_sFallback.resize (iSize); // zero initialised
	m_pData      = m_sFallback.data();
	m_iSize      = iSize;
	m_bWritable  = true;
	m_bPrivate   = bPrivate;
	m_bAnonymous = true;
	return true;

} // AcquireAnonymous

//-----------------------------------------------------------------------------
bool KMemoryRegion::Flush ()
//-----------------------------------------------------------------------------
{
	// only a shared, file backed, writable mapping has anything to flush to disk
	if (!m_bWritable || m_bPrivate || m_bAnonymous || m_pMapBase == nullptr)
	{
		return true;
	}

#if defined(DEKAF2_KMEMORYMAP_USE_POSIX)

	if (::msync (m_pMapBase, m_iSize, MS_SYNC) != 0)
	{
		return SetErrnoError ("msync: ");
	}
	return true;

#elif defined(DEKAF2_KMEMORYMAP_USE_WINDOWS)

	if (!::FlushViewOfFile (m_pMapBase, 0))
	{
		return SetError ("FlushViewOfFile failed");
	}
	if (m_pFileHandle != nullptr)
	{
		::FlushFileBuffers (static_cast<HANDLE>(m_pFileHandle));
	}
	return true;

#else

	return true;

#endif

} // Flush

//-----------------------------------------------------------------------------
void KMemoryRegion::Close ()
//-----------------------------------------------------------------------------
{
	// persist a shared writable file mapping before tearing it down
	Flush ();

#if defined(DEKAF2_KMEMORYMAP_USE_POSIX)

	if (m_pMapBase != nullptr)
	{
		::munmap (m_pMapBase, m_iSize);
	}

#elif defined(DEKAF2_KMEMORYMAP_USE_WINDOWS)

	if (m_pMapBase != nullptr)
	{
		::UnmapViewOfFile (m_pMapBase);
	}
	if (m_pMapHandle != nullptr)
	{
		::CloseHandle (static_cast<HANDLE>(m_pMapHandle));
	}
	if (m_pFileHandle != nullptr)
	{
		::CloseHandle (static_cast<HANDLE>(m_pFileHandle));
	}

#endif

	m_pData       = nullptr;
	m_iSize       = 0;
	m_pMapBase    = nullptr;
	m_pMapHandle  = nullptr;
	m_pFileHandle = nullptr;
	m_sFallback.clear ();
	m_bWritable   = false;
	m_bPrivate    = false;
	m_bAnonymous  = false;

} // Close

} // namespace detail

DEKAF2_NAMESPACE_END
