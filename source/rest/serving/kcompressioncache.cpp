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

#include <dekaf2/rest/serving/kcompressioncache.h>
#include <dekaf2/io/compression/bits/kiostreams_filters.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/web/url/kurl.h>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace {

/// the source file is read in blocks of this size, and the deadline is checked after each block
constexpr std::size_t BlockSize = 256 * 1024;

/// a compressed file with this percentage of the source size or more is not kept - it would
/// occupy nearly the size of the source in the cache directory for a small gain
constexpr uint64_t NegativePercent = 95;

//-----------------------------------------------------------------------------
/// The compression levels for a source file size. The first two tiers compress within
/// about 2 seconds, the last one at streaming speed. zstd levels up to 19 keep the window
/// at 8 MB or less, which is the maximum for HTTP (RFC 9659).
struct Tier
//-----------------------------------------------------------------------------
{
	uint64_t iMaxSize;
	uint16_t iZstd;
	uint16_t iBrotli;
	uint16_t iGzip;
};

constexpr std::array<Tier, 3> Tiers
{{
	{ 1024 * 1024,                           19, 11, 9 },
	{ 16 * 1024 * 1024,                      12,  9, 9 },
	{ std::numeric_limits<uint64_t>::max(),   3,  5, 6 }
}};

//-----------------------------------------------------------------------------
const Tier& GetTier(uint64_t iSize)
//-----------------------------------------------------------------------------
{
	for (const auto& Tier : Tiers)
	{
		if (iSize <= Tier.iMaxSize)
		{
			return Tier;
		}
	}

	return Tiers.back();

} // GetTier

//-----------------------------------------------------------------------------
/// returns the file extension of an entry, or an empty string if the cache does not support the compression
KStringView GetExtension(KHTTPCompression::COMP Compression)
//-----------------------------------------------------------------------------
{
	switch (Compression)
	{
#ifdef DEKAF2_HAS_LIBZSTD
		case KHTTPCompression::ZSTD:
			return "zst";
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case KHTTPCompression::BROTLI:
			return "br";
#endif
		case KHTTPCompression::GZIP:
			return "gz";

		case KHTTPCompression::ZLIB:
		case KHTTPCompression::BZIP2:
#ifdef DEKAF2_HAS_LIBLZMA
		case KHTTPCompression::XZ:
		case KHTTPCompression::LZMA:
#endif
		case KHTTPCompression::NONE:
		case KHTTPCompression::ALL:
			break;
	}

	return {};

} // GetExtension

//-----------------------------------------------------------------------------
/// adds the compressor with the level for the source size to the filter chain
void PushCompressor(boost::iostreams::filtering_ostream& Filter, KHTTPCompression::COMP Compression, uint64_t iSourceSize)
//-----------------------------------------------------------------------------
{
	const auto& Levels = GetTier(iSourceSize);

	switch (Compression)
	{
#ifdef DEKAF2_HAS_LIBZSTD
		case KHTTPCompression::ZSTD:
			// no worker threads - the calling thread compresses
			Filter.push(dekaf2::iostreams::zstd_compressor(dekaf2::iostreams::zstd_params(Levels.iZstd, 0)));
			break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case KHTTPCompression::BROTLI:
			Filter.push(dekaf2::iostreams::brotli_compressor(dekaf2::iostreams::brotli_params(Levels.iBrotli)));
			break;
#endif
		case KHTTPCompression::GZIP:
			Filter.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(Levels.iGzip)));
			break;

		case KHTTPCompression::ZLIB:
		case KHTTPCompression::BZIP2:
#ifdef DEKAF2_HAS_LIBLZMA
		case KHTTPCompression::XZ:
		case KHTTPCompression::LZMA:
#endif
		case KHTTPCompression::NONE:
		case KHTTPCompression::ALL:
			// not supported, Get() and Lookup() reject these before
			break;
	}

} // PushCompressor

//-----------------------------------------------------------------------------
/// the key of an entry: inode, size, modification time and change time of the source file
KString CreateKey(const KFileStat& Stat)
//-----------------------------------------------------------------------------
{
	auto Nanoseconds = [](KUnixTime tTime)
	{
		return chrono::duration_cast<chrono::nanoseconds>(tTime.time_since_epoch()).count();
	};

	return kFormat("{:x}-{:x}-{:x}-{:x}",
	               Stat.Inode(),
	               Stat.Size(),
	               Nanoseconds(Stat.ModificationTime()),
	               Nanoseconds(Stat.ChangeTime()));

} // CreateKey

