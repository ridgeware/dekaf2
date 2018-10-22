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
// https://github.com/JoachimSchurig/CppCDDB/blob/master/untar.cpp
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

//  I learned about the tar format and the various representations in the wild
//  by reading through the code of the following fine projects:
//
//  https://github.com/abergmeier/tarlib
//
//  and
//
//  https://techoverflow.net/blog/2013/03/29/reading-tar-files-in-c/
//
//  The below code is another implementation, based on that information
//  but following neither of the above implementations in the resulting
//  code, particularly because this is a pure C++11 implementation for untar.
//  So please do not blame those for any errors this code may cause or have.

#include <cstring>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include "kuntar.h"
#include "klog.h"

namespace dekaf2 {

namespace tar {

//-----------------------------------------------------------------------------
void Header::reset()
//-----------------------------------------------------------------------------
{
    if (!m_keep_members_once)
	{
        m_file_size = 0;
        m_modification_time = 0;
        m_filename.clear();
        m_linkname.clear();
        m_is_end = false;
        m_is_ustar = false;
        m_entrytype = Unknown;
    }
    else
	{
		m_keep_members_once = false;
	}

} // reset

//-----------------------------------------------------------------------------
void Header::clear()
//-----------------------------------------------------------------------------
{
	std::memset(raw.header, 0, HeaderLen);
	m_keep_members_once = false;
    reset();

} // clear

//-----------------------------------------------------------------------------
bool Header::Analyze()
//-----------------------------------------------------------------------------
{
    // check for end header
    if (raw.header[0] == 0)
	{
        // check if full header is 0, then this is the end header of an archive
        m_is_end = true;

		for (auto ch : raw.header)
		{
            if (ch)
			{
                m_is_end = false;
                break;
            }
        }

		if (m_is_end)
		{
			return true;
		}
    }

    // validate checksum
    {
        uint64_t header_checksum = std::strtoull(header.checksum, nullptr, 8);
        std::memset(header.checksum, ' ', 8);
        uint64_t sum = 0;
        for (auto ch : raw.header)
		{
			sum += ch;
		}
        if (header_checksum != sum)
		{
			kDebug(2, "invalid header checksum");
			return false;
		}
    }

    m_is_ustar = (memcmp("ustar", header.extension.ustar.indicator, 5) == 0);

    // extract the filename
    m_filename.assign(header.file_name, std::min((size_t)100, ::strnlen(header.file_name, 100)));

    if (m_is_ustar)
	{
        // check if there is a filename prefix
        size_t plen = ::strnlen(header.extension.ustar.filename_prefix, 155);
        // yes, insert it before the existing filename
        if (plen)
		{
            m_filename = KString(header.extension.ustar.filename_prefix,
								 std::min((size_t)155, plen)) + "/" + m_filename;
        }
    }

    if (!m_filename.empty() && *m_filename.rbegin() == '/')
	{
        // make sure we detect also pre-1988 directory names :)
        if (!header.extension.ustar.type_flag)
		{
			header.extension.ustar.type_flag = '5';
		}
    }

    // now analyze the entry type

    switch (header.extension.ustar.type_flag)
	{
        case 0:
        case '0':
            // normal file
            if (m_entrytype == Longname1)
			{
                // this is a subsequent read on GNU tar long names
                // the current block contains only the filename and the next block contains metadata
                // set the filename from the current header
                m_filename.assign(header.file_name,
								  std::min((size_t)HeaderLen, ::strnlen(header.file_name, HeaderLen)));
                // the next header contains the metadata, so replace the header before reading the metadata
                m_entrytype = Longname2;
                m_keep_members_once = true;
                return true;
            }
            m_entrytype = File;

            header.file_bytes_terminator = 0;
            if ((header.file_bytes_octal[0] & 0x80) != 0)
			{
                m_file_size = std::strtoull(header.file_bytes_octal, nullptr, 256);
            }
			else
			{
                m_file_size = std::strtoull(header.file_bytes_octal, nullptr, 8);
            }
            
            header.modification_time_terminator = 0;
            m_modification_time = std::strtoull(header.modification_time_octal, nullptr, 8);
            break;

        case '1':
            // link
            m_entrytype = Link;
            m_linkname.assign(header.extension.ustar.linked_file_name,
							  std::min((size_t)100, ::strnlen(header.file_name, 100)));
            break;

        case '2':
            // symlink
            m_entrytype = Symlink;
            m_linkname.assign(header.extension.ustar.linked_file_name,
							  std::min((size_t)100, ::strnlen(header.file_name, 100)));
            break;

        case '5':
            // directory
            m_entrytype = Directory;
            m_file_size = 0;

            header.modification_time_terminator = 0;
            m_modification_time = std::strtoull(header.modification_time_octal, nullptr, 8);
            break;

        case '6':
            // fifo
            m_entrytype = Fifo;
            break;

        case 'L':
            // GNU long filename
            m_entrytype = Longname1;
            m_keep_members_once = true;
            return true;

        default:
            m_entrytype = Unknown;
            break;
    }

	return true;

} // Header::Analyze

} // end of namespace tar


//-----------------------------------------------------------------------------
KUnTar::KUnTar(KInStream& Stream, int AcceptedTypes, bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: m_Stream(Stream)
	, m_AcceptedTypes(AcceptedTypes)
	, m_bSkipAppleResourceForks(bSkipAppleResourceForks)
{
}

//-----------------------------------------------------------------------------
KUnTar::KUnTar(KStringView sArchiveFilename, int AcceptedTypes, bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: m_File(std::make_unique<KInFile>(sArchiveFilename))
	, m_Stream(*m_File)
	, m_AcceptedTypes(AcceptedTypes)
	, m_bSkipAppleResourceForks(bSkipAppleResourceForks)
{
}

//-----------------------------------------------------------------------------
bool KUnTar::Read(void* buf, size_t len)
//-----------------------------------------------------------------------------
{
	size_t rb = m_Stream.Read(buf, len);

	if (rb != len)
	{
		kDebug(2, "unexpected end of input stream, trying to read {} bytes, but got {}", len, rb);
		return false;
	}

	return true;

} // Read

//-----------------------------------------------------------------------------
size_t KUnTar::CalcPadding()
//-----------------------------------------------------------------------------
{
	if (m_header.Type() == tar::File)
	{
		// check if we have to skip some padding bytes (tar files have a block size of 512)
		return (tar::Header::HeaderLen - (m_header.Filesize() % tar::Header::HeaderLen)) % tar::Header::HeaderLen;
	}

	return 0;

} // CalcPadding

//-----------------------------------------------------------------------------
bool KUnTar::ReadPadding()
//-----------------------------------------------------------------------------
{
	size_t padding = CalcPadding();

	if (padding)
	{
		// this invalidates the (raw) header, but it is a handy buffer to read up to 512
		// bytes into here (and we do not need it anymore)
		if (!Read(*m_header, padding))
		{
			return false;
		}
	}

	return true;

} // ReadPadding

//-----------------------------------------------------------------------------
bool KUnTar::Next()
//-----------------------------------------------------------------------------
{
    do
	{
		if (!m_bIsConsumed && (m_header.Type() == tar::File))
		{
			if (!SkipCurrentFile())
			{
				return false;
			}
		}

        m_header.reset();

		if (!Read(*m_header, tar::Header::HeaderLen))
		{
			return false;
		}

		if (!m_header.Analyze())
		{
			return false;
		}

        // this is the only valid exit condition from reading a tar archive - end header reached
        if (m_header.IsEnd())
		{
			return false;
		}

        if (m_header.Type() == tar::File)
		{
			m_bIsConsumed = false;
        }

    } while ((m_header.Type() & m_AcceptedTypes) == 0
			 || (m_bSkipAppleResourceForks && m_header.Filename().StartsWith("./._")));

    return true;

} // Entry

//-----------------------------------------------------------------------------
bool KUnTar::Skip(size_t iSize)
//-----------------------------------------------------------------------------
{
	if (!iSize)
	{
		return true;
	}

	enum { SKIP_BUFSIZE = 4096 };
	char sBuffer[SKIP_BUFSIZE];
	size_t iRead = 0;

	for (;iSize;)
	{
		auto iChunk = std::min(static_cast<size_t>(SKIP_BUFSIZE), iSize);
		auto iReadChunk = Read(sBuffer, iChunk);
		iRead += iReadChunk;
		iSize -= iReadChunk;

		if (iReadChunk < iChunk)
		{
			break;
		}
	}

	return iRead == iSize;

} // Skip

//-----------------------------------------------------------------------------
bool KUnTar::SkipCurrentFile()
//-----------------------------------------------------------------------------
{
	return Skip(m_header.Filesize() + CalcPadding());

} // SkipCurrentFile

//-----------------------------------------------------------------------------
bool KUnTar::Read(KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	if (m_header.Type() != tar::File)
	{
		kDebug(2, "cannot read - not a file");
		return false;
	}

	if (!OutStream.Write(m_Stream, m_header.Filesize()).Good())
	{
		kDebug(2, "cannot write to stream");
		return false;
	}

	if (!ReadPadding())
	{
		return false;
	}

	m_bIsConsumed = true;

	return true;

} // Read

//-----------------------------------------------------------------------------
bool KUnTar::Read(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (m_header.Type() != tar::File)
	{
		kDebug(2, "cannot read - not a file");
		return false;
	}
	
	// resize the buffer to be able to read the file size
#ifdef DEKAF2_KSTRING_HAS_RESIZE_UNINITIALIZED
	sBuffer.resize(m_header.Filesize(), KString::ResizeUninitialized());
#else
	sBuffer.resize(m_header.Filesize());
#endif

	// read the file into the buffer
	if (!Read(&sBuffer[0], m_header.Filesize()))
	{
		return false;
	}

	if (!ReadPadding())
	{
		return false;
	}

	m_bIsConsumed = true;

	return true;

} // Read

//-----------------------------------------------------------------------------
bool KUnTar::File(KString& sName, KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sName.clear();
	sBuffer.clear();

    if (!Next())
	{
		return false;
	}

	if (m_header.Type() == tar::File)
	{
		if (!Read(sBuffer))
		{
			return false;
		}
	}

    sName = m_header.Filename();

    return true;

} // File


//-----------------------------------------------------------------------------
KUnTarCompressed::KUnTarCompressed(COMPRESSION Compression,
				 KInStream& InStream,
				 int AcceptedTypes,
				 bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: KUnTar(m_FilteredInStream, AcceptedTypes, bSkipAppleResourceForks)
{
	SetupFilter(Compression, InStream);

} // ctor

//-----------------------------------------------------------------------------
KUnTarCompressed::KUnTarCompressed(COMPRESSION Compression,
				 KStringView sArchiveFilename,
				 int AcceptedTypes,
				 bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: KUnTar(m_FilteredInStream, AcceptedTypes, bSkipAppleResourceForks)
{
	m_File = std::make_unique<KInFile>(sArchiveFilename);

	if (Compression == AUTODETECT)
	{
		KString sSuffix = kExtension(sArchiveFilename).ToLower();

		if (sSuffix == "tar")
		{
			Compression = NONE;
		}
		else if (sSuffix == "tgz" || sSuffix == "gz" || sSuffix == "gzip")
		{
			Compression = GZIP;
		}
		else if (sSuffix == "tbz" || sSuffix == "tbz2" || sSuffix == "bz2" || sSuffix == "bzip2")
		{
			Compression = BZIP2;
		}
		else
		{
			Compression = NONE;
		}
	}

	SetupFilter(Compression, *m_File);

} // ctor

//-----------------------------------------------------------------------------
void KUnTarCompressed::SetupFilter(COMPRESSION Compression, KInStream& InStream)
//-----------------------------------------------------------------------------
{
	switch (Compression)
	{
		case AUTODETECT:
		case NONE:
			break;

		case GZIP:
			m_Filter.push(boost::iostreams::gzip_decompressor());
			break;

		case BZIP2:
			m_Filter.push(boost::iostreams::bzip2_decompressor());
			break;
	}

	m_Filter.push(InStream.InStream());

} // SetupFilter

} // end of namespace dekaf2

