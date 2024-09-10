/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

#include "kfileserver.h"
#include "kfilesystem.h"
#include "khttperror.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KFileServer::Open(KStringView sDocumentRoot,
                       KStringView sRequest,
                       KStringView sRoute,
                       bool        bHadTrailingSlash,
                       bool        bCreateAdHocIndex,
                       bool        bWithUpload)
//-----------------------------------------------------------------------------
{
	clear();

	if (!sRequest.remove_prefix(sRoute) || (!sRequest.empty() && sRequest.front() != '/'))
	{
		kDebug(1, "invalid document path (internal error): {}", sRequest);

		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("invalid path: {}", sRequest) };
	}

	if (!sRequest.empty())
	{
		sRequest.remove_prefix(1); // the leading slash

		if (!sRequest.empty() && !kIsSafePathname(sRequest))
		{
			kDebug(1, "invalid document path: {}", sRequest);

			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("invalid path: /{}", sRequest) };
		}
	}

	m_sFileSystemPath = sDocumentRoot;

	if (!sRequest.empty())
	{
		m_sFileSystemPath += kDirSep;
		m_sFileSystemPath += sRequest;
	}

	m_FileStat = KFileStat(m_sFileSystemPath);

	if (!Exists() && bCreateAdHocIndex && m_sFileSystemPath.ends_with("/index.html"))
	{
		// remove the filename component so that this matches the directory
		// lookup below
		m_sFileSystemPath = kDirname(m_sFileSystemPath, /*bWithTrailingSlash=*/false);
		m_FileStat        = KFileStat(m_sFileSystemPath);
		bHadTrailingSlash = true;
	}

	if (IsDirectory())
	{
		if (bHadTrailingSlash)
		{
			// try index.html
			m_sFileSystemPath += kDirSep;
			m_sFileSystemPath += m_sDirIndexFile;
			m_FileStat         = KFileStat(m_sFileSystemPath);

			if (!Exists() && kExtension(m_sFileSystemPath) == "html")
			{
				// check for (index).htm if extension was .html
				m_sFileSystemPath.remove_suffix(1);
				m_FileStat = KFileStat(m_sFileSystemPath);

				if (!Exists())
				{
					// add the trailing l again to have a valid index file name
					// for the auto index generation
					m_sFileSystemPath += 'l';
				}
			}

			if (!Exists())
			{
				if (bCreateAdHocIndex || bWithUpload)
				{
					StartIndexFile();
				}

				if (bCreateAdHocIndex)
				{
					if (!IsDirectory())
					{
						GenerateAdHocIndexFile(sRequest);
					}
					else
					{
						kDebug(1, "auto generation of index file is shadowed by directory {}{}{}", sRequest, kDirSep, m_sDirIndexFile);
					}
				}

				if (bWithUpload)
				{
					if (!IsDirectory())
					{
						GenerateUploadIndexFile(sRequest);
					}
					else
					{
						kDebug(1, "auto generation of index file is shadowed by directory {}{}{}", sRequest, kDirSep, m_sDirIndexFile);
					}
				}

				if (bCreateAdHocIndex || bWithUpload)
				{
					EndIndexFile();
				}
			}
		}
		else
		{
			m_bReDirectory = true;
			return false;
		}
	}

	return true;

} // Open

//-----------------------------------------------------------------------------
void KFileServer::StartIndexFile()
//-----------------------------------------------------------------------------
{
	m_sIndex = "<html><body>\n";

} // StartIndexFile

//-----------------------------------------------------------------------------
void KFileServer::EndIndexFile()
//-----------------------------------------------------------------------------
{
	m_sIndex += "</body></html>\n";

	m_FileStat.SetType(KFileType::FILE);
	m_FileStat.SetSize(m_sIndex.size());
	m_FileStat.SetModificationTime(KUnixTime::now());
	m_FileStat.SetDefaults();
	m_bIsAdHocIndex = true;

} // EndIndexFile

