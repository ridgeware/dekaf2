/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include "kzip.h"
#include "klog.h"
#include "kexception.h"
#include "koutstringstream.h"
#include "kfilesystem.h"
#include <zip.h>

namespace dekaf2 {

#if (LIBZIP_VERSION_MAJOR > 1) || (LIBZIP_VERSION_MAJOR == 1 && LIBZIP_VERSION_MINOR >= 7)
	#define DEKAF2_HAVE_LIBZIP_COMPRESSION_METHOD_SUPPORTED 1
#endif

//-----------------------------------------------------------------------------
inline zip* pZip(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<zip*>(p);
}

//-----------------------------------------------------------------------------
inline zip* pZip(const KUniqueVoidPtr& p)
//-----------------------------------------------------------------------------
{
	return static_cast<zip*>(p.get());
}

//-----------------------------------------------------------------------------
auto zipDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	if (data)
	{
		if (zip_close(pZip(data)))
		{
			kWarning("cannot properly close ZIP archive - changes are discarded");
			zip_discard(pZip(data));
		}
	}

}; // zipDeleter

//-----------------------------------------------------------------------------
bool KZip::DirEntry::IsDirectory() const
//-----------------------------------------------------------------------------
{
	return !sName.empty() && *sName.rbegin() == '/';

} // IsDirectory

//-----------------------------------------------------------------------------
void KZip::DirEntry::clear()
//-----------------------------------------------------------------------------
{
	sName.clear();
	iIndex    = 0;
	iSize     = 0;
	iCompSize = 0;
	mtime     = 0;

} // clear

//-----------------------------------------------------------------------------
uint16_t KZip::DirEntry::PercentCompressed() const
//-----------------------------------------------------------------------------
{
	if (iSize)
	{
		return iCompSize * 100 / iSize;
	}
	else
	{
		return 100;
	}

} // PercentCompressed

//-----------------------------------------------------------------------------
bool KZip::DirEntry::from_zip_stat(const struct zip_stat* stat)
//-----------------------------------------------------------------------------
{
	/*
	struct zip_stat
	{
		zip_uint64_t valid;             // which fields have valid values
		const char *_Nullable name;     // name of the file
		zip_uint64_t index;             // index within archive
		zip_uint64_t size;              // size of file (uncompressed)
		zip_uint64_t comp_size;         // size of file (compressed)
		time_t mtime;                   // modification time
		zip_uint32_t crc;               // crc of file data
		zip_uint16_t comp_method;       // compression method used
		zip_uint16_t encryption_method; // encryption method used
		zip_uint32_t flags;             // reserved for future use
	};
	*/

	if (!stat)
	{
		clear();
		return false;
	}

	sName      = stat->name;
	iIndex     = stat->index;
	iSize      = stat->size;
	iCompSize  = stat->comp_size;
	mtime      = stat->mtime;

	switch (stat->comp_method)
	{
		case ZIP_CM_STORE:
			Compression = CompMethod::NONE;
			break;

		case ZIP_CM_DEFLATE:
		case ZIP_CM_DEFLATE64:
			Compression = CompMethod::DEFLATE;
			break;

#ifdef DEKAF2_HAVE_LIBZIP_COMPRESSION_METHOD_SUPPORTED
		case 12: //ZIP_CM_BZIP2
			Compression = CompMethod::BZIP2;
			break;

		case 95: // ZIP_CM_XZ
			Compression = CompMethod::XZ;
			break;

		case 93: // ZIP_CM_ZSTD
			Compression = CompMethod::ZSTD;
			break;
#endif
		default:
			Compression = CompMethod::OTHER;
			break;
	}

	return true;

} // from_zip_stat

//-----------------------------------------------------------------------------
KZip::iterator::iterator(KZip& Zip, uint64_t iIndex) noexcept
//-----------------------------------------------------------------------------
: m_Zip(&Zip)
, m_iIndex(iIndex)
{
	if (iIndex < m_Zip->size())
	{
		m_DirEntry = m_Zip->Get(iIndex);
	}

} // ctor

//-----------------------------------------------------------------------------
KZip::iterator::reference KZip::iterator::operator*() const
//-----------------------------------------------------------------------------
{
	if (m_iIndex <= m_Zip->size())
	{
		return m_DirEntry;
	}
	else
	{
		throw KError("KZip::iterator out of range");
	}

} // operator*

//-----------------------------------------------------------------------------
KZip::iterator& KZip::iterator::operator++() noexcept
//-----------------------------------------------------------------------------
{
	if (++m_iIndex < m_Zip->size())
	{
		m_DirEntry = m_Zip->Get(m_iIndex);
	}

	return *this;

} // operator++