//-----------------------------------------------------------------------------
/// removes a leading slash, returns false if the relative path would leave its directory
bool NormalizeRelPath(KStringView& sRelPath)
//-----------------------------------------------------------------------------
{
	sRelPath.remove_prefix('/');

#ifdef DEKAF2_IS_WINDOWS
	// a colon in a Windows path addresses a drive or an alternate data stream
	if (sRelPath.contains(':'))
	{
		return false;
	}
#endif

	return kIsSafeURLPath(sRelPath);

} // NormalizeRelPath

//-----------------------------------------------------------------------------
/// removes all files of an entry directory that do not belong to the key
void RemoveOtherKeys(const KString& sEntryDirectory, KStringView sKey)
//-----------------------------------------------------------------------------
{
	KString sPrefix = sKey;
	sPrefix += '.';

	for (const auto& File : KDirectory(sEntryDirectory, KFileType::FILE))
	{
		if (!File.Filename().starts_with(sPrefix))
		{
			kRemoveFile(File.Path());
		}
	}

} // RemoveOtherKeys

//-----------------------------------------------------------------------------
/// the destinations of the compressed data of a pass: the entry file, and after the
/// start of the transmission also the response
struct Destinations
//-----------------------------------------------------------------------------
{
	KOutStream* pFile     { nullptr };
	KOutStream* pResponse { nullptr };
	uint64_t    iWritten  { 0 };
};

//-----------------------------------------------------------------------------
// the end of the filter chain, writes the compressed data into the destinations - the
// chain copies its devices, therefore the sink only points to the destinations
class DestinationSink : public boost::iostreams::sink
//-----------------------------------------------------------------------------
{
public:
	DestinationSink(Destinations& Dest) : m_Dest(&Dest) {}

	std::streamsize write(const char* sData, std::streamsize iSize)
	{
		auto iCount = static_cast<std::size_t>(iSize);

		m_Dest->pFile->Write(sData, iCount);
		m_Dest->iWritten += iCount;

		if (m_Dest->pResponse && !m_Dest->pResponse->Write(sData, iCount).Good())
		{
			// the client has disconnected - the entry is completed nevertheless
			kDebug(2, "stopping transmission, the response stream failed");
			m_Dest->pResponse = nullptr;
		}

		return iSize;
	}

private:
	Destinations* m_Dest;

}; // DestinationSink

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KCompressionCache::KCompressionCache(KString sCacheDirectory)
//-----------------------------------------------------------------------------
: m_sCacheDirectory(std::move(sCacheDirectory))
{
	// entry paths are built with / as separator, also on Windows
	m_sCacheDirectory.remove_suffix('/');

} // ctor

//-----------------------------------------------------------------------------
KCompressionCache::~KCompressionCache()
//-----------------------------------------------------------------------------
{
	// Cancel() waits for a sweep that is running right now
	for (const auto& Timer : m_SweepTimers)
	{
		Dekaf::getInstance().GetTimer().Cancel(Timer.second);
	}

} // dtor

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KCompressionCache::GetSupportedCompressors()
//-----------------------------------------------------------------------------
{
	return KHTTPCompression::GZIP
#ifdef DEKAF2_HAS_LIBZSTD
	     | KHTTPCompression::ZSTD
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
	     | KHTTPCompression::BROTLI
#endif
	     ;

} // GetSupportedCompressors

//-----------------------------------------------------------------------------
KString KCompressionCache::GetDocumentRootDirectory(KStringView sDocumentRoot) const
//-----------------------------------------------------------------------------
{
	sDocumentRoot.remove_suffix('/');

	// the hash separates the entries of different document roots in one cache directory
	return kFormat("{}/{:x}", m_sCacheDirectory, sDocumentRoot.Hash());

} // GetDocumentRootDirectory

//-----------------------------------------------------------------------------
KString KCompressionCache::GetEntryDirectory(KStringView sDocumentRoot, KStringView sRelPath) const
//-----------------------------------------------------------------------------
{
	auto sDirectory = GetDocumentRootDirectory(sDocumentRoot);

	if (!sRelPath.empty())
	{
		sDirectory += '/';
		sDirectory += sRelPath;
	}

	return sDirectory;

} // GetEntryDirectory

