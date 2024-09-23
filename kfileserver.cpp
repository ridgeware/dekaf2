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
#include "kwebobjects.h"
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
					if (!IsDirectory())
					{
						GenerateAdHocIndexFile(sRequest, bCreateAdHocIndex, bWithUpload);
					}
					else
					{
						kDebug(1, "auto generation of index file is shadowed by directory {}{}{}", sRequest, kDirSep, m_sDirIndexFile);
					}
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

namespace {

constexpr KStringView sIndexStyles = R"(
body {
	font-family: Verdana, sans-serif;
}
table {
	border: 1px solid #bbbbbb;
}
td, th {
	padding: 0px 5px 0px 5px;
	margin: 0px 0px 0px 0px;
}
a {
	width: fit-content;
	text-decoration: none;
	text-color: black;
	background-color: #ffffff;
	border: 0px solid #bbbbbb;
	transition-duration: 0.1s;
}
a:hover {
	box-shadow: 0 12px 16px 0 rgba(0,0,0,0.25),0 17px 50px 0 rgba(0,0,0,0.20);
}
button {
	background-color: #ffffff;
	border: 0.5px solid #cccccc;
	cursor: pointer;
	transition-duration: 0.1s;
}
button:hover {
	box-shadow: 0 12px 16px 0 rgba(0,0,0,0.25),0 17px 50px 0 rgba(0,0,0,0.20);
}
)";

//-----------------------------------------------------------------------------
void AddIndexItem(html::Table& table, const KDirectory::DirEntry& Item, bool bWithDelete)
//-----------------------------------------------------------------------------
{
	constexpr KStringView sWasteBin   = "&#x1F6AE";
	constexpr KStringView sFolderIcon = "&#x1F4C1";

	bool bIsDirectory = Item.Type() == KFileType::DIRECTORY;

	auto& row = table.Add(html::TableRow());

	KString sItemLink = Item.Filename();

	if (bIsDirectory)
	{
		sItemLink += '/';
	}

	KString sTitle;

	if (bIsDirectory)
	{
		sTitle = "open directory '";
		sTitle += Item.Filename();
		sTitle += '\'';

		auto& td     = row.Add(html::TableData());
		auto& link   = td.Add(html::Link(sItemLink));
		auto& button = link.Add(html::Button());
		button.SetType(html::Button::BUTTON).AddRawText(sFolderIcon).SetTitle(sTitle);
	}
	else
	{
		sTitle = "open file '";
		sTitle += Item.Filename();
		sTitle += '\'';

		row += html::TableData();
	}

	auto& td = row.Add(html::TableData());
	td.SetAlign(html::TableData::LEFT);
	td += html::Link(sItemLink, Item.Filename()).SetTitle(sTitle);

	row += html::TableData(Item.ModificationTime().to_string()).SetAlign(html::TableData::LEFT);

	if (bIsDirectory)
	{
		row += html::TableData();
	}
	else
	{
		row += html::TableData(kFormBytes(Item.Size(), " ")).SetAlign(html::TableData::RIGHT);
	}

	if (bWithDelete)
	{
		auto& td = row.Add(html::TableData());
		auto& formDel = td.Add(html::Form());
		formDel.SetMethod(html::Form::POST);
		auto& formButton = formDel.Add(html::Button(KStringView{}, html::Button::SUBMIT));
		formButton.AddRawText(sWasteBin).SetValue(Item.Filename());

		if (bIsDirectory)
		{
			formButton.SetName("deleteDir");
			formButton.SetTitle(kFormat("{} '{}'", "delete directory", Item.Filename()));
		}
		else
		{
			formButton.SetName("deleteFile");
			formButton.SetTitle(kFormat("{} '{}'", "delete file", Item.Filename()));
		}
	}
	else
	{
		row += html::TableData();
	}

} // AddIndexItem

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void KFileServer::GenerateAdHocIndexFile(KStringView sDirectory, bool bWithIndex, bool bWithDelete)
//-----------------------------------------------------------------------------
{
	kDebug(2, "{}", sDirectory);

	constexpr KStringView sBackIcon = "&#x1F519";

	KString sDir   = kDirname(m_sFileSystemPath);
	KString sTitle = kFormat("Index of /{} :", sDirectory);

	html::Page page(sTitle, "en");
	page.AddStyle(sIndexStyles);
	auto& body = page.Body();

	if (bWithIndex)
	{
		KDirectory Dir(sDir);
		Dir.RemoveHidden();
		Dir.Sort();

		body.Add(html::Heading(3, sTitle));
		auto& table = body.Add(html::Table());

		{
			auto& row = table.Add(html::TableRow());
			auto& th  = row.Add(html::TableHeader());

			if (!sDirectory.empty())
			{
				auto& link   = th.Add(html::Link("..").SetTitle("go back"));
				auto& button = link.Add(html::Button());
				button.SetType(html::Button::BUTTON).SetTitle("go back").AddRawText(sBackIcon);
			}

			row += html::TableHeader("name").SetAlign(html::TableHeader::LEFT);
			row += html::TableHeader("last modification").SetAlign(html::TableHeader::LEFT);
			row += html::TableHeader("size").SetAlign(html::TableHeader::RIGHT);
			row += html::TableHeader();
		}

		for (const auto& File : Dir)
		{
			// list all dirs
			if (File.Type() == KFileType::DIRECTORY)
			{
				AddIndexItem(table, File, bWithDelete);
			}
		}

		for (const auto& File : Dir)
		{
			// list all files
			if (File.Type() == KFileType::FILE)
			{
				AddIndexItem(table, File, bWithDelete);
			}
		}
	}

	if (bWithDelete)
	{
		body += html::Paragraph();
		auto& table = body.Add(html::Table());
		{
			auto& row  = table.Add(html::TableRow());
			auto& col1 = row.Add(html::TableData());
			col1.Add(html::HTML("label").AddText("Add Directory:"));

			auto& col2 = row.Add(html::TableData().SetAlign(html::TableData::RIGHT));
			auto& form = col2.Add(html::Form());
			form.SetMethod(html::Form::POST);
			form += html::Input("createDir").SetType(html::Input::TEXT);
			form += html::Input().SetType(html::Input::SUBMIT).SetValue("Create");
		}
		{
			auto& row  = table.Add(html::TableRow());
			auto& col1 = row.Add(html::TableData());
			col1.Add(html::HTML("label").AddText("Add File:"));

			auto& col2 = row.Add(html::TableData().SetAlign(html::TableData::RIGHT));
			auto& form = col2.Add(html::Form());
			form.SetMethod(html::Form::POST).SetEncType(html::Form::FORMDATA);
			form += html::Input("upload1").SetType(html::Input::FILE);
			form += html::Input().SetType(html::Input::SUBMIT).SetValue("Upload");
		}
	}

	m_sIndex = page.Serialize();

	m_FileStat.SetType(KFileType::FILE);
	m_FileStat.SetSize(m_sIndex.size());
	m_FileStat.SetModificationTime(KUnixTime::now());
	m_FileStat.SetDefaults();
	m_bIsAdHocIndex = true;

} // GenerateAdHocIndexFile

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
