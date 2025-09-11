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
KFileServer::KFileServer(const KJSON& jConfig)
//-----------------------------------------------------------------------------
: m_jConfig(jConfig)
, m_sDirIndexFile(kjson::GetStringRef(jConfig, "indexfile"))
{
	if (m_sDirIndexFile.empty())
	{
		m_sDirIndexFile = "index.html";
	}
}

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

	KString sNormalizedRequest;

	if (!sRequest.empty())
	{
		sRequest.remove_prefix(1); // the leading slash

		if (!sRequest.empty())
		{
			sNormalizedRequest = kNormalizePath(sRequest, true);

			if (sNormalizedRequest.empty())
			{
				kDebug(1, "invalid document path: {}", sRequest);

				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("invalid path: /{}", sRequest) };
			}
		}

		sRequest = sNormalizedRequest;
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
	background-color: #bbcddf; 
}
table {
	border: 1px solid #555555;
	border-radius: 2px;
}
td, th {
	padding: 0px 5px 0px 5px;
	margin: 0px 0px 0px 0px;
}
a {
	width: fit-content;
	text-decoration: none;
	text-color: black;
	transition-duration: 0.1s;
}
input[type='text'] {
	border: 1px solid black;
	border-radius: 2px;
	width: 100%;
}
button, input[type='submit'], input::file-selector-button {
	background-color: #cbddff;
	border: 1px solid black;
	border-radius: 6px;
	cursor: pointer;
	transition-duration: 0.1s;
}
a:hover, button:hover, input[type='submit']:hover, input::file-selector-button:hover {
	box-shadow: 0 12px 16px 0 rgba(0,0,0,0.25),0 17px 50px 0 rgba(0,0,0,0.20);
}
)";

//-----------------------------------------------------------------------------
void AddIndexItem(html::Table& table, const KDirectory::DirEntry& Item, bool bWithDelete)
//-----------------------------------------------------------------------------
{
	constexpr KStringView sWasteBin   = "&#x1F6AE;";
	constexpr KStringView sFolderIcon = "&#x1F4C1;";

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
		row += html::TableData(kFormBytes(Item.Size(), 1, " ")).SetAlign(html::TableData::RIGHT);
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

	constexpr KStringView sBackIcon = "&#x1F519;";

	KString sDir   = kDirname(m_sFileSystemPath);
	KString sTitle = kFormat("Index of /{} :", sDirectory);

	html::Page page(sTitle, "en");

	auto& sStyles = kjson::GetStringRef(m_jConfig, "styles");

	if (sStyles.empty())
	{
		// no CSS styles added - use default styles
		page.AddStyle(sIndexStyles);
	}
	else if (sStyles.ends_with(".css"))
	{
		// this is a path to css styles - use a style link in the header
		page.Head().Add(html::StyleSheet(sStyles));
	}
	else
	{
		// this is a stylesheet - copy it into the header
		page.AddStyle(sStyles);
	}

	page.Head().AddRawText(kjson::GetStringRef(m_jConfig, "addtohead"));
	page.Body().AddRawText(kjson::GetStringRef(m_jConfig, "addtobodytop"));

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
			auto& form = row.Add(html::Form());
			form.SetMethod(html::Form::POST);

			auto& col1 = form.Add(html::TableData());
			col1.Add(html::HTML("label").AddText("Add Directory:"));

			auto& col2 = form.Add(html::TableData());
			col2 += html::Input("createDir").SetType(html::Input::TEXT);

			auto& col3 = form.Add(html::TableData().SetAlign(html::TableData::RIGHT));
			col3 += html::Input().SetType(html::Input::SUBMIT).SetValue("Create");
		}
		{
			auto& row  = table.Add(html::TableRow());
			auto& form = row.Add(html::Form());
			form.SetMethod(html::Form::POST).SetEncType(html::Form::FORMDATA);

			auto& col1 = form.Add(html::TableData());
			col1.Add(html::HTML("label").AddText("Add File:"));

			auto& col2 = form.Add(html::TableData());
			col2 += html::Input("upload1").SetType(html::Input::FILE);

			auto& col3 = form.Add(html::TableData().SetAlign(html::TableData::RIGHT));
			col3 += html::Input().SetType(html::Input::SUBMIT).SetValue("Upload");
		}
	}

	page.Body().AddRawText(kjson::GetStringRef(m_jConfig, "addtobodybottom"));

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

#if DEKAF2_IS_CLANG && DEKAF2_CLANG_VERSION_MAJOR < 4
	return std::unique_ptr<KInStream>(static_cast<KInStream*>(Stream.release()));
#else
	return Stream;
#endif

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

#if DEKAF2_IS_CLANG && DEKAF2_CLANG_VERSION_MAJOR < 4
	return std::unique_ptr<KOutStream>(static_cast<KOutStream*>(Stream.release()));
#else
	return Stream;
#endif

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

// don't test KFileServer if its base class KErrorBase is not nothrow_move_constructible
// (this is the case with a combination of old clang and some libstdc++)
static_assert(!std::is_nothrow_move_constructible<KErrorBase>::value || std::is_nothrow_move_constructible<KFileServer>::value,
			  "KFileServer is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