//-----------------------------------------------------------------------------
KZip::iterator KZip::iterator::operator++(int) noexcept
//-----------------------------------------------------------------------------
{
	iterator Copy = *this;

	if (++m_iIndex < m_Zip->size())
	{
		m_DirEntry = m_Zip->Get(m_iIndex);
	}

	return Copy;

} // operator++(int)

//-----------------------------------------------------------------------------
KZip::iterator& KZip::iterator::operator--() noexcept
//-----------------------------------------------------------------------------
{
	if (--m_iIndex < m_Zip->size())
	{
		m_DirEntry = m_Zip->Get(m_iIndex);
	}

	return *this;

} // operator--

//-----------------------------------------------------------------------------
KZip::iterator KZip::iterator::operator--(int) noexcept
//-----------------------------------------------------------------------------
{
	iterator Copy = *this;

	if (--m_iIndex < m_Zip->size())
	{
		m_DirEntry = m_Zip->Get(m_iIndex);
	}

	return Copy;

} // operator--(int)

//-----------------------------------------------------------------------------
bool KZip::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	kDebug (2, sError);

	m_sError = std::move(sError);

	if (m_bThrow)
	{
		throw KError(kFormat("KZip: {}", m_sError));
	}

	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KZip::SetError(int iError) const
//-----------------------------------------------------------------------------
{
	std::array<char, 128> Buffer;

	zip_error_to_str(Buffer.data(), Buffer.size(), iError, errno);

	return SetError(KString(Buffer.data()));

} // SetError

//-----------------------------------------------------------------------------
bool KZip::SetError() const
//-----------------------------------------------------------------------------
{
	return SetError(zip_strerror(pZip(D)));

} // SetError

//-----------------------------------------------------------------------------
KZip::~KZip()
//-----------------------------------------------------------------------------
{
	// make sure we delete the archive first, as that triggers a zip_close(),
	// which needs the buffers in m_WriteBuffers
	Close();

} // dtor

//-----------------------------------------------------------------------------
bool KZip::Open(KStringViewZ sFilename, bool bWrite)
//-----------------------------------------------------------------------------
{
	int iError;

	auto* zip = zip_open(sFilename.c_str(), bWrite ? ZIP_CREATE : ZIP_RDONLY, &iError);

	if (!zip)
	{
		SetError(iError);
	}

	D = KUniqueVoidPtr(zip, zipDeleter);

	// do not clear before assigning a new zip - it could trigger the closure of
	// the previous one, accessing on the buffers!
	m_WriteBuffers.clear();

	return D.get();

} // Open

//-----------------------------------------------------------------------------
void KZip::Close()
//-----------------------------------------------------------------------------
{
	D.reset();

} // Close

//-----------------------------------------------------------------------------
bool KZip::Contains(KStringViewZ sName, bool bNoPathCompare) const noexcept
//-----------------------------------------------------------------------------
{
	return zip_name_locate(pZip(D), sName.c_str(), bNoPathCompare ? ZIP_FL_NODIR : 0) >= 0;

} // Contains

//-----------------------------------------------------------------------------
struct KZip::DirEntry KZip::Get(std::size_t iIndex) const
//-----------------------------------------------------------------------------
{
	DirEntry File;
	struct zip_stat stat;

	if (zip_stat_index(pZip(D), iIndex, 0, &stat) < 0)
	{
		SetError();
	}
	else
	{
		// convert
		File.from_zip_stat(&stat);
	}

	return File;

}  // Get

//-----------------------------------------------------------------------------
struct KZip::DirEntry KZip::Get(KStringViewZ sName, bool bNoPathCompare) const
//-----------------------------------------------------------------------------
{
	DirEntry File;
	struct zip_stat stat;

	if (zip_stat(pZip(D), sName.c_str(), bNoPathCompare ? ZIP_FL_NODIR : 0, &stat) < 0)
	{
		SetError();
	}
	else
	{
		// convert
		File.from_zip_stat(&stat);
	}

	return File;

} // Get

//-----------------------------------------------------------------------------
std::size_t KZip::size() const noexcept
//-----------------------------------------------------------------------------
{
	return zip_get_num_entries(pZip(D), 0);

} // size

//-----------------------------------------------------------------------------
KZip::Directory KZip::Files() const
//-----------------------------------------------------------------------------
{
	Directory Files;

	for (std::size_t i = 0; i < size(); ++i)
	{
		auto Entry = Get(i);

		if (!Entry.IsDirectory())
		{
			Files.push_back(std::move(Entry));
		}
	}

	return Files;

} // Files

