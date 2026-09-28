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

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/http/protocol/khttpcompression.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <condition_variable>
#include <functional>
#include <mutex>

/// @file kcompressioncache.h
/// a cache for compressed variants of static files

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup rest_serving
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps compressed variants of static files in a cache directory outside of the document
/// root, so that every version of a file is compressed only once. An entry stays valid as
/// long as its source file keeps inode, size, modification time and change time - a changed
/// source file gets a new entry at its next request.
///
/// @code
/// KCompressionCache Cache("/var/cache/www");
///
/// KFileStat Stat("/var/www/js/app.js");
///
/// auto Entry = Cache.Get("/var/www", "js/app.js", Stat, KHTTPCompression::ZSTD, chrono::seconds(3), nullptr);
///
/// if (Entry.Status == KCompressionCache::State::Hit)
/// {
///     // send the file Entry.sPath with "Content-Encoding: zstd" and "Content-Length: <Entry.iSize>"
/// }
/// @endcode
///
/// Get() compresses a missing entry in the calling thread. When this takes longer than the
/// deadline, Get() calls the transmitter, sends the compressed data into the stream that the
/// transmitter returns, and completes the entry at the same time. Other threads that ask for
/// the same entry meanwhile wait until their own deadline, and do not compress the file a
/// second time. All methods are thread safe, and several processes can share one cache
/// directory.
///
/// The cache directory has the layout <cache>/<document root id>/<relative path>/<key>.<ext>,
/// with the key built from inode, size, modification time and change time of the source
/// file, and ext one of zst, br and gz. An entry of size 0 marks a file that does not get
/// smaller by the compression.
class DEKAF2_PUBLIC KCompressionCache
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the state of a cache entry
	enum class State
	{
		Miss,        ///< there is no entry
		Hit,         ///< the entry exists, see Entry::sPath and Entry::iSize
		Negative,    ///< the compression does not pay off for this file, send it uncompressed
		Busy,        ///< another thread or process compresses the file, send it uncompressed
		Failed,      ///< the compression failed, send the file uncompressed
		Transmitted, ///< Get() has sent the compressed data into the stream of the transmitter
		Aborted      ///< the compression failed after the transmission started, the response is incomplete
	};

	/// the result of Lookup() and Get()
	struct Entry
	{
		State    Status { State::Miss };
		KString  sPath;        ///< the file system path of the entry, with State::Hit
		uint64_t iSize  { 0 }; ///< the size of the entry, with State::Hit and State::Transmitted
	};

	/// Get() calls the transmitter once when the deadline has passed. It sends the response
	/// headers and returns the stream for the compressed data, or nullptr to complete the
	/// entry without transmission.
	using Transmitter = std::function<KOutStream*()>;

	//-----------------------------------------------------------------------------
	/// @param sCacheDirectory the directory for the cache entries, created when needed
	KCompressionCache(KString sCacheDirectory);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// stops the regular sweeps
	~KCompressionCache();
	//-----------------------------------------------------------------------------

	KCompressionCache(const KCompressionCache&) = delete;
	KCompressionCache& operator=(const KCompressionCache&) = delete;

	//-----------------------------------------------------------------------------
	/// returns the compressions that an entry can have: zstd, br and gzip, if built in
	static KHTTPCompression::COMP GetSupportedCompressors();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the entry of a source file for a compression, without compressing the file
	/// @param sDocumentRoot the document root of the source file
	/// @param sRelPath the path of the source file relative to the document root, with / as separator
	/// @param SourceStat the status of the source file
	/// @param Compression the compression of the entry
	DEKAF2_NODISCARD
	Entry Lookup(KStringView sDocumentRoot, KStringView sRelPath, const KFileStat& SourceStat, KHTTPCompression::COMP Compression) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the entry of a source file for a compression, and compresses the file if the entry is missing
	/// @param sDocumentRoot the document root of the source file
	/// @param sRelPath the path of the source file relative to the document root, with / as separator
	/// @param SourceStat the status of the source file
	/// @param Compression the compression of the entry
	/// @param Deadline the time after which the transmission starts, and until which another thread waits for the entry
	/// @param StartTransmission the transmitter, nullptr to compress the file completely before returning
	DEKAF2_NODISCARD
	Entry Get(KStringView               sDocumentRoot,
	          KStringView               sRelPath,
	          const KFileStat&          SourceStat,
	          KHTTPCompression::COMP    Compression,
	          KDuration                 Deadline,
	          const Transmitter&        StartTransmission);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// removes the entries of a source file, or of all files in a directory tree
	/// @param sDocumentRoot the document root of the source file
	/// @param sRelPath the path of the file or directory relative to the document root, with / as separator
	void Forget(KStringView sDocumentRoot, KStringView sRelPath);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// removes the entries of missing or changed source files, and empty directories
	/// @param sDocumentRoot the document root whose entries are checked
	/// @return the count of removed files
	std::size_t Sweep(KStringView sDocumentRoot);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// calls Sweep() for a document root in regular intervals, in a thread of the global timer -
	/// a second call for the same document root does nothing
	/// @param sDocumentRoot the document root whose entries are checked
	/// @param Interval the time between two sweeps, the first sweep runs after one interval
	void SweepRegularly(KString sDocumentRoot, KDuration Interval);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the cache directory
	const KString& GetCacheDirectory() const { return m_sCacheDirectory; }
	//-----------------------------------------------------------------------------

//------
private:
//------

	DEKAF2_PRIVATE
	KString GetDocumentRootDirectory(KStringView sDocumentRoot) const;
	DEKAF2_PRIVATE
	KString GetEntryDirectory(KStringView sDocumentRoot, KStringView sRelPath) const;
	DEKAF2_PRIVATE
	Entry Compress(const KString& sSourcePath,
	               const KFileStat& SourceStat,
	               const KString& sEntryDirectory,
	               const KString& sKey,
	               KHTTPCompression::COMP Compression,
	               KDuration Deadline,
	               const Transmitter& StartTransmission);

	KString                 m_sCacheDirectory;
	std::mutex              m_Mutex;
	std::condition_variable m_Finished;
	// the paths of the entries that are compressed right now
	KUnorderedSet<KString>  m_Running;
	// the timers of the regular sweeps, by document root
	KUnorderedMap<KString, KTimer::ID_t> m_SweepTimers;

}; // KCompressionCache

/// @}

DEKAF2_NAMESPACE_END