//-----------------------------------------------------------------------------
KCompressionCache::Entry KCompressionCache::Lookup(KStringView             sDocumentRoot,
                                                   KStringView             sRelPath,
                                                   const KFileStat&        SourceStat,
                                                   KHTTPCompression::COMP  Compression) const
//-----------------------------------------------------------------------------
{
	Entry Result;

	auto sExtension = GetExtension(Compression);

	if (sExtension.empty() || !NormalizeRelPath(sRelPath) || sRelPath.empty() || !SourceStat.IsFile())
	{
		kDebug(2, "cannot cache {} with compression {}", sRelPath, KHTTPCompression::ToString(Compression));
		Result.Status = State::Failed;
		return Result;
	}

	auto sEntry = kFormat("{}/{}.{}", GetEntryDirectory(sDocumentRoot, sRelPath), CreateKey(SourceStat), sExtension);

	KFileStat EntryStat(sEntry);

	if (EntryStat.IsFile())
	{
		if (EntryStat.Size() == 0)
		{
			Result.Status = State::Negative;
		}
		else
		{
			Result.Status = State::Hit;
			Result.sPath  = std::move(sEntry);
			Result.iSize  = EntryStat.Size();
		}
	}

	return Result;

} // Lookup

//-----------------------------------------------------------------------------
KCompressionCache::Entry KCompressionCache::Get(KStringView               sDocumentRoot,
                                                KStringView               sRelPath,
                                                const KFileStat&          SourceStat,
                                                KHTTPCompression::COMP    Compression,
                                                KDuration                 Deadline,
                                                const Transmitter&        StartTransmission)
//-----------------------------------------------------------------------------
{
	auto Result = Lookup(sDocumentRoot, sRelPath, SourceStat, Compression);

	if (Result.Status != State::Miss)
	{
		return Result;
	}

	NormalizeRelPath(sRelPath);

	auto sEntryDirectory = GetEntryDirectory(sDocumentRoot, sRelPath);
	auto sKey            = CreateKey(SourceStat);
	auto sEntry          = kFormat("{}/{}.{}", sEntryDirectory, sKey, GetExtension(Compression));

	{
		std::unique_lock<std::mutex> Lock(m_Mutex);

		if (m_Running.contains(sEntry))
		{
			// another thread compresses this entry - wait for it, but not longer than the
			// deadline, and never start a second pass
			auto IsDone = [this, &sEntry]() { return !m_Running.contains(sEntry); };

			if (StartTransmission)
			{
				m_Finished.wait_for(Lock, Deadline, IsDone);
			}
			else
			{
				m_Finished.wait(Lock, IsDone);
			}

			bool bDone = IsDone();

			Lock.unlock();

			if (bDone)
			{
				Result = Lookup(sDocumentRoot, sRelPath, SourceStat, Compression);

				if (Result.Status == State::Hit || Result.Status == State::Negative)
				{
					return Result;
				}
			}

			Result.Status = State::Busy;
			return Result;
		}

		m_Running.insert(sEntry);
	}

	auto RemoveRunning = [this, &sEntry]()
	{
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_Running.erase(sEntry);
		}

		m_Finished.notify_all();
	};

	KScopeGuard<decltype(RemoveRunning)> Guard(RemoveRunning);

	if (!kCreateDir(sEntryDirectory))
	{
		kDebug(1, "cannot create cache directory: {}", sEntryDirectory);
		Result.Status = State::Failed;
		return Result;
	}

	// several processes can share the cache directory: an advisory lock on a lock file
	// excludes a second pass - the operating system releases the lock when a process ends
	auto sLock = sEntry + ".lock";

	kTouchFile(sLock);

	{
		KFileLock FileLock(sLock, KFileLock::Exclusive, /*bWait=*/false);

		if (!FileLock)
		{
			kDebug(2, "another process compresses {}", sEntry);
			Result.Status = State::Busy;
			return Result;
		}

		// another process may have completed the entry in the meantime
		Result = Lookup(sDocumentRoot, sRelPath, SourceStat, Compression);

		if (Result.Status == State::Miss)
		{
			Result = Compress(kFormat("{}/{}", sDocumentRoot, sRelPath),
			                  SourceStat,
			                  sEntryDirectory,
			                  sKey,
			                  Compression,
			                  Deadline,
			                  StartTransmission);
		}
	}

	// this fails on Windows while another process has the lock file open, which is harmless
	kRemoveFile(sLock);

	return Result;

} // Get

