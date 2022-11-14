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

#include "kuntar.h"
#include "kfilesystem.h"
#include "kstringutils.h"
#include "klog.h"
#include "kexception.h"

#include <cstring>
#include <array>

namespace dekaf2 {

KUnTar::iterator::iterator(KUnTar* UnTar) noexcept
: m_UnTar(UnTar)
{
	operator++();
}

//-----------------------------------------------------------------------------
KUnTar::iterator::reference KUnTar::iterator::operator*() const
//-----------------------------------------------------------------------------
{
	if (m_UnTar)
	{
		return *m_UnTar;
	}
	else
	{
		throw KError("KUnTar::iterator out of range");
	}
}

//-----------------------------------------------------------------------------
KUnTar::iterator& KUnTar::iterator::operator++() noexcept
//-----------------------------------------------------------------------------
{
	if (m_UnTar)
	{
		if (m_UnTar->Next() == false)
		{
			m_UnTar = nullptr;
		}
	}
	
	return *this;

} // operator++

//-----------------------------------------------------------------------------
void KUnTar::Decoded::Reset()
//-----------------------------------------------------------------------------
{
	if (!m_bKeepMembersOnce)
	{
		m_sFilename.clear();
		m_sLinkname.clear();
		m_sUser.clear();
		m_sGroup.clear();
		m_iMode             = 0;
		m_iUserId           = 0;
		m_iGroupId          = 0;
		m_iFilesize         = 0;
		m_tModificationTime = 0;
		m_bIsEnd            = false;
		m_bIsUstar          = false;
		m_EntryType         = tar::Unknown;
	}
	else
	{
		m_bKeepMembersOnce = false;
	}

} // Reset

//-----------------------------------------------------------------------------
void KUnTar::Decoded::clear()
//-----------------------------------------------------------------------------
{
	m_bKeepMembersOnce = false;
	Reset();

} // clear

//-----------------------------------------------------------------------------
uint64_t KUnTar::Decoded::FromNumbers(const char* pStart, uint16_t iSize)
//-----------------------------------------------------------------------------
{
	// check if the null byte came before the end of the array
	iSize = static_cast<uint16_t>(::strnlen(pStart, iSize));

	KStringView sView(pStart, iSize);

	auto chFirst = sView.front();

	if (DEKAF2_UNLIKELY(m_bIsUstar && ((chFirst & 0x80) != 0)))
	{
		// MSB is set, this is a binary encoding
		if (DEKAF2_UNLIKELY((chFirst & 0x7F) != 0))
		{
			// create a local copy with removed MSB bit
			KString sBuffer(sView);
			sBuffer[0] = (chFirst & 0x7f);
			return kToInt<uint64_t>(sBuffer, 256);
		}
		else
		{
			sView.remove_prefix(1);
			return kToInt<uint64_t>(sView, 256);
		}
	}
	else
	{
		// this is an octal encoding
		return kToInt<uint64_t>(sView, 8);
	}

} // FromNumbers

#if (__GNUC__ > 10)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
//-----------------------------------------------------------------------------
bool KUnTar::Decoded::Decode(const tar::TarHeader& Tar)
//-----------------------------------------------------------------------------
{
	Reset();

	// check for end header
    if (Tar.raw[0] == 0)
	{
        // check if full header is 0, then this is the end header of an archive
        m_bIsEnd = true;

		for (auto ch : Tar.raw)
		{
            if (ch)
			{
                m_bIsEnd = false;
                break;
            }
        }

		if (m_bIsEnd)
		{
			return true;
		}
    }

	// keep this on top before calling FromNumbers(), which has a dependency
	m_bIsUstar = (std::memcmp("ustar", Tar.header.extension.ustar.indicator, 5) == 0);

    // validate checksum
    {
        uint64_t header_checksum = FromNumbers(Tar.header.checksum, 8);
        uint64_t sum { 0 };

		// we calculate with unsigned chars first, as that is what today's
		// tars typically do

		for (uint16_t iPos = 0; iPos < 148; ++iPos)
		{
			sum += static_cast<uint8_t>(Tar.raw[iPos]);
		}

		for (uint16_t iPos = 0; iPos < 8; ++iPos)
		{
			sum += ' ';
		}

		for (uint16_t iPos = 148 + 8; iPos < tar::HeaderLen; ++iPos)
		{
			sum += static_cast<uint8_t>(Tar.raw[iPos]);
		}

		if (header_checksum != sum)
		{
			// try again with signed chars, which were used by some implementations

			sum = 0;

			for (uint16_t iPos = 0; iPos < 148; ++iPos)
			{
				sum += Tar.raw[iPos];
			}

			for (uint16_t iPos = 0; iPos < 8; ++iPos)
			{
				sum += ' ';
			}

			for (uint16_t iPos = 148 + 8; iPos < tar::HeaderLen; ++iPos)
			{
				sum += Tar.raw[iPos];
			}

			if (header_checksum != sum)
			{
				// that failed, too
				kDebug(2, "invalid header checksum");
				return false;
			}
		}
    }

	m_iMode     = static_cast<uint32_t>(FromNumbers(Tar.header.mode, 8));
	m_iUserId   = static_cast<uint32_t>(FromNumbers(Tar.header.owner_ids.user,  8));
	m_iGroupId  = static_cast<uint32_t>(FromNumbers(Tar.header.owner_ids.group, 8));

	if (m_bIsUstar)
	{
		// copy user and group
		m_sUser.assign(Tar.header.extension.ustar.owner_names.user,
					  ::strnlen(Tar.header.extension.ustar.owner_names.user, 32));
		m_sGroup.assign(Tar.header.extension.ustar.owner_names.group,
					   ::strnlen(Tar.header.extension.ustar.owner_names.group, 32));

		// check if there is a filename prefix
		auto plen = ::strnlen(Tar.header.extension.ustar.filename_prefix, 155);

		// insert the prefix before the existing filename
		if (plen)
		{
			m_sFilename.assign(Tar.header.extension.ustar.filename_prefix, plen);
			m_sFilename += '/';
		}
	}

	// append file name to a prefix (or none)
	m_sFilename.append(Tar.header.file_name, ::strnlen(Tar.header.file_name, 100));

	auto TypeFlag = Tar.header.extension.ustar.type_flag;

    if (!m_sFilename.empty() && m_sFilename.back() == '/')
	{
        // make sure we detect also pre-1988 directory names :)
		if (!TypeFlag)
		{
			TypeFlag = '5';
		}
    }

    // now analyze the entry type

    switch (TypeFlag)
	{
        case 0:
        case '0':
		// treat contiguous file as normal
		case '7':
            // normal file
            if (m_EntryType == tar::Longname1)
			{
                // this is a subsequent read on GNU tar long names
                // the current block contains only the filename and the next block contains metadata
                // set the filename from the current header
                m_sFilename.assign(Tar.header.file_name,
								  ::strnlen(Tar.header.file_name, tar::HeaderLen));
                // the next header contains the metadata, so replace the header before reading the metadata
                m_EntryType        = tar::Longname2;
                m_bKeepMembersOnce = true;
                return true;
            }
            m_EntryType         = tar::File;
			m_iFilesize         = FromNumbers(Tar.header.file_bytes, 12);
			m_tModificationTime = FromNumbers(Tar.header.modification_time , 12);
			break;

        case '1':
            // link
            m_EntryType = tar::Hardlink;
            m_sLinkname.assign(Tar.header.extension.ustar.linked_file_name,
							  ::strnlen(Tar.header.file_name, 100));
            break;

        case '2':
            // symlink
            m_EntryType = tar::Symlink;
            m_sLinkname.assign(Tar.header.extension.ustar.linked_file_name,
							  ::strnlen(Tar.header.file_name, 100));
            break;

        case '5':
            // directory
            m_EntryType         = tar::Directory;
            m_iFilesize         = 0;
			m_tModificationTime = FromNumbers(Tar.header.modification_time, 12);
            break;

        case '6':
            // fifo
            m_EntryType = tar::Fifo;
            break;

        case 'L':
            // GNU long filename
            m_EntryType        = tar::Longname1;
            m_bKeepMembersOnce = true;
            return true;

        default:
            m_EntryType = tar::Unknown;
            break;
    }

	return true;

} // Decode
#if (__GNUC__ > 10)
#pragma GCC diagnostic pop
#endif

//-----------------------------------------------------------------------------
KUnTar::KUnTar(KInStream& Stream, int AcceptedTypes, bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: m_Stream(Stream)
	, m_AcceptedTypes(AcceptedTypes)
	, m_bSkipAppleResourceForks(bSkipAppleResourceForks)
{
}

//-----------------------------------------------------------------------------
KUnTar::KUnTar(KStringViewZ sArchiveFilename, int AcceptedTypes, bool bSkipAppleResourceForks)
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
		return SetError(kFormat("unexpected end of input stream, trying to read {} bytes, but got only {}", len, rb));
	}

