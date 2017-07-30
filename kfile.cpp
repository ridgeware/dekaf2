/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
//
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <experimental/filesystem>

#include "kfile.h"
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

namespace fs = std::experimental::filesystem;

//-----------------------------------------------------------------------------
KFile::KFile(KFile&& other) noexcept
//-----------------------------------------------------------------------------
	: m_FilePath(std::move(other.m_FilePath))
	, m_eFlags(std::move(other.m_eFlags))
	, m_File(std::move(other.m_File))
{
}

//-----------------------------------------------------------------------------
KFile& KFile::operator=(KFile&& other) noexcept
//-----------------------------------------------------------------------------
{
	m_FilePath = std::move(other.m_FilePath);
	m_eFlags = std::move(other.m_eFlags);
	m_File = std::move(other.m_File);
	return *this;
}

//-----------------------------------------------------------------------------
KString KFile::CalcFOpenModeString(KFileFlags eFlags)
//-----------------------------------------------------------------------------
{
	KString sMode;

	// we have READ, WRITE, APPEND
	// fopen() has r, r+, w, w+, a, a+

	if (eFlags & APPEND)
	{
		// append mode always includes creation if not existing
		if (eFlags & READ)
		{
			sMode = "a+";
		}
		else
		{
			sMode = "a";
		}
	}
	else if (eFlags & WRITE)
	{
		// these modes always truncate the file if existing
		if (eFlags & READ)
		{
			sMode = "w+";
		}
		else
		{
			sMode = "w";
		}
	}
	else if (eFlags & READ)
	{
		sMode = "r";
	}

	// RVO
	return sMode;
}

//-----------------------------------------------------------------------------
bool KFile::Open(const KString& sPathName, KFileFlags eFlags)
//-----------------------------------------------------------------------------
{
	Close();

	m_FilePath = fs::u8path(sPathName.s());
	m_eFlags = eFlags;

	if ((eFlags & (READ | WRITE | APPEND)) == 0)
	{
		return true;
	}

	m_File = fopen(m_FilePath.c_str(), CalcFOpenModeString(eFlags).c_str());

	if (!m_File)
	{
		KLog().warning ("KFile: Unable to open file '{0}': {1}", m_FilePath.string(), strerror(errno));
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KFile::Open(FILE* fileptr, KFileFlags eFlags)
//-----------------------------------------------------------------------------
{
	Close();

	m_File = fileptr;
}

//-----------------------------------------------------------------------------
bool KFile::Open(int filedesc, KFileFlags eFlags)
//-----------------------------------------------------------------------------
{
	return Open(fdopen(filedesc, CalcFOpenModeString(eFlags).c_str()));
}

//-----------------------------------------------------------------------------
void KFile::Close()
//-----------------------------------------------------------------------------
{
	if (m_File)
	{
		fclose(m_File);
		m_File = nullptr;
	}
}

//-----------------------------------------------------------------------------
bool KFile::Write(const KString& sLine)
//-----------------------------------------------------------------------------
{
	if (sLine.empty())
	{
		return true;
	}

	if (fwrite(sLine.data(), 1, sLine.size(), m_File) != sLine.size())
	{
		KLog().warning("KFile: cannot write to file '{0}': {1}", m_FilePath.string(), strerror(errno));
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KFile::WriteLine(const KString& sLine)
//-----------------------------------------------------------------------------
{
	if (!Write(sLine))
	{
		return false;
	}

	if (fputc('\n', m_File) == EOF)
	{
		KLog().warning("KFile: cannot write to file '{0}': {1}", m_FilePath.string(), strerror(errno));
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KFile::Flush()
//-----------------------------------------------------------------------------
{
	if (fflush(m_File) == EOF)
	{
		KLog().warning("KFile: cannot flush file '{0}': {1}", m_FilePath.string(), strerror(errno));
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
bool KFile::Exists(KFileFlags eFlags) const
//-----------------------------------------------------------------------------
{
	std::error_code ec;

	fs::file_status status = fs::status(m_FilePath, ec);

	if (ec)
	{
		return false;
	}

	if (!fs::exists(status))
	{
		return false;
	}

	if (fs::is_directory(status))
	{
		return false;
	}

	if ((eFlags & TEST0) == 0)
	{
		// exists and is a file
		return true;
	}

	if (fs::is_empty(m_FilePath, ec))
	{
		return false;
	}

	if (ec)
	{
		return false;
	}

	return true;
} // Exists

// static versions

//-----------------------------------------------------------------------------
bool KFile::Exists (const KString& sPath, KFileFlags iFlags/*=NONE*/)
//-----------------------------------------------------------------------------
{
	KFile File(sPath);
	return File.Exists(iFlags);

} // Exists

//-----------------------------------------------------------------------------
bool KFile::GetContent (KString& sContent, const KString& sPath, KFileFlags eFlags/*=TEXT*/)
//-----------------------------------------------------------------------------
{
	KFile File(sPath, eFlags | READ);
	return File.KFileReader::GetContent(sContent);

} // GetContent

//-----------------------------------------------------------------------------
size_t KFile::GetSize (const KString& sPath)
//-----------------------------------------------------------------------------
{
	KFile File(sPath);
	return File.KFileReader::GetSize();

} // GetSize


} // end of namespace dekaf2