//-----------------------------------------------------------------------------
KZip::Directory KZip::Directories() const
//-----------------------------------------------------------------------------
{
	Directory Directories;

	for (std::size_t i = 0; i < size(); ++i)
	{
		auto Entry = Get(i);

		if (Entry.IsDirectory())
		{
			Directories.push_back(std::move(Entry));
		}
	}

	return Directories;

} // Directories

//-----------------------------------------------------------------------------
KZip::Directory KZip::FilesAndDirectories() const
//-----------------------------------------------------------------------------
{
	Directory FilesAndDirectories;
	FilesAndDirectories.reserve(size());

	for (std::size_t i = 0; i < size(); ++i)
	{
		FilesAndDirectories.push_back(Get(i));
	}

	return FilesAndDirectories;

} // FilesAndDirectories

//-----------------------------------------------------------------------------
bool KZip::Read(KOutStream& OutStream, const DirEntry& DirEntry)
//-----------------------------------------------------------------------------
{
	if (DirEntry.sName.empty())
	{
		return SetError("KZip: no file specified for reading");
	}

	if (DirEntry.IsDirectory())
	{
		return SetError(kFormat("KZip: file is directory: {}", DirEntry.sName));
	}

	std::array<char, KDefaultCopyBufSize> Buffer;

	struct zip_file* file;

	if (!m_sPassword.empty())
	{
		file = zip_fopen_index_encrypted(pZip(D), DirEntry.iIndex, 0, m_sPassword.c_str());
	}
	else
	{
		file = zip_fopen_index(pZip(D), DirEntry.iIndex, 0);
	}

	auto File = std::make_unique<std::unique_ptr<struct zip_file, int (*)(struct zip_file*)>>(file, zip_fclose);

	if (File.get() == nullptr)
	{
		return SetError();
	}

	for (;;)
	{
		auto iRead = zip_fread(file,
							   Buffer.data(),
							   static_cast<zip_uint64_t>(Buffer.size()));

		if (iRead > 0)
		{
			if (!OutStream.Write(Buffer.data(), iRead).Good())
			{
				return false;
			}
		}

		if (iRead < static_cast<int64_t>(Buffer.size()))
		{
			// we're done
			return true;
		}

		if (iRead < 0)
		{
			break;
		}
	}

	return SetError(kFormat("could not read file: {}", DirEntry.sName));

} // Read

//-----------------------------------------------------------------------------
bool KZip::Read(KStringViewZ sFileName, const DirEntry& DirEntry)
//-----------------------------------------------------------------------------
{
	KOutFile OutFile(sFileName);

	if (!OutFile.is_open())
	{
		return SetError(kFormat("cannot open file: {}", sFileName));
	}

	return Read(OutFile, DirEntry);

} // Read

//-----------------------------------------------------------------------------
KString KZip::Read(const DirEntry& DirEntry)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	sBuffer.reserve(DirEntry.iSize);

	KOutStringStream oss(sBuffer);

	if (!Read(oss, DirEntry))
	{
		sBuffer.clear();
	}

	return sBuffer;

} // Read

//-----------------------------------------------------------------------------
bool KZip::Read(const Directory& Directory, KStringViewZ sTargetDirectory, bool bWithSubdirectories)
//-----------------------------------------------------------------------------
{
	if (!kDirExists(sTargetDirectory))
	{
		if (!kCreateDir(sTargetDirectory))
		{
			return SetError(kFormat("cannot create directory: {}", sTargetDirectory));
		}
	}

	for (const auto& File : Directory)
	{
		if (File.IsDirectory())
		{
			if (bWithSubdirectories)
			{
				KString sName = sTargetDirectory;
				sName += kDirSep;
				sName += File.SafePath();

				if (!kMakeDir(sName))
				{
					return SetError(kFormat("cannot create directory: {}", sName));
				}
			}
		}
		else
		{
			KString sName = sTargetDirectory;
			sName += kDirSep;

			if (bWithSubdirectories)
			{
				KString sSafePath = File.SafePath();
				KStringView sDirname = kDirname(sSafePath);
				if (sDirname != ".")
				{
					sName += sDirname;

					if (!kDirExists(sName))
					{
						if (!kCreateDir(sName))
						{
							return SetError(kFormat("cannot create directory: {}", sName));
						}
					}

					sName += kDirSep;
				}
				sName += kBasename(sSafePath);
			}
			else
			{
				sName += File.SafeName();
			}

			if (!Read(sName, File))
			{
				// error is already set
				return false;
			}
		}
	}

	return true;

} // Read