//-----------------------------------------------------------------------------
KCompressionCache::Entry KCompressionCache::Compress(const KString&          sSourcePath,
                                                     const KFileStat&        SourceStat,
                                                     const KString&          sEntryDirectory,
                                                     const KString&          sKey,
                                                     KHTTPCompression::COMP  Compression,
                                                     KDuration               Deadline,
                                                     const Transmitter&      StartTransmission)
//-----------------------------------------------------------------------------
{
	Entry Result;

	auto sEntry    = kFormat("{}/{}.{}", sEntryDirectory, sKey, GetExtension(Compression));
	auto sTemp     = sEntry + ".tmp";
	// without a transmitter the deadline is not used
	auto tDeadline = StartTransmission ? chrono::steady_clock::now() + chrono::nanoseconds(Deadline) : chrono::steady_clock::time_point::max();

	KInFile  Source(sSourcePath);
	KOutFile TempFile(sTemp, std::ios::trunc);

	if (!Source.is_open() || !TempFile.is_open())
	{
		kDebug(1, "cannot open {} or {}", sSourcePath, sTemp);
		kRemoveFile(sTemp);
		Result.Status = State::Failed;
		return Result;
	}

	Destinations Dest;
	Dest.pFile = &TempFile;

	bool bTransmitterCalled { false };
	bool bTransmitting      { false };
	bool bOK                { true  };

	auto StartTransmissionWhenDue = [&]()
	{
		if (bTransmitterCalled || !StartTransmission || chrono::steady_clock::now() < tDeadline)
		{
			return;
		}

		bTransmitterCalled = true;

		auto* pResponse = StartTransmission();

		if (!pResponse)
		{
			return;
		}

		bTransmitting = true;

		// send the compressed data that the entry file already contains
		if (Dest.iWritten)
		{
			TempFile.Flush();
			KInFile Written(sTemp);
			pResponse->Write(Written, static_cast<std::size_t>(Dest.iWritten));
		}

		Dest.pResponse = pResponse->Good() ? pResponse : nullptr;
	};

	uint64_t iRead { 0 };

	try
	{
		boost::iostreams::filtering_ostream Filter;

		PushCompressor(Filter, Compression, SourceStat.Size());
		Filter.push(DestinationSink(Dest));

		std::vector<char> Buffer(BlockSize);

		for (;;)
		{
			StartTransmissionWhenDue();

			auto iBlock = Source.Read(Buffer.data(), Buffer.size());

			if (!iBlock)
			{
				break;
			}

			iRead += iBlock;

			if (!Filter.write(Buffer.data(), static_cast<std::streamsize>(iBlock)))
			{
				bOK = false;
				break;
			}
		}

		// closes the chain, which writes the rest of the compressed data
		Filter.reset();
	}
	catch (const std::exception& ex)
	{
		kDebug(1, "compressing {} failed: {}", sSourcePath, ex.what());
		bOK = false;
	}

	bOK = bOK && TempFile.Good();
	TempFile.close();

	// the source file must not have changed while it was compressed, otherwise the entry
	// would not fit its key
	if (bOK && (iRead != SourceStat.Size() || CreateKey(KFileStat(sSourcePath)) != sKey))
	{
		kDebug(1, "{} changed while it was compressed", sSourcePath);
		bOK = false;
	}

	if (!bOK)
	{
		kRemoveFile(sTemp);
		// with a started transmission the response is incomplete
		Result.Status = bTransmitting ? State::Aborted : State::Failed;
		return Result;
	}

	bool bNegative = Dest.iWritten * 100 >= SourceStat.Size() * NegativePercent;

	if (bNegative)
	{
		// an entry of size 0 marks a file that does not get smaller
		KOutFile Empty(sTemp, std::ios::trunc);
	}

	if (!kRename(sTemp, sEntry))
	{
		kDebug(1, "cannot rename {} to {}", sTemp, sEntry);
		kRemoveFile(sTemp);
		Result.Status = bTransmitting ? State::Transmitted : State::Failed;
		return Result;
	}

	RemoveOtherKeys(sEntryDirectory, sKey);

	kDebug(2, "compressed {} with {}: {} -> {} bytes", sSourcePath, KHTTPCompression::ToString(Compression), SourceStat.Size(), Dest.iWritten);

	if (bTransmitting)
	{
		Result.Status = State::Transmitted;
		Result.iSize  = Dest.iWritten;
	}
	else if (bNegative)
	{
		Result.Status = State::Negative;
	}
	else
	{
		Result.Status = State::Hit;
		Result.sPath  = std::move(sEntry);
		Result.iSize  = Dest.iWritten;
	}

	return Result;

} // Compress

