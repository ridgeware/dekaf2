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

#include <cinttypes>
#include <vector>
#include <memory>
#include <boost/iostreams/filtering_stream.hpp>

#include "kstring.h"
#include "kreader.h"
#include "kwriter.h"

namespace dekaf2 {

namespace tar {

enum EntryType
{
	Unknown   = 0,
	File      = 1,
	Directory = 2,
	Link      = 4,
	Symlink   = 8,
	Fifo      = 16,
	Longname1 = 32,
	Longname2 = 64,
	All       = 127
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Representation of the header structure of a tar archive
class Header
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum { HeaderLen = 512 };

    Header() { clear(); }

    void clear();
    void reset();
    char* operator*() { return raw.header; }
    bool Analyze();
    bool IsEnd() const { return m_is_end; }
    EntryType Type() const { return m_entrytype; }
    bool IsFile() const { return Type() == File; }
    bool IsDirectory() const { return Type() == Directory; }
    const KString& Filename() const { return m_filename; }
    const KString& Linkname() const { return m_linkname; }
    size_t Filesize() const { return m_file_size; }

//----------
private:
//----------

	/// the header structure
    union {
        struct {
            char header[HeaderLen];
        } raw;
        struct {
            char file_name[100];
            char mode[8];
            struct {
                char user[8];
                char group[8];
            } owner_ids;

            char file_bytes_octal[11];
            char file_bytes_terminator;
            char modification_time_octal[11];
            char modification_time_terminator;

            char checksum[8];

            union {
                struct {
                    char link_indicator;
                    char linked_file_name[100];
                } legacy;
                struct {
                    char type_flag;
                    char linked_file_name[100];
                    char indicator[6];
                    char version[2];
                    struct {
                        char user[32];
                        char group[32];
                    } owner_names;
                    struct {
                        char major[8];
                        char minor[8];
                    } device;
                    char filename_prefix[155];
                } ustar;
            } extension;
        } header;
    };

    uint64_t m_file_size;
    uint64_t m_modification_time;
    KString m_filename;
    KString m_linkname;
    bool m_is_end;
    bool m_is_ustar;
    EntryType m_entrytype;
    bool m_keep_members_once;

}; // Header

} // end of namespace tar


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver for uncompressed archives
class KUnTar
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an open stream
	KUnTar(KInStream& Stream,
		   int AcceptedTypes = tar::File,
		   bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name
	KUnTar(KStringView sArchiveFilename,
		   int AcceptedTypes = tar::File,
		   bool bSkipAppleResourceForks = false);

    /// Simple interface: call for subsequent files, with sBuffer getting filled
	/// with the file's data. Returns false if end of archive. Additional properties
	/// can be requested by the other property methods.
    bool File(KString& sName, KString& sBuffer);

    /// Advance to next entry in an archive, returns false at error or end of archive
	bool Next();

    /// Get type of the current file
	tar::EntryType Type() const { return m_header.Type(); }

    /// Get name of the current file (when the type is File, Link or Symlink)
    const KString& Filename() const { return m_header.Filename(); }

	/// Get link name of the current file (when the type is Link or Symlink)
    const KString& Linkname() const { return m_header.Linkname(); }

	/// Get size in bytes of the current file (when the type is File)
	uint64_t Filesize() const { return m_header.Filesize(); }

	/// Read content of the current file into a KOutStream (when the type is File)
	bool Read(KOutStream& OutStream);

	/// Read content of the current file into a KString (when the type is File)
	bool Read(KString& sBuffer);

//----------
private:
//----------

	size_t CalcPadding();
	bool ReadPadding();
	bool Skip(size_t iSize);
	bool SkipCurrentFile();
	bool Read(void* buf, size_t len);

	std::unique_ptr<KInFile> m_File;
	KInStream& m_Stream;
	int m_AcceptedTypes;
	bool m_bSkipAppleResourceForks;
	mutable bool m_bIsConsumed { true };
	tar::Header m_header;

}; // KUnTar


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Tar unarchiver that can read GZip or BZip2 compressed archives
class KUnTarCompressed : public KUnTar
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum COMPRESSION
	{
		NONE,
		GZIP,
		BZIP2,
		AUTODETECT
	};

	/// Construct around an open stream. Compression type options are NONE, GZIP, BZIP2.
	KUnTarCompressed(COMPRESSION Compression,
					 KInStream& InStream,
					 int AcceptedTypes = tar::File,
					 bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name. Compression type options are NONE, GZIP, BZIP2,
	/// AUTODETECT. If AUTODETECT, compression will be set from the file name suffix.
	KUnTarCompressed(COMPRESSION Compression,
					 KStringView sArchiveFilename,
					 int AcceptedTypes = tar::File,
					 bool bSkipAppleResourceForks = false);

	/// Construct from an archive file name. Compression will be set from the file name suffix.
	KUnTarCompressed(KStringView sArchiveFilename,
					 int AcceptedTypes = tar::File,
					 bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(AUTODETECT, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

//----------
private:
//----------

	void SetupFilter(COMPRESSION Compression, KInStream& InStream);

	boost::iostreams::filtering_istream m_Filter;
	KInStream m_FilteredInStream { m_Filter };
	std::unique_ptr<KInFile> m_File;

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
			   int AcceptedTypes = tar::File,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(GZIP, InStream, AcceptedTypes, bSkipAppleResourceForks)
	{}

	/// Construct from an archive file name
	KUnTarGZip(KStringView sArchiveFilename,
			   int AcceptedTypes = tar::File,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(GZIP, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
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
			   int AcceptedTypes = tar::File,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(BZIP2, InStream, AcceptedTypes, bSkipAppleResourceForks)
	{}

	/// Construct from an archive file name
	KUnTarBZip2(KStringView sArchiveFilename,
			   int AcceptedTypes = tar::File,
			   bool bSkipAppleResourceForks = false)
	: KUnTarCompressed(BZIP2, sArchiveFilename, AcceptedTypes, bSkipAppleResourceForks)
	{}

}; // KUnTarBZip2


} // end of namespace dekaf2
