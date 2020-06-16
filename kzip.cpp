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
#include <zip.h>

namespace dekaf2 {

//-----------------------------------------------------------------------------
inline zip* pZip(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<zip*>(p);
}

//-----------------------------------------------------------------------------
auto zipDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	zip_close(static_cast<zip_t*>(data));
};

//-----------------------------------------------------------------------------
bool KZip::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	kDebug (2, sError);
	
	if (m_bThrow)
	{
		throw KError(std::move(sError));
	}
	else
	{
		m_sError = std::move(sError);
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
bool KZip::Open(KStringViewZ sFilename, bool bWrite)
//-----------------------------------------------------------------------------
{
	int iError;

	auto* zip = zip_open(sFilename.c_str(), bWrite ? ZIP_CREATE : ZIP_RDONLY, &iError);

	if (!zip)
	{
		SetError(iError);
	}

	D = unique_void_ptr(zip, zipDeleter);

	return D.get();

} // Open

//-----------------------------------------------------------------------------
bool KZip::Contains(KStringViewZ sName) const noexcept
//-----------------------------------------------------------------------------
{
	return zip_name_locate(pZip(D.get()), sName.c_str(), 0) >= 0;

} // Contains

//-----------------------------------------------------------------------------
bool KZip::DirEntry::from_zip_stat(const void* vzip_stat)
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

	auto* stat = static_cast<const struct zip_stat*>(vzip_stat);

	if (!stat)
	{
		return false;
	}

	sName             = stat->name;
	iIndex            = stat->index;
	iSize             = stat->size;
	iCompSize         = stat->comp_size;
	mtime             = stat->mtime;
	iCRC              = stat->crc;
	iCompMethod       = stat->comp_method;
	iEncryptionMethod = stat->encryption_method;

	return true;

} // from_zip_stat

//-----------------------------------------------------------------------------
struct KZip::DirEntry KZip::DirEntry(std::size_t iIndex) const
//-----------------------------------------------------------------------------
{
	struct DirEntry File;
	struct zip_stat stat;

	if (zip_stat_index(pZip(D.get()), iIndex, 0, &stat) < 0)
	{
		if (m_bThrow)
		{
			throw std::runtime_error(zip_strerror(pZip(D.get())));
		}
		else
		{
			m_sError = zip_strerror(pZip(D.get()));
		}
	}
	else
	{
		// convert
		File.from_zip_stat(&stat);
	}

	return File;

}  // DirEntry

//-----------------------------------------------------------------------------
struct KZip::DirEntry KZip::DirEntry(KStringViewZ sName) const
//-----------------------------------------------------------------------------
{
	struct DirEntry File;
	struct zip_stat stat;

	if (zip_stat(pZip(D.get()), sName.c_str(), 0, &stat) < 0)
	{
		if (m_bThrow)
		{
			throw std::runtime_error(zip_strerror(pZip(D.get())));
		}
		else
		{
			m_sError = zip_strerror(pZip(D.get()));
		}
	}
	else
	{
		// convert
		File.from_zip_stat(&stat);
	}

	return File;

} // DirEntry

//-----------------------------------------------------------------------------
std::size_t KZip::size() const noexcept
//-----------------------------------------------------------------------------
{
	return zip_get_num_entries(pZip(D.get()), 0);

} // size

//-----------------------------------------------------------------------------
bool KZip::Read(KOutStream& OutStream, const struct DirEntry& DirEntry)
//-----------------------------------------------------------------------------
{
	if (DirEntry.sName.empty())
	{
		return SetError("KZip: no file specified for reading");
	}

	std::array<char, 4096> Buffer;

	struct zip_file* file;

	if (!m_sPassword.empty())
	{
		file = zip_fopen_index_encrypted(pZip(D.get()), DirEntry.iIndex, 0, m_sPassword.c_str());
	}
	else
	{
		file = zip_fopen_index(pZip(D.get()), DirEntry.iIndex, 0);
	}

	auto File = std::make_unique<std::unique_ptr<struct zip_file, int (*)(struct zip_file*)>>(file, zip_fclose);

	if (File.get() == nullptr)
	{
		return SetError(zip_strerror(pZip(D.get())));
	}

	for (;;)
	{
		auto iRead = zip_fread(file,
							   Buffer.data(),
							   static_cast<zip_uint64_t>(Buffer.size()));

		if (iRead > 0)
		{
			OutStream.Write(Buffer.data(), iRead);
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
bool KZip::Read(KStringViewZ sFileName, const struct DirEntry& DirEntry)
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
KString KZip::Read(const struct DirEntry& DirEntry)
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

} // end of namespace dekaf2
