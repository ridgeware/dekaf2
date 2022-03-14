/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

// large portions of this code are taken from
// https://github.com/JoachimSchurig/CppCDDB/blob/master/untar.hpp
// which is under a BSD style open source license

//  Copyright © 2016 Joachim Schurig. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
*/

#pragma once

#include "bits/ktarheader.h"

#include "kstring.h"
#include "kreader.h"
#include "kwriter.h"
#include "kfilesystem.h"
#include "kcompression.h"

#include <cinttypes>
#include <vector>
#include <memory>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver for uncompressed archives
class DEKAF2_PUBLIC KUnTar
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class iterator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		using iterator_category = std::forward_iterator_tag;
		using value_type        = KUnTar;
		using pointer           = value_type*;
		using reference         = value_type&;

		iterator(KUnTar* UnTar = nullptr) noexcept;

		reference operator*() const;

		pointer operator->() const
		{
			return & operator*();
		}

		// post increment
		iterator& operator++() noexcept;

		bool operator==(const iterator& other) const noexcept
		{
			return m_UnTar == other.m_UnTar;
		}

		bool operator!=(const iterator& other) const noexcept
		{
			return !operator==(other);
		}

	//------
	private:
	//------

		KUnTar* m_UnTar { nullptr };

	}; // iterator

	using const_iterator = iterator;

	/// Construct around an open stream
	KUnTar(KInStream& Stream,
		   int AcceptedTypes = tar::All,
		   bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name
	KUnTar(KStringView sArchiveFilename,
		   int AcceptedTypes = tar::All,
		   bool bSkipAppleResourceForks = false);

	/// reads all files and directories in an archive
	/// @param sTargetDirectory directory into which the archive will be expanded.
	/// If it does not exist it will be created.
	/// @param bWithSubdirectories create subdirectories on extraction, or write
	/// all files into a flat hierarchy. Default true.
	bool ReadAll(KStringViewZ sTargetDirectory, bool bWithSubdirectories = true);

    /// Simple interface: call for subsequent files, with sBuffer getting filled
	/// with the file's data. Returns false if end of archive. Additional properties
	/// can be requested by the other property methods.
    bool File(KStringRef& sName, KStringRef& sBuffer);

    /// Advance to next entry in an archive, returns false at error or end of archive
	bool Next();

    /// Get name of the current file (when the type is File, Link or Symlink)
    const KString& Filename() const
	{
		return m_Header.m_sFilename;
	}

	/// Return a sanitized file name (no path, no escaping, no special characters)
	KString SafeName() const
	{
		return kMakeSafeFilename(kBasename(Filename()), false);
	}

	/// Return a sanitized path name (no escaping, no special characters)
	KString SafePath() const
	{
		return kMakeSafePathname(Filename(), false);
	}

	/// Get link name of the current file (when the type is Link or Symlink)
    const KString& Linkname() const
	{
		return m_Header.m_sLinkname;
	}

	/// Return a sanitized file name (no path, no escaping, no special characters)
	KString SafeLinkName() const
	{
		return kMakeSafeFilename(kBasename(Linkname()), false);
	}

	/// Return a sanitized path name (no escaping, no special characters)
	KString SafeLinkPath() const
	{
		return kMakeSafePathname(Linkname(), false);
	}

	/// Get size in bytes of the current file (when the type is File)
	uint64_t Filesize() const
	{
		return m_Header.m_iFilesize;
	}

	/// Returns the type of the current tar entry
	tar::EntryType Type() const
	{
		return m_Header.m_EntryType;
	}

	/// Returns true if current tar entry type is File
	bool IsFile() const
	{
		return Type() == tar::File;
	}

	/// Returns true if current tar entry type is Directory
	bool IsDirectory() const
	{
		return Type() == tar::Directory;
	}

	/// Returns true if current tar entry type is a hard link
	bool IsHardlink() const
	{
		return Type() == tar::Hardlink;
	}

	/// Returns true if current tar entry type is a symbolic link
	bool IsSymlink() const
	{
		return Type() == tar::Symlink;
	}

	/// Returns the owner name of the tar entry
	const KString& User() const
	{
		return m_Header.m_sUser;
	}

	/// Returns the group name of the tar entry
	const KString& Group() const
	{
		return m_Header.m_sGroup;
	}

	/// Returns the owner ID of the tar entry
	uint32_t UserID() const
	{
		return m_Header.m_iUserId;
	}

	/// Returns the group ID of the tar entry
	uint32_t GroupID() const
	{
		return m_Header.m_iGroupId;
	}

	/// Returns the file mode of the tar entry
	uint32_t Mode() const
	{
		return m_Header.m_iMode;
	}

	/// Returns the modification time of the tar entry
	time_t ModificationTime() const
	{
		return m_Header.m_tModificationTime;
	}

	/// Read content of the current file into a KOutStream (when the type is File)
	bool Read(KOutStream& OutStream);

	/// Read content of the current file into a KString (when the type is File)
	bool Read(KStringRef& sBuffer);

	/// Read content of the current file into a file sFilename
	bool ReadFile(KStringViewZ sFilename);

	/// Returns error description on failure
	const KString& Error() const
	{
		return m_Error;
	}

	iterator begin() noexcept
	{
		return iterator(this);
	}

	const_iterator begin() const noexcept
	{
		return const_iterator(const_cast<KUnTar*>(this));
	}

	const_iterator cbegin() const noexcept
	{
		return const_iterator(const_cast<KUnTar*>(this));
	}

	iterator end() noexcept
	{
		return iterator();
	}

	const_iterator end() const noexcept
	{
		return const_iterator();
	}

	const_iterator cend() const noexcept
	{
		return const_iterator();
	}

//----------
private:
//----------

	DEKAF2_PRIVATE
	size_t  CalcPadding();
	DEKAF2_PRIVATE
	bool    ReadPadding();
	DEKAF2_PRIVATE
	bool    Skip(size_t iSize);
	DEKAF2_PRIVATE
	bool    SkipCurrentFile();
	DEKAF2_PRIVATE
	bool    Read(void* buf, size_t len);
	DEKAF2_PRIVATE
	bool    SetError(KString sError);
	DEKAF2_PRIVATE
	KString CreateTargetDirectory(KStringViewZ sBaseDir, KStringViewZ sEntry, bool bWithSubdirectories);

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Decoded
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		void     clear();
		void     Reset();
		uint64_t FromNumbers(const char* pStart, uint16_t iSize);
		bool     Decode(const tar::TarHeader& TarHeader);
		bool     IsEnd() const
		{
			return m_bIsEnd;
		}

		KString   m_sFilename;
		KString   m_sLinkname;
		KString   m_sUser;
		KString   m_sGroup;
		uint32_t  m_iMode;
		uint32_t  m_iUserId;
		uint32_t  m_iGroupId;
		uint64_t  m_iFilesize;
		time_t    m_tModificationTime;
		tar::EntryType m_EntryType    { tar::Unknown };

	//----------
	private:
	//----------

		bool      m_bIsEnd            { false };
		bool      m_bIsUstar          { false };
		bool      m_bKeepMembersOnce  { false };

	}; // Decoded

	Decoded                  m_Header;
	KString                  m_Error;
	std::unique_ptr<KInFile> m_File;
	KInStream&               m_Stream;
	int                      m_AcceptedTypes;
	bool                     m_bSkipAppleResourceForks;
	mutable bool             m_bIsConsumed { true };

}; // KUnTar


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver that can read GZip or BZip2 compressed archives
class KUnTarCompressed : public KUnTar
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an open stream. Compression type options are NONE, GZIP, BZIP2.
	KUnTarCompressed(KUnCompressIStream::COMPRESSION Compression,
					 KInStream& InStream,
					 int AcceptedTypes = tar::All,
					 bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name. Compression type options are NONE, GZIP, BZIP2,
	/// AUTODETECT. If AUTODETECT, compression will be set from the file name suffix.
	KUnTarCompressed(KUnCompressIStream::COMPRESSION Compression,
					 KStringView sArchiveFilename,
					 int AcceptedTypes = tar::All,
					 bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name. Compression will be set from the file name suffix.
	KUnTarCompressed(KStringView sArchiveFilename,
					 int AcceptedTypes = tar::All,
					 bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::AUTO, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

//----------
private:
//----------

	KUnCompressIStream                  m_Filter;
	KInStream                           m_FilteredInStream { m_Filter };
	std::unique_ptr<KInFile>            m_File;

}; // KUnTarCompressed


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver that can read GZip compressed archives
class KUnTarGZip : public KUnTarCompressed
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an open stream
	KUnTarGZip(KInStream& InStream,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::GZIP, InStream, AcceptedTypes, bSkipAppleResourceForks)
	{}

	/// Construct from an archive file name
	KUnTarGZip(KStringView sArchiveFilename,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::GZIP, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

}; // KUnTarGZip


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver that can read BZip2 compressed archives
class KUnTarBZip2 : public KUnTarCompressed
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an open stream
	KUnTarBZip2(KInStream& InStream,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::BZIP2, InStream, AcceptedTypes, bSkipAppleResourceForks)
	{}

	/// Construct from an archive file name
	KUnTarBZip2(KStringView sArchiveFilename,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::BZIP2, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

}; // KUnTarBZip2

#ifdef DEKAF2_HAS_LIBZSTD
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver that can read ZSTD compressed archives
class KUnTarZstd : public KUnTarCompressed
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an open stream
	KUnTarZstd(KInStream& InStream,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::ZSTD, InStream, AcceptedTypes, bSkipAppleResourceForks)
	{}

	/// Construct from an archive file name
	KUnTarZstd(KStringView sArchiveFilename,
			   int AcceptedTypes = tar::All,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(KUnCompressIStream::ZSTD, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

}; // KUnTarZstd
#endif

} // end of namespace dekaf2