	return true;

} // Read

//-----------------------------------------------------------------------------
size_t KUnTar::CalcPadding()
//-----------------------------------------------------------------------------
{
	if (Filesize() > 0)
	{
		// check if we have to skip some padding bytes (tar files have a block size of 512)
		return (tar::HeaderLen - (Filesize() % tar::HeaderLen)) % tar::HeaderLen;
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
		std::array<char, tar::HeaderLen> Padding;

		if (!Read(Padding.data(), padding))
		{
			return SetError("cannot read block padding");
		}
	}

	return true;

} // ReadPadding

//-----------------------------------------------------------------------------
bool KUnTar::Next()
//-----------------------------------------------------------------------------
{
	// delete a previous error
	m_Error.clear();

    do
	{
		if (!m_bIsConsumed && (Filesize() > 0))
		{
			if (!SkipCurrentFile())
			{
				return false;
			}
		}

		tar::TarHeader TarHeader;

		if (!Read(&TarHeader, tar::HeaderLen))
		{
			return SetError("cannot not read tar header");
		}

		if (!m_Header.Decode(TarHeader))
		{
			// the only false return from Decode happens on a bad checksum compare
			return SetError("invalid tar header (bad checksum)");
		}

        // this is the only valid exit condition from reading a tar archive - end header reached
        if (m_Header.IsEnd())
		{
			// no error!
			return false;
		}

        if (Filesize() > 0)
		{
			m_bIsConsumed = false;
        }

    } while ((Type() & m_AcceptedTypes) == 0
			 || (m_bSkipAppleResourceForks && kBasename(Filename()).starts_with("._")));

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

	std::array<char, KDefaultCopyBufSize> sBuffer;
	size_t iRead = 0;

	for (auto iRemain = iSize; iRemain;)
	{
		auto iChunk = std::min(sBuffer.size(), iRemain);
		if (!Read(sBuffer.data(), iChunk))
		{
			break;
		}
		iRead   += iChunk;
		iRemain -= iChunk;
	}

	if (iRead == iSize)
	{
		return true;
	}
	else
	{
		return SetError("cannot not skip file");
	}

} // Skip