//-----------------------------------------------------------------------------
void KCompressionCache::Forget(KStringView sDocumentRoot, KStringView sRelPath)
//-----------------------------------------------------------------------------
{
	if (!NormalizeRelPath(sRelPath))
	{
		return;
	}

	// an empty relative path forgets the whole document root
	auto sDirectory = GetEntryDirectory(sDocumentRoot, sRelPath);

	if (kDirExists(sDirectory))
	{
		kDebug(2, "removing {}", sDirectory);
		kRemoveDir(sDirectory);
	}

} // Forget

//-----------------------------------------------------------------------------
std::size_t KCompressionCache::Sweep(KStringView sDocumentRoot)
//-----------------------------------------------------------------------------
{
	std::size_t iRemoved { 0 };

	auto sRootDirectory = GetDocumentRootDirectory(sDocumentRoot);

	if (!kDirExists(sRootDirectory))
	{
		return iRemoved;
	}

	for (const auto& File : KDirectory(sRootDirectory, KFileType::FILE, /*bRecursive=*/true))
	{
		KString sFile = File.Path();
#ifdef DEKAF2_IS_WINDOWS
		// KDirectory joins its paths with the native separator
		sFile.Replace('\\', '/');
#endif
		KStringView sRelPath = sFile;
		KStringView sName    = File.Filename();

		// the directory of an entry is the path of its source file
		if (!sRelPath.remove_prefix(sRootDirectory) ||
		    !sRelPath.remove_prefix('/') ||
		    !sRelPath.remove_suffix(sName) ||
		    !sRelPath.remove_suffix('/'))
		{
			// not an entry
			if (kRemoveFile(File.Path()))
			{
				++iRemoved;
			}
			continue;
		}

		// temporary and lock files of the current key belong to a running pass
		KStringView sKey = sName;
		sKey.erase(sKey.find('.'));

		KFileStat SourceStat(kFormat("{}/{}", sDocumentRoot, sRelPath));

		if (!SourceStat.IsFile() || CreateKey(SourceStat) != sKey)
		{
			if (kRemoveFile(File.Path()))
			{
				++iRemoved;
			}
		}
	}

	// remove the empty directories, the deepest first
	std::vector<KString> Directories;

	for (const auto& Directory : KDirectory(sRootDirectory, KFileType::DIRECTORY, /*bRecursive=*/true))
	{
		Directories.push_back(Directory.Path());
	}

	std::sort(Directories.begin(), Directories.end(), [](const KString& a, const KString& b)
	{
		return a.size() > b.size();
	});

	Directories.push_back(sRootDirectory);

	for (const auto& sDirectory : Directories)
	{
		if (KDirectory(sDirectory).empty())
		{
			kRemoveDir(sDirectory);
		}
	}

	kDebug(2, "removed {} files from {}", iRemoved, sRootDirectory);

	return iRemoved;

} // Sweep

//-----------------------------------------------------------------------------
void KCompressionCache::SweepRegularly(KString sDocumentRoot, KDuration Interval)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	if (m_SweepTimers.contains(sDocumentRoot))
	{
		return;
	}

	auto ID = Dekaf::getInstance().GetTimer().CallEvery(Interval, [this, sDocumentRoot](KUnixTime)
	{
		Sweep(sDocumentRoot);
	});

	m_SweepTimers.emplace(std::move(sDocumentRoot), ID);

} // SweepRegularly

DEKAF2_NAMESPACE_END