//-----------------------------------------------------------------------------
bool KZip::ReadAll(KStringViewZ sTargetDirectory, bool bWithSubdirectories)
//-----------------------------------------------------------------------------
{
	return Read(FilesAndDirectories(), sTargetDirectory, bWithSubdirectories);

} // ReadAll

//-----------------------------------------------------------------------------
uint16_t KZip::CompMethodToZipInt(CompMethod Compression)
//-----------------------------------------------------------------------------
{
	switch (Compression)
	{
		case NONE:    return ZIP_CM_STORE;
		case DEFLATE: return ZIP_CM_DEFAULT;
#ifdef ZIP_CM_BZIP2
		case BZIP2:   return ZIP_CM_BZIP2;
#endif
#ifdef ZIP_CM_XZ
		case XZ:      return ZIP_CM_XZ;
#endif
#ifdef ZIP_CM_ZSTD
		case ZSTD:    return ZIP_CM_ZSTD;
#endif
		case OTHER:   return ZIP_CM_DEFAULT;
	}

	// gcc ..
	return ZIP_CM_DEFAULT;

} // CompMethodToZipInt

//-----------------------------------------------------------------------------
bool KZip::HaveCompression(CompMethod Compression)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_LIBZIP_COMPRESSION_METHOD_SUPPORTED
	return zip_compression_method_supported(CompMethodToZipInt(Compression), 0) == 0;
#else
	return false;
#endif

} // HaveCompression

//-----------------------------------------------------------------------------
uint16_t KZip::ScaleCompressionLevel(uint16_t iLevel, uint16_t iMax)
//-----------------------------------------------------------------------------
{
	iLevel = (iLevel > 100) ? 100 : iLevel;
	uint16_t iScaled = iMax * iLevel / 100;
	iScaled += (iScaled) ? 0 : 1;
	return iScaled;

} // ScaleLevel

//-----------------------------------------------------------------------------
bool KZip::SetCompression(CompMethod Compression, uint16_t iCompressionLevelInPercent)
//-----------------------------------------------------------------------------
{
	m_iCompressionLevel = 0;

	if (!HaveCompression(Compression))
	{
		kDebug(1, "compression method {} is not supported", Compression);
		m_Compression = CompMethod::DEFLATE;
		return false;
	}
	else
	{
		m_Compression = Compression;

		if (iCompressionLevelInPercent > 0)
		{
			// calc compression level between min and max for the algorithm
			m_iCompressionLevel = ScaleCompressionLevel(iCompressionLevelInPercent, (Compression == ZSTD) ? 19 : 9);
		}
		return true;
	}
	
} // SetCompression

//-----------------------------------------------------------------------------
bool KZip::HaveStrongEncryption() const
//-----------------------------------------------------------------------------
{
#ifdef ZIP_EM_AES_256
	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
bool KZip::SetEncryptionForFile(uint64_t iIndex)
//-----------------------------------------------------------------------------
{
#ifdef ZIP_EM_AES_256
#if (LIBZIP_VERSION_MAJOR > 1) || (LIBZIP_VERSION_MAJOR == 1 && LIBZIP_VERSION_MINOR >= 7)
	if (zip_encryption_method_supported(ZIP_EM_AES_256, 0))
#else
	if (true)
#endif
	{
		if (zip_file_set_encryption(pZip(D), iIndex, ZIP_EM_AES_256, m_sPassword.c_str()) == -1)
		{
			return SetError();
		}
	}
	else
#endif // of ifdef ZIP_EM_AES_256
	{
		return SetError("AES_256 encryption is not supported");
	}

	return true;

} // SetEncryptionForFile

//-----------------------------------------------------------------------------
bool KZip::SetCompressionForFile(uint64_t iIndex)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_LIBZIP_COMPRESSION_METHOD_SUPPORTED
	if (zip_set_file_compression(pZip(D), iIndex, CompMethodToZipInt(m_Compression), m_iCompressionLevel) == 0)
	{
		return true;
	}
	return SetError();
#else
	return false;
#endif

} // SetCompressionForFile

