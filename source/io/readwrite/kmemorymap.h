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
// |/|   Software without restriction, including without limitation        |\|
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

#pragma once

/// @file kmemorymap.h
/// memory mapping of files (read-only or read/write) and anonymous mappings,
/// with a transparent fallback where memory mapping is unavailable. Read access
/// is offered through KConstMemoryMap, read/write access through KMemoryMap.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/containers/sequential/kbuffer.h>
#include <cstddef>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup io_readwrite
/// @{

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Shared storage and platform machinery behind KConstMemoryMap and KMemoryMap.
/// Owns the mapped region (or, where mapping is unavailable, a private buffer)
/// and exposes read-only access to it. Not used directly - construct a
/// KConstMemoryMap (read) or a KMemoryMap (read/write) instead.
class DEKAF2_PUBLIC KMemoryRegion : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// not copyable - a mapping is a unique resource
	KMemoryRegion (const KMemoryRegion&)            = delete;
	KMemoryRegion& operator= (const KMemoryRegion&) = delete;

	/// movable
	KMemoryRegion (KMemoryRegion&& other) noexcept;
	KMemoryRegion& operator= (KMemoryRegion&& other) noexcept;

	~KMemoryRegion ()
	{
		Close ();
	}

	/// release the mapping (or buffer), flushing first if it is a shared writable file mapping
	void Close ();

	/// is a region currently mapped?
	DEKAF2_NODISCARD
	bool is_open () const                 { return m_pData != nullptr; }

	/// pointer to the first byte (nullptr if not open)
	DEKAF2_NODISCARD
	const char* data () const             { return m_pData; }

	/// size of the region in bytes
	DEKAF2_NODISCARD
	std::size_t size () const             { return m_iSize; }

	/// is the region empty (not open, or a zero byte file)?
	DEKAF2_NODISCARD
	bool empty () const                   { return m_iSize == 0; }

	/// read-only view over the entire region (binary safe, not NUL terminated)
	DEKAF2_NODISCARD
	KStringView ToView () const           { return KStringView (m_pData, m_iSize); }

	/// read-only KConstBuffer over the entire region (to interface with a C API)
	DEKAF2_NODISCARD
	KConstBuffer ToConstBuffer () const   { return KConstBuffer (m_pData, m_iSize); }

//----------
protected:
//----------

	KMemoryRegion () = default;

	/// map a file - read-only or read/write, the latter shared or private (copy on write)
	bool AcquireFile      (KStringViewZ sFilename, bool bWritable, bool bPrivate);
	/// create an anonymous (not file backed) writable mapping of iSize zero bytes
	bool AcquireAnonymous (std::size_t iSize, bool bPrivate);
	/// flush a shared writable file mapping back to disk (no-op otherwise)
	bool Flush ();
	/// base address for writable access, or nullptr if the region is read-only
	DEKAF2_NODISCARD
	void* MutableBase ();

	std::size_t m_iSize { 0 }; ///< accessible to KMemoryMap::Buffer()

//----------
private:
//----------

	/// read the whole file into m_sFallback - the read-only fallback when mapping is unavailable
	bool MapFileIntoMemory (KStringViewZ sFilename);

	const char* m_pData        { nullptr }; ///< first byte of the region
	void*       m_pMapBase     { nullptr }; ///< mmap()/MapViewOfFile() base, to release
	void*       m_pMapHandle   { nullptr }; ///< Windows file mapping HANDLE (unused on POSIX)
	void*       m_pFileHandle  { nullptr }; ///< Windows file HANDLE (unused on POSIX / anonymous)
	KString     m_sFallback;                ///< owns memory for the read-into-memory / anon fallback
	bool        m_bWritable    { false };   ///< region is writable
	bool        m_bPrivate     { false };   ///< MAP_PRIVATE / copy on write - writes do not reach the file
	bool        m_bAnonymous   { false };   ///< not backed by a file

}; // KMemoryRegion

} // namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read-only memory mapping of a file. Lookups read straight from the mapped
/// pages, so several threads and processes share a single copy. Errors are
/// reported through KErrorBase (Error() / HasError()).
class DEKAF2_PUBLIC KConstMemoryMap : public detail::KMemoryRegion
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor, does not open a file
	KConstMemoryMap () = default;

	/// constructs and immediately maps the given file read-only - check is_open() / Error()
	explicit KConstMemoryMap (KStringViewZ sFilename)
	{
		Open (sFilename);
	}

	/// map a file read-only into memory
	/// @return true on success, false on error (see Error())
	bool Open (KStringViewZ sFilename)
	{
		return AcquireFile (sFilename, false, false);
	}

}; // KConstMemoryMap

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read/write memory mapping of a file, or an anonymous (not file backed)
/// writable mapping. The writable bytes are exposed as a KBuffer; reads still
/// work through the inherited KConstMemoryMap interface (ToView() etc.).
class DEKAF2_PUBLIC KMemoryMap : public detail::KMemoryRegion
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// how writes relate to the backing file: Shared writes through to the file,
	/// Private keeps them in a copy on write copy and leaves the file unchanged
	enum class Sharing { Shared, Private };

	/// default constructor, does not open a file
	KMemoryMap () = default;

	/// constructs and immediately maps the given file read/write - check is_open() / Error()
	explicit KMemoryMap (KStringViewZ sFilename, Sharing Share = Sharing::Shared)
	{
		Open (sFilename, Share);
	}

	/// map a file read/write into memory
	/// @param Share Shared writes through to the file, Private is copy on write
	/// @return true on success, false on error (see Error())
	bool Open (KStringViewZ sFilename, Sharing Share = Sharing::Shared)
	{
		return AcquireFile (sFilename, true, Share == Sharing::Private);
	}

	/// create an anonymous (not file backed) writable mapping of iSize zero bytes
	/// @param iSize size of the mapping in bytes
	/// @param Share Shared can be inherited across fork(), Private is copy on write
	/// @return the mapping, which has is_open() == false on error (see Error())
	DEKAF2_NODISCARD
	static KMemoryMap Anonymous (std::size_t iSize, Sharing Share = Sharing::Shared)
	{
		KMemoryMap Map;
		Map.AcquireAnonymous (iSize, Share == Sharing::Private);
		return Map;
	}

	/// mutable, non-owning view over the entire region (a null buffer if not open).
	/// Write through CharData()/UInt8Data() or the KBuffer fill interface; for a
	/// shared file mapping call Sync() (or Close()) to flush the changes to disk.
	DEKAF2_NODISCARD
	KBuffer Buffer ()
	{
		auto* pBase = MutableBase ();

		if (pBase == nullptr)
		{
			return KBuffer ();
		}

		KBuffer Buf (pBase, m_iSize);
		Buf.resize (m_iSize); // the whole region is live content
		return Buf;
	}

	/// flush pending writes of a shared file mapping back to disk (no-op for
	/// anonymous, copy on write, or read-only regions)
	bool Sync ()
	{
		return Flush ();
	}

}; // KMemoryMap

/// @}

DEKAF2_NAMESPACE_END