//-----------------------------------------------------------------------------
void KFileServer::GenerateAdHocIndexFile(KStringView sDirectory)
//-----------------------------------------------------------------------------
{
	kDebug(2, "{}", sDirectory);
	KString sDir = kDirname(m_sFileSystemPath);

	KDirectory Dir(sDir);
	Dir.RemoveHidden();
	Dir.Sort();

	m_sIndex += kFormat("<h3>Index of /{}:</h3><table>\n", sDirectory);
	m_sIndex += "<th></th><th>&nbsp;</th><th align=left>name</th><th>&nbsp;</th><th align=left>last modification</th><th>&nbsp;</th><th align=right>size</th>\n";

	if (!sDirectory.empty())
	{
		// add a back link
		m_sIndex += "<tr><td>&#x23f4;</td><td></td><td align=left><a href=\"..\">.&nbsp;.&nbsp;</a></td><td></td><td></td><td></td><td></td></tr>\n";
	}

	for (const auto& File : Dir)
	{
		// list all dirs
		if (File.Type() == KFileType::DIRECTORY)
		{
			m_sIndex += kFormat("<tr><td>&#x274f;</td><td></td><td align=left><a href=\"{}/\">{}</a></td><td></td><td align=left>{}</td><td></td><td></td></tr>\n",
								File.Filename(), File.Filename(), File.ModificationTime().to_string());
		}
	}

	m_sIndex += "<br/>";

	for (const auto& File : Dir)
	{
		// list all files
		if (File.Type() == KFileType::FILE)
		{
			m_sIndex += kFormat("<tr><td></td><td></td><td align=left><a href=\"{}\">{}</a></td><td></td><td align=left>{}</td><td></td><td align=right>{}</td></tr>\n",
								File.Filename(), File.Filename(), File.ModificationTime().to_string(), kFormBytes(File.Size(), "&nbsp;"));
		}
	}

	m_sIndex += "</table>";

} // GenerateAdHocIndexFile

//-----------------------------------------------------------------------------
void KFileServer::GenerateUploadIndexFile(KStringView sDirectory)
//-----------------------------------------------------------------------------
{
	kDebug(2, "{}", sDirectory);

	m_sIndex += R"(
		<form method="post" enctype="multipart/form-data">
		<p>
			<label>Add file: </label>
			<br/><input type="file" name="upload1"/>
<!---		<br/><input type="file" name="upload2"/>
			<br/><input type="file" name="upload3"/> --->
		</p>
		<p>
			<input type="submit"/>
		</p>
		</form>
	)";

} // GenerateUploadIndexFile

//-----------------------------------------------------------------------------
std::unique_ptr<KInStream> KFileServer::GetStreamForReading(std::size_t iFromPos)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KInFile> Stream;

	if (!m_bIsAdHocIndex)
	{
		if (m_FileStat.IsFile())
		{
			Stream = std::make_unique<KInFile>(m_sFileSystemPath);
		}

		if (!Stream || !Stream->is_open() || !Stream->Good())
		{
			kDebug(1, "Cannot open file: {}", m_sFileSystemPath);

			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
		}
		else if (iFromPos > 0)
		{
			if (!Stream->SetReadPosition(iFromPos))
			{
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad position" };
			}
		}
	}

	return Stream;

} // GetStreamForReading

//-----------------------------------------------------------------------------
std::unique_ptr<KOutStream> KFileServer::GetStreamForWriting(std::size_t iToPos)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KOutFile> Stream;

	Stream = std::make_unique<KOutFile>(m_sFileSystemPath);

	if (!Stream || !Stream->is_open() || !Stream->Good())
	{
		kDebug(1, "Cannot open file: {}", m_sFileSystemPath);

		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "cannot open file" };
	}
	else if (iToPos > 0)
	{
		if (!Stream->SetWritePosition(iToPos))
		{
			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad position" };
		}
	}

	return Stream;

} // GetStreamForWriting

//-----------------------------------------------------------------------------
const KMIME& KFileServer::GetMIMEType(bool bInspect)
//-----------------------------------------------------------------------------
{
	if (m_mime == KMIME::NONE)
	{
		if (m_FileStat.IsFile())
		{
			m_mime = KMIME::CreateByExtension(m_sFileSystemPath);

			if (m_mime == KMIME::NONE)
			{
				if (bInspect)
				{
					m_mime = KMIME::CreateByInspection(m_sFileSystemPath, KMIME::BINARY);
				}
				else
				{
					m_mime = KMIME::BINARY;
				}
			}
		}
		else
		{
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
		}
	}
	
	return m_mime;

} // GetMIMEType

//-----------------------------------------------------------------------------
void KFileServer::clear()
//-----------------------------------------------------------------------------
{
	m_sFileSystemPath.clear();
	m_sIndex.clear();
	m_mime          = KMIME::NONE;
	m_FileStat      = KFileStat();
	m_bReDirectory  = false;
	m_bIsAdHocIndex = false;

} // clear

static_assert(std::is_nothrow_move_constructible<KFileServer>::value,
			  "KFileServer is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