//-----------------------------------------------------------------------------
bool KZip::WriteBuffer(KStringView sBuffer, KStringViewZ sDispname)
//-----------------------------------------------------------------------------
{
	kDebug(2, "adding buffer with name {}", sDispname);

	// we need to copy all data into a temporary buffer until
	// the archive is closed because the architecture of libzip
	// only stores data when destructing
	auto pBuffer = std::make_unique<char[]>(sBuffer.size());

	auto* Source = zip_source_buffer(pZip(D), pBuffer.get(), sBuffer.length(), 0);

	if (!Source)
	{
		return SetError();
	}

	std::memcpy(pBuffer.get(), sBuffer.data(), sBuffer.size());

	auto iIndex = zip_file_add(pZip(D), sDispname.c_str(), Source, ZIP_FL_OVERWRITE);

	if (iIndex < 0)
	{
		zip_source_free(Source);
		return SetError();
	}

#ifdef DEKAF2_HAVE_LIBZIP_COMPRESSION_METHOD_SUPPORTED
	if (m_Compression != CompMethod::DEFLATE || m_iCompressionLevel > 0)
	{
		SetCompressionForFile(iIndex);
	}
#endif

	if (!m_sPassword.empty())
	{
		SetEncryptionForFile(iIndex);
	}

	m_WriteBuffers.push_back(std::move(pBuffer));

	return true;

} // WriteBuffer

//-----------------------------------------------------------------------------
bool KZip::WriteFile(KStringViewZ sFilename, KStringViewZ sDispname)
//-----------------------------------------------------------------------------
{
	kDebug(2, "adding file '{}' with name {}", sFilename, sDispname);

	if (sFilename.empty())
	{
		return SetError("missing file name");
	}

	auto* Source = zip_source_file(pZip(D), sFilename.c_str(), 0, -1);

	if (!Source)
	{
		return SetError();
	}

	if (sDispname.empty())
	{
		auto pos = sFilename.find_last_of(detail::kAllowedDirSep);

		if (pos != KStringView::npos)
		{
			sDispname = sFilename.substr(pos + 1);
		}
		else
		{
			sDispname = sFilename;
		}
	}

	auto iIndex = zip_file_add(pZip(D), sDispname.c_str(), Source, ZIP_FL_OVERWRITE);

	if (iIndex < 0)
	{
		zip_source_free(Source);
		return SetError();
	}

	if (!m_sPassword.empty())
	{
		SetEncryptionForFile(iIndex);
	}

	return true;

} // Write

//-----------------------------------------------------------------------------
bool KZip::Write(KInStream& InStream, KStringViewZ sDispname)
//-----------------------------------------------------------------------------
{
	return WriteBuffer (kReadAll(InStream), sDispname);

} // Write

//-----------------------------------------------------------------------------
bool KZip::WriteDirectory(KStringViewZ sDispname)
//-----------------------------------------------------------------------------
{
	kDebug(2, "adding directory: {}", sDispname);

	auto iIndex = zip_dir_add(pZip(D), sDispname.c_str(), 0);

	if (iIndex < 0)
	{
		return SetError();
	}

	return true;

} // AddDirectory

//-----------------------------------------------------------------------------
bool KZip::WriteFiles(const KDirectory& Directory, KStringView sDirectoryRoot, KStringView sNewRoot)
//-----------------------------------------------------------------------------
{
	sDirectoryRoot.Trim("/\\");
	sNewRoot.Trim("/\\");

	for (const auto& File : Directory)
	{
		// construct the name as it shall appear in the archive
		KString sDispName;

		sDispName += sNewRoot;

		if (!sNewRoot.empty())
		{
			sDispName += kDirSep;
		}

		// search for directory root in file's path
		auto iPos = File.Path().find(sDirectoryRoot);

		if (iPos < 2 && (iPos == 0 || File.Path()[0] == kDirSep))
		{
			// found either at pos 0 or 1, in the latter case we remove
			// the slash
			sDispName += File.Path().Mid(iPos + sDirectoryRoot.size() + 1);
		}
		else
		{
			// directory root not found, remove leading slash
			// if existing
			if (File.Path().front() == kDirSep)
			{
				sDispName += File.Path().Mid(1);
			}
			else
			{
				sDispName += File.Path();
			}
		}

		if (File.Type() == KFileType::DIRECTORY)
		{
			if (!WriteDirectory(sDispName))
			{
				return false;
			}
		}
		else if (File.Type() == KFileType::FILE)
		{
			// now add the file to the archive
			if (!WriteFile(File.Path(), sDispName))
			{
				return false;
			}
		}
		else
		{
			kDebug(1, "cannot include file type {} of file {} in archive", File.Serialize(), File.Path());
		}
	}

	return true;

} // WriteFiles

//-----------------------------------------------------------------------------
bool KZip::WriteFiles(KStringViewZ sSourceDirectory, KStringView sNewRoot, bool bRecursive, bool bSorted)
//-----------------------------------------------------------------------------
{
	KDirectory Directory(sSourceDirectory, KFileTypes::ALL, bRecursive);

	if (bSorted)
	{
		Directory.Sort();
	}

	return WriteFiles(Directory, sSourceDirectory, sNewRoot);

} // WriteFiles


} // end of namespace dekaf2
