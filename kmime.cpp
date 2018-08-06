/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "kmime.h"
#include "kbase64.h"
#include "kquotedprintable.h"
#include "kfilesystem.h"
#include "klog.h"
#include "kfrozen.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KMIME::ByExtension(KStringView sFilename, KMIME Default)
//-----------------------------------------------------------------------------
{

	static constexpr std::pair<KStringView, KStringViewZ> s_MIME_Extensions[]
	{
		{ "aac"_ksv,   AAC },
		{ "mid"_ksv,   MIDI },
		{ "midi"_ksv,  MIDI },
		{ "oga"_ksv,   OGA },
		{ "wav"_ksv,   WAV },
		{ "weba"_ksv,  WEBA },

		{ "bin"_ksv,   BINARY },
		{ "js"_ksv,    JAVASCRIPT },
		{ "json"_ksv,  JSON },
		{ "xml"_ksv,   XML },
		{ "swf"_ksv,   SWF },
		{ "bz2"_ksv,   BZ2 },
		{ "csh"_ksv,   CSH },
		{ "doc"_ksv,   DOC },
		{ "docx"_ksv,  DOCX },
		{ "epub"_ksv,  EPUB },
		{ "jar"_ksv,   JAR },
		{ "odp"_ksv,   ODP },
		{ "ods"_ksv,   ODS },
		{ "odt"_ksv,   ODT },
		{ "ogx"_ksv,   OGX },
		{ "pdf"_ksv,   PDF },
		{ "ppt"_ksv,   PPT },
		{ "pptx"_ksv,  PPTX },
		{ "rar"_ksv,   RAR },
		{ "rtf"_ksv,   RTF },
		{ "sh"_ksv,    SH },
		{ "tar"_ksv,   TAR },
		{ "ts"_ksv,    TS },
		{ "vsd"_ksv,   VSD },
		{ "xhtml"_ksv, XHTML },
		{ "xls"_ksv,   XLS },
		{ "xlsx"_ksv,  XLSX },
		{ "zip"_ksv,   ZIP },
		{ "7z"_ksv,    SEVENZIP },

		{ "eot"_ksv,   EOT },
		{ "otf"_ksv,   OTF },
		{ "ttc"_ksv,   TTC },
		{ "ttf"_ksv,   TTF },
		{ "woff"_ksv,  WOFF },
		{ "woff2"_ksv, WOFF2 },

		{ "jpg"_ksv,   JPEG },
		{ "jpeg"_ksv,  JPEG },
		{ "gif"_ksv,   GIF },
		{ "png"_ksv,   PNG },
		{ "ico"_ksv,   ICON },
		{ "svg"_ksv,   SVG },
		{ "tif"_ksv,   TIFF },
		{ "tiff"_ksv,  TIFF },
		{ "webp"_ksv,  WEBP },

		{ "txt"_ksv,   TEXT_UTF8 },
		{ "htm"_ksv,   HTML_UTF8 },
		{ "html"_ksv,  HTML_UTF8 },
		{ "css"_ksv,   CSS },
		{ "csv"_ksv,   CSV },
		{ "ics"_ksv,   CALENDAR },

		{ "avi"_ksv,   AVI },
		{ "mpeg"_ksv,  MPEG },
		{ "ogv"_ksv,   OGV },
		{ "webm"_ksv,  WEBM },
	};

	static constexpr auto s_Extension_Map = frozen::make_unordered_map(s_MIME_Extensions);

	KString sExtension = kExtension(sFilename).ToLowerLocale();

	if (sExtension.empty())
	{
		sExtension = sFilename;
	}

	auto it = s_Extension_Map.find(sExtension);

	if (it != s_Extension_Map.end())
	{
		m_mime = it->second;
		return true;
	}

	m_mime = Default;
	return false;

} // ByExtension

//-----------------------------------------------------------------------------
KMIME KMIME::CreateByExtension(KStringView sFilename, KMIME Default)
//-----------------------------------------------------------------------------
{
	KMIME mime;
	mime.ByExtension(sFilename, Default);
	return mime;

} // Create

//-----------------------------------------------------------------------------
bool KMIMEPart::IsMultiPart() const
//-----------------------------------------------------------------------------
{
	return KStringView(m_MIME).StartsWith("multipart/");

} // IsMultiPart