//-----------------------------------------------------------------------------
bool KUnTar::SkipCurrentFile()
//-----------------------------------------------------------------------------
{
	return Skip(Filesize() + CalcPadding());

} // SkipCurrentFile

//-----------------------------------------------------------------------------
bool KUnTar::Read(KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	if (Type() != tar::File)
	{
		return SetError("cannot read - current tar entry is not a file");
	}

	if (!OutStream.Write(m_Stream, Filesize()).Good())
	{
		return SetError("cannot write to output stream");
	}

	if (!ReadPadding())
	{
		return false;
	}

	m_bIsConsumed = true;

	return true;

} // Read

//-----------------------------------------------------------------------------
bool KUnTar::Read(KStringRef& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (Type() != tar::File)
	{
		return SetError("cannot read - current tar entry is not a file");
	}

	
	// resize the buffer to be able to read the file size
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	sBuffer.resize_uninitialized(Filesize());
#else
	#ifdef __cpp_lib_string_resize_and_overwrite
	sContent.resize_and_overwrite(Filesize(), [](KStringRef::pointer buf, KStringRef::size_type buf_size) noexcept { return buf_size; });
	#else
	sBuffer.resize(Filesize());
	#endif
#endif

	// read the file into the buffer
	if (!Read(&sBuffer[0], Filesize()))
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
bool KUnTar::ReadFile(KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	KOutFile File(sFilename, std::ios_base::out & std::ios_base::trunc);

	if (!File.is_open())
	{
		return SetError(kFormat("cannot create file {}", sFilename));
	}

	return Read(File);

} // ReadFile

//-----------------------------------------------------------------------------
KString KUnTar::CreateTargetDirectory(KStringViewZ sBaseDir, KStringViewZ sEntry, bool bWithSubdirectories)
//-----------------------------------------------------------------------------
{
	KString sName = sBaseDir;
	sName += kDirSep;

	if (bWithSubdirectories)
	{
		KString     sSafePath = kMakeSafePathname(sEntry, false);
		KStringView sDirname  = kDirname(sSafePath);

		if (sDirname != ".")
		{
			sName += sDirname;

			if (!kCreateDir(sName))
			{
				SetError(kFormat("cannot create directory: {}", sName));
				return KString{};
			}

			sName += kDirSep;
		}
		sName += kBasename(sSafePath);
	}
	else
	{
		sName += kMakeSafeFilename(kBasename(sEntry), false);
	}

	return sName;

} // CreateTargetDirectory

//-----------------------------------------------------------------------------
bool KUnTar::ReadAll(KStringViewZ sTargetDirectory, bool bWithSubdirectories)
//-----------------------------------------------------------------------------
{
	if (!kDirExists(sTargetDirectory))
	{
		if (!kCreateDir(sTargetDirectory))
		{
			return SetError(kFormat("cannot create directory: {}", sTargetDirectory));
		}
	}

	for (auto& File : *this)
	{
		switch (File.Type())
		{
			case tar::Directory:
			case tar::File:
				{
					auto sName = CreateTargetDirectory(sTargetDirectory, SafePath(), bWithSubdirectories);

					if (sName.empty())
					{
						// error is already set
						return false;
					}

					if (File.Type() == tar::File)
					{
						if (!ReadFile(sName))
						{
							// error is already set
							return false;
						}
					}
				}
				break;

			case tar::Hardlink:
			case tar::Symlink:
				{
					auto sLink = CreateTargetDirectory(sTargetDirectory, SafePath(), bWithSubdirectories);

					if (sLink.empty())
					{
						// error is already set
						return false;
					}

					KString sOrigin = (bWithSubdirectories) ? File.SafeLinkPath() : File.SafeLinkName();

					if (File.Type() == tar::Symlink)
					{
						if (!kCreateSymlink(sOrigin, sLink))
						{
							return SetError(kFormat("cannot create symlink {} > {}", sOrigin, sLink));
						}
					}
					else
					{
						if (!kCreateHardlink(sOrigin, sLink))
						{
							return SetError(kFormat("cannot create hardlink {} > {}", sOrigin, sLink));
						}
					}
				}
				break;

			default:
				break;
		}
	}

	return true;

} // ReadAll

//-----------------------------------------------------------------------------
bool KUnTar::File(KStringRef& sName, KStringRef& sBuffer)
//-----------------------------------------------------------------------------
{
	sName.clear();
	sBuffer.clear();

    if (!Next())
	{
		return false;
	}

	if (Type() == tar::File)
	{
		if (!Read(sBuffer))
		{
			return false;
		}
	}

    sName = Filename();

    return true;

} // File

//-----------------------------------------------------------------------------
bool KUnTar::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	m_Error = std::move(sError);
	kDebug(2, m_Error);

	return false;

} // SetError

//-----------------------------------------------------------------------------
KUnTarCompressed::KUnTarCompressed(KUnCompressIStream::COMPRESSION Compression,
				 KInStream& InStream,
				 int AcceptedTypes,
				 bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: KUnTar(m_FilteredInStream, AcceptedTypes, bSkipAppleResourceForks)
{
	m_Filter.open(InStream, Compression);

} // ctor

//-----------------------------------------------------------------------------
KUnTarCompressed::KUnTarCompressed(KUnCompressIStream::COMPRESSION Compression,
				 KStringViewZ sArchiveFilename,
				 int AcceptedTypes,
				 bool bSkipAppleResourceForks)
//-----------------------------------------------------------------------------
	: KUnTar(m_FilteredInStream, AcceptedTypes, bSkipAppleResourceForks)
{
	m_File = std::make_unique<KInFile>(sArchiveFilename);

	if (Compression == KUnCompressIStream::AUTO)
	{
		Compression = KUnCompressIStream::GetCompressionMethodFromFilename(sArchiveFilename);
	}

	m_Filter.open(*m_File, Compression);

} // ctor

} // end of namespace dekaf2