//-----------------------------------------------------------------------------
bool KMIMEPart::IsBinary() const
//-----------------------------------------------------------------------------
{
	return !KStringView(m_MIME).StartsWith("text/");

} // IsBinary

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KString& sOut, const KReplacer& Replacer, uint16_t recursion, bool bIsMultipartRelated) const
//-----------------------------------------------------------------------------
{
	if (!IsMultiPart())
	{
		if (!m_Data.empty())
		{
			sOut += "Content-Type: ";
			sOut += m_MIME;
			sOut += "\r\n";

			if (bIsMultipartRelated && !m_sName.empty())
			{
				sOut += "Content-ID: ";
				sOut += KQuotedPrintable::Encode(m_sName, true);
				sOut += "\r\n";
			}

			sOut += "Content-Transfer-Encoding: ";
			if (IsBinary())
			{
				sOut += "base64";
			}
			else
			{
				sOut += "quoted-printable";
			}

			sOut += "\r\nContent-Disposition: ";
			if (!m_sName.empty() && !bIsMultipartRelated)
			{
				if (m_MIME == KMIME::MULTIPART_FORM_DATA)
				{
					sOut += "form-data; name=\"";
					sOut += m_sName;
					sOut += '"';
				}
				else
				{
					sOut += "attachment;\r\n filename=";
					sOut += KQuotedPrintable::Encode(m_sName, true);
				}
			}
			else
			{
				sOut += "inline";
			}

			sOut += "\r\n\r\n"; // End of headers

			if (IsBinary())
			{
				sOut += KBase64::Encode(m_Data);
			}
			else
			{
				if (Replacer.empty() && !Replacer.GetRemoveAllVariables())
				{
					sOut += KQuotedPrintable::Encode(m_Data, false);
				}
				else
				{
					sOut += KQuotedPrintable::Encode(Replacer.Replace(m_Data), false);
				}
			}

			return true;
		}
	}
	else if (m_Parts.empty())
	{
		return false;
	}
	else if (m_Parts.size() == 1)
	{
		// serialize the single part directly, do not embed it into a multipart structure
		return m_Parts.front().Serialize(sOut, Replacer, recursion);
	}
	else
	{
		++recursion;

		sOut += "Content-Type: ";
		sOut += m_MIME;
		KString sBoundary;
		sBoundary.Format("----=_KMIME_Part_{}_{}.{}----", recursion, random(), random());
		sOut += ";\r\n boundary=\"";
		sOut += sBoundary;
		sOut += "\"\r\n"; // End of headers (see the next \r\n at the begin of the boundaries)

		for (auto& it : m_Parts)
		{
			sOut += "\r\n--";
			sOut += sBoundary;
			sOut += "\r\n";
			it.Serialize(sOut, Replacer, recursion, m_MIME == KMIME::MULTIPART_RELATED);
		}

		sOut += "\r\n--";
		sOut += sBoundary;
		sOut += "--\r\n";

		return true;
	}

	return false;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KOutStream& Stream, const KReplacer& Replacer, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	if (Serialize(sOut, Replacer, recursion))
	{
		return Stream.Write(sOut).Good();
	}
	else
	{
		return false;
	}

} // Serialize

//-----------------------------------------------------------------------------
KString KMIMEPart::Serialize(const KReplacer& Replacer, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut, Replacer, recursion);
	return sOut;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Attach(KMIMEPart&& part)
//-----------------------------------------------------------------------------
{
	if (IsMultiPart())
	{
		m_Parts.push_back(std::move(part));
		return true;
	}
	else
	{
		kDebug(1, "cannot attach to non-multipart KMIMEPart");
		return false;
	}

} // Attach

//-----------------------------------------------------------------------------
bool KMIMEPart::Stream(KInStream& Stream, KStringView sDispname)
//-----------------------------------------------------------------------------
{
	if (Stream.ReadAll(m_Data))
	{
		m_sName = sDispname;
		return true;
	}
	else
	{
		kDebug(2, "cannot read stream: {}", sDispname);
		return false;
	}

} // Stream

//-----------------------------------------------------------------------------
bool KMIMEPart::File(KStringView sFilename, KStringView sDispname)
//-----------------------------------------------------------------------------
{
	KInFile File(sFilename);
	if (File.is_open())
	{
		if (sDispname.empty())
		{
			sDispname = kBasename(sFilename);
		}
		if (m_MIME == KMIME::NONE)
		{
			m_MIME.ByExtension(sFilename, KMIME::BINARY);
		}
		return Stream(File, sDispname);
	}
	else
	{
		kDebug(2, "cannot open file: {}", sFilename);
	}
	return false;

} // File

//-----------------------------------------------------------------------------
KMIMEDirectory::KMIMEDirectory(KStringViewZ sPathname)
//-----------------------------------------------------------------------------
{
	if (kDirExists(sPathname))
	{
		// get all regular files
		KDirectory Dir(sPathname, KDirectory::EntryType::REGULAR);
		if (Dir.Find("index.html", true))
		{
			// we have an index.html

			// create a multipart/related structure (it will be removed
			// automatically by the serializer if there are no related files..)
			MIME(KMIME::MULTIPART_RELATED);

			// create a multipart/alternative structure inside the
			// multipart/related (will be removed by the serializer if there
			// is no text part)
			KMIMEMultiPartAlternative Alternative;

			if (Dir.Find("index.txt", true))
			{
				// add the text version to the alternative part
				KString sFile(sPathname);
				sFile += "/index.txt";
				Alternative += KMIMEFileInline(sFile);
			}

			// add the html version
			KString sFile = sPathname;
			sFile += "/index.html";
			Alternative += KMIMEFileInline(sFile);

			// and attach to the related part
			*this += std::move(Alternative);

			for (auto& it : Dir)
			{
				// add all other files as related to the first part
				*this += KMIMEFile(it);
			}
		}
		else
		{
			// just create a multipart/mixed with all files
			MIME(KMIME::MULTIPART_MIXED);

			for (auto& it : Dir)
			{
				*this += KMIMEFile(it);
			}
		}
	}
	else
	{
		kDebug(2, "Directory does not exist: {}", sPathname);
	}

} // ctor


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KCharSet::ANY_ISO8859;
constexpr KStringViewZ KCharSet::DEFAULT_CHARSET;

constexpr KStringViewZ KMIME::NONE;
constexpr KStringViewZ KMIME::AAC;
constexpr KStringViewZ KMIME::MIDI;
constexpr KStringViewZ KMIME::OGA;
constexpr KStringViewZ KMIME::WAV;
constexpr KStringViewZ KMIME::WEBA;
constexpr KStringViewZ KMIME::BINARY;
constexpr KStringViewZ KMIME::JAVASCRIPT;
constexpr KStringViewZ KMIME::JSON;
constexpr KStringViewZ KMIME::XML;
constexpr KStringViewZ KMIME::SWF;
constexpr KStringViewZ KMIME::WWW_FORM_URLENCODED;
constexpr KStringViewZ KMIME::AZV;
constexpr KStringViewZ KMIME::BZ2;
constexpr KStringViewZ KMIME::CSH;
constexpr KStringViewZ KMIME::DOC;
constexpr KStringViewZ KMIME::DOCX;
constexpr KStringViewZ KMIME::EPUB;
constexpr KStringViewZ KMIME::JAR;
constexpr KStringViewZ KMIME::ODP;
constexpr KStringViewZ KMIME::ODS;
constexpr KStringViewZ KMIME::ODT;
constexpr KStringViewZ KMIME::OGX;
constexpr KStringViewZ KMIME::PDF;
constexpr KStringViewZ KMIME::PPT;
constexpr KStringViewZ KMIME::PPTX;
constexpr KStringViewZ KMIME::RAR;
constexpr KStringViewZ KMIME::RTF;
constexpr KStringViewZ KMIME::SH;
constexpr KStringViewZ KMIME::TAR;
constexpr KStringViewZ KMIME::TS;
constexpr KStringViewZ KMIME::VSD;
constexpr KStringViewZ KMIME::XHTML;
constexpr KStringViewZ KMIME::XLS;
constexpr KStringViewZ KMIME::XLSX;
constexpr KStringViewZ KMIME::ZIP;
constexpr KStringViewZ KMIME::SEVENZIP;
constexpr KStringViewZ KMIME::EOT;
constexpr KStringViewZ KMIME::OTF;
constexpr KStringViewZ KMIME::TTC;
constexpr KStringViewZ KMIME::TTF;
constexpr KStringViewZ KMIME::WOFF;
constexpr KStringViewZ KMIME::WOFF2;
constexpr KStringViewZ KMIME::BMP;
constexpr KStringViewZ KMIME::JPEG;
constexpr KStringViewZ KMIME::GIF;
constexpr KStringViewZ KMIME::PNG;
constexpr KStringViewZ KMIME::ICON;
constexpr KStringViewZ KMIME::SVG;
constexpr KStringViewZ KMIME::TIFF;
constexpr KStringViewZ KMIME::WEBP;
constexpr KStringViewZ KMIME::MULTIPART_FORM_DATA;
constexpr KStringViewZ KMIME::MULTIPART_ALTERNATIVE;
constexpr KStringViewZ KMIME::MULTIPART_MIXED;
constexpr KStringViewZ KMIME::MULTIPART_RELATED;
constexpr KStringViewZ KMIME::TEXT_PLAIN;
constexpr KStringViewZ KMIME::TEXT_UTF8;
constexpr KStringViewZ KMIME::HTML_UTF8;
constexpr KStringViewZ KMIME::CSS;
constexpr KStringViewZ KMIME::CSV;
constexpr KStringViewZ KMIME::CALENDAR;
constexpr KStringViewZ KMIME::AVI;
constexpr KStringViewZ KMIME::MPEG;
constexpr KStringViewZ KMIME::OGV;
constexpr KStringViewZ KMIME::WEBM;

#endif

} // end of namespace dekaf2
