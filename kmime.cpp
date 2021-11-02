/*
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
#include "khttp_header.h"
#include "kbase64.h"
#include "kquotedprintable.h"
#include "kfilesystem.h"
#include "klog.h"
#include "kfrozen.h"
#include "ksystem.h"
#include "kctype.h"
#include "kinshell.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KMIME::ByExtension(KStringView sFilename, KStringView Default)
//-----------------------------------------------------------------------------
{

#ifdef DEKAF2_HAS_FROZEN
	static constexpr std::pair<KStringView, KStringViewZ> s_MIME_Extensions[]
#else
	static const std::unordered_map<KStringView, KStringViewZ> s_Extension_Map
#endif
	{
		{ "aac"_ksv  , AAC        },
		{ "mid"_ksv  , MIDI       },
		{ "midi"_ksv , MIDI       },
		{ "oga"_ksv  , OGA        },
		{ "wav"_ksv  , WAV        },
		{ "weba"_ksv , WEBA       },

		{ "bin"_ksv  , BINARY     },
		{ "js"_ksv   , JAVASCRIPT },
		{ "json"_ksv , JSON       },
		{ "xml"_ksv  , XML        },
		{ "swf"_ksv  , SWF        },
		{ "bz2"_ksv  , BZ2        },
		{ "csh"_ksv  , CSH        },
		{ "doc"_ksv  , DOC        },
		{ "docx"_ksv , DOCX       },
		{ "epub"_ksv , EPUB       },
		{ "jar"_ksv  , JAR        },
		{ "odp"_ksv  , ODP        },
		{ "ods"_ksv  , ODS        },
		{ "odt"_ksv  , ODT        },
		{ "ogx"_ksv  , OGX        },
		{ "pdf"_ksv  , PDF        },
		{ "ppt"_ksv  , PPT        },
		{ "pptx"_ksv , PPTX       },
		{ "rar"_ksv  , RAR        },
		{ "rtf"_ksv  , RTF        },
		{ "sh"_ksv   , SH         },
		{ "tar"_ksv  , TAR        },
		{ "ts"_ksv   , TS         },
		{ "vsd"_ksv  , VSD        },
		{ "xhtml"_ksv, XHTML      },
		{ "xls"_ksv  , XLS        },
		{ "xlsx"_ksv , XLSX       },
		{ "zip"_ksv  , ZIP        },
		{ "7z"_ksv   , SEVENZIP   },

		{ "eot"_ksv  , EOT        },
		{ "otf"_ksv  , OTF        },
		{ "ttc"_ksv  , TTC        },
		{ "ttf"_ksv  , TTF        },
		{ "woff"_ksv , WOFF       },
		{ "woff2"_ksv, WOFF2      },

		{ "jpg"_ksv  , JPEG       },
		{ "jpeg"_ksv , JPEG       },
		{ "gif"_ksv  , GIF        },
		{ "png"_ksv  , PNG        },
		{ "ico"_ksv  , ICON       },
		{ "svg"_ksv  , SVG        },
		{ "tif"_ksv  , TIFF       },
		{ "tiff"_ksv , TIFF       },
		{ "webp"_ksv , WEBP       },

		{ "txt"_ksv  , TEXT_UTF8  },
		{ "htm"_ksv  , HTML_UTF8  },
		{ "html"_ksv , HTML_UTF8  },
		{ "css"_ksv  , CSS        },
		{ "csv"_ksv  , CSV        },
		{ "ics"_ksv  , CALENDAR   },
		{ "po"_ksv   , PO         },

		{ "avi"_ksv  , AVI        },
		{ "mpeg"_ksv , MPEG       },
		{ "ogv"_ksv  , OGV        },
		{ "webm"_ksv , WEBM       },
	};

#ifdef DEKAF2_HAS_FROZEN
	static constexpr auto s_Extension_Map = frozen::make_unordered_map(s_MIME_Extensions);
#endif

	KString sExtension = kExtension(sFilename);

	if (sExtension.empty())
	{
		sExtension = sFilename;
		
		if (sExtension.front() == '.')
		{
			sExtension.erase(0, 1);
		}
	}

	sExtension.MakeLowerASCII();

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
KMIME KMIME::CreateByExtension(KStringView sFilename, KStringView Default)
//-----------------------------------------------------------------------------
{
	KMIME mime;
	mime.ByExtension(sFilename, Default);
	return mime;

} // CreateByExtension

//-----------------------------------------------------------------------------
bool KMIME::ByInspection(KStringViewZ sFilename, KStringView Default)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS
	if (kFileExists(sFilename, true))
	{
		KInShell Shell(kFormat("file --mime-type '{}'", sFilename));

		if (Shell.is_open())
		{
			auto sMime = Shell.ReadLine();
			auto iClip = sMime.rfind(": ");

			if (iClip != KString::npos)
			{
				sMime.erase(0, iClip + 2);
			}

			m_mime = sMime;

			if (m_mime != NONE)
			{
				return true;
			}
		}
	}
#endif

	m_mime = Default;

	return false;

} // ByInspection

//-----------------------------------------------------------------------------
KMIME KMIME::CreateByInspection(KStringViewZ sFilename, KStringView Default)
//-----------------------------------------------------------------------------
{
	KMIME mime;
	mime.ByInspection(sFilename, Default);
	return mime;

} // CreateByInspection

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

//-----------------------------------------------------------------------------
bool KMIME::IsCompressible()
//-----------------------------------------------------------------------------
{
	switch (m_mime.Hash())
	{
		case AAC.Hash():
		case OGA.Hash():
		case SWF.Hash():
		case BZ2.Hash():
		case DOCX.Hash():
		case JAR.Hash():
		case ODP.Hash():
		case ODS.Hash():
		case ODT.Hash():
		case OGX.Hash():
		case PPTX.Hash():
		case RAR.Hash():
		case ZIP.Hash():
		case SEVENZIP.Hash():
		case JPEG.Hash():
		case GIF.Hash():
		case PNG.Hash():
		case AVI.Hash():
		case MPEG.Hash():
		case OGV.Hash():
			return false;

		default:
			return true;
	}

	return true;

} // IsCompressible

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME))
{
	CreateMultiPartBoundary();
}

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KString sMessage, KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME)), m_Data(std::move(sMessage))
{
	CreateMultiPartBoundary();
}

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KString sControlName, KString sValue, KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME))
, m_Data(std::move(sValue))
, m_sControlName(std::move(sControlName))
{
	CreateMultiPartBoundary();
}

//-----------------------------------------------------------------------------
bool KMIMEPart::CreateMultiPartBoundary() const
//-----------------------------------------------------------------------------
{
	if (IsMultiPart())
	{
		if (!m_iRandom1 && !m_iRandom2)
		{
			m_iRandom1 = kRandom();
			m_iRandom2 = kRandom();
		}
		return true;
	}
	return false;

} // CreateMultiPartBoundary

//-----------------------------------------------------------------------------
bool KMIMEPart::IsMultiPart() const
//-----------------------------------------------------------------------------
{
	return m_MIME.Serialize().starts_with("multipart/");

} // IsMultiPart

//-----------------------------------------------------------------------------
bool KMIMEPart::IsBinary() const
//-----------------------------------------------------------------------------
{
	return !m_MIME.Serialize().starts_with("text/");

} // IsBinary

//-----------------------------------------------------------------------------
KMIME KMIMEPart::ContentType() const
//-----------------------------------------------------------------------------
{
	KString sContentType = m_MIME.Serialize();

	if (CreateMultiPartBoundary())
	{
		sContentType += kFormat("; boundary=----=_KMIME_Part_1_{}.{}----", m_iRandom1, m_iRandom2);
	}

	return KMIME(std::move(sContentType));

} // GetContentType

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KString& sOut, bool bForHTTP, const KReplacer& Replacer, uint16_t recursion, KMIME ParentMIME) const
//-----------------------------------------------------------------------------
{
	if (!IsMultiPart())
	{
		if (!m_Data.empty())
		{
			if (!(bForHTTP && recursion == 0))
			{
				sOut += "Content-Type: ";
				sOut += m_MIME;
				sOut += "\r\n";

				if (ParentMIME == KMIME::MULTIPART_RELATED && !m_sControlName.empty())
				{
					sOut += "Content-ID: ";
					sOut += KQuotedPrintable::Encode(m_sControlName, true);
					sOut += "\r\n";
				}

				if (!bForHTTP)
				{
					sOut += "Content-Transfer-Encoding: ";
					if (IsBinary())
					{
						sOut += "base64";
					}
					else
					{
						sOut += "quoted-printable";
					}
					sOut += "\r\n";
				}

				sOut += "Content-Disposition: ";
				if ((!m_sControlName.empty() || !m_sFileName.empty()) && ParentMIME != KMIME::MULTIPART_RELATED)
				{
					if (ParentMIME == KMIME::MULTIPART_FORM_DATA)
					{
						sOut += "form-data";
						if (!m_sControlName.empty())
						{
							sOut += "; name=\"";
							// TODO check if we should better use QuotedPrintable for UTF8 file names
							sOut += m_sControlName;
							sOut += "\"";
						}
						if (!m_sFileName.empty())
						{
							sOut += "; filename=\"";
							// TODO check if we should better use QuotedPrintable for UTF8 file names
							sOut += m_sFileName;
							sOut += '"';
						}
					}
					else
					{
						sOut += "attachment;\r\n filename=";
						sOut += KQuotedPrintable::Encode(m_sFileName, true);
					}
				}
				else
				{
					sOut += "inline";
				}

				sOut += "\r\n\r\n"; // End of headers
			}

			if (bForHTTP)
			{
				if (IsBinary() || (Replacer.empty() && !Replacer.GetRemoveAllVariables()))
				{
					sOut += m_Data;
				}
				else
				{
					sOut += Replacer.Replace(m_Data);
				}
			}
			else if (IsBinary())
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
	else if (m_Parts.size() == 1 && !bForHTTP)
	{
		// for non-HTTP, serialize the single part directly, do not embed it into a multipart structure
		return m_Parts.front().Serialize(sOut, bForHTTP, Replacer, recursion);
	}
	else
	{
		++recursion;

		uint32_t iRandom1;
		uint32_t iRandom2;

		if (recursion == 1)
		{
			if (!m_iRandom1 && !m_iRandom2)
			{
				m_iRandom1 = kRandom();
				m_iRandom2 = kRandom();
			}
			iRandom1 = m_iRandom1;
			iRandom2 = m_iRandom2;
		}
		else
		{
			iRandom1 = kRandom();
			iRandom2 = kRandom();
		}

		KString sBoundary;
		// having the '=' in the boundary guarantees for base64 and quoted printable encoding
		// that the boundary is unique
		sBoundary.Format("----=_KMIME_Part_{}_{}.{}----", recursion, iRandom1, iRandom2);

		if (!(bForHTTP && recursion == 1))
		{
			sOut += "Content-Type: ";
			sOut += m_MIME;
			sOut += ";\r\n boundary=\"";
			sOut += sBoundary;
			sOut += "\"\r\n\r\n"; // End of headers
		}

		for (auto& it : m_Parts)
		{
			sOut += "--";
			sOut += sBoundary;
			sOut += "\r\n";
			it.Serialize(sOut, bForHTTP, Replacer, recursion, m_MIME);
			sOut += "\r\n";
		}

		sOut += "--";
		sOut += sBoundary;
		sOut += "--\r\n";

		return true;
	}

	return false;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KOutStream& Stream, bool bForHTTP, const KReplacer& Replacer, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	if (Serialize(sOut, bForHTTP, Replacer, recursion))
	{
		return Stream.Write(sOut).Good();
	}
	else
	{
		return false;
	}

} // Serialize

//-----------------------------------------------------------------------------
KString KMIMEPart::Serialize(bool bForHTTP, const KReplacer& Replacer, uint16_t recursion) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut, bForHTTP, Replacer, recursion);
	return sOut;

} // Serialize

//-----------------------------------------------------------------------------
bool KMIMEPart::Attach(KMIMEPart part)
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
bool KMIMEPart::Stream(KStringView sControlName, KInStream& Stream, KStringView sDispname)
//-----------------------------------------------------------------------------
{
	if (Stream.ReadRemaining(m_Data))
	{
		m_sControlName = sControlName;
		m_sFileName = sDispname;
		return true;
	}
	else
	{
		kDebug(2, "cannot read stream: {}", sDispname);
		return false;
	}

} // Stream

//-----------------------------------------------------------------------------
bool KMIMEPart::File(KStringView sControlName, KStringView sFilename, KStringView sDispname)
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
		return Stream(sControlName, File, sDispname);
	}
	else
	{
		kDebug(2, "cannot open file: {}", sFilename);
	}
	return false;

} // File

//-----------------------------------------------------------------------------
KMIMEFile::KMIMEFile(KStringView sControlName, KStringView sData, KStringView sDispname, KMIME MIME)
//-----------------------------------------------------------------------------
: KMIMEPart(std::move(MIME))
{
	m_Data = sData;
	m_sControlName = sControlName;
	m_sFileName = sDispname;

	if (m_MIME == KMIME::NONE)
	{
		m_MIME.ByExtension(sDispname, KMIME::BINARY);
	}

} // ctor

//-----------------------------------------------------------------------------
KMIMEDirectory::KMIMEDirectory(KStringViewZ sPathname)
//-----------------------------------------------------------------------------
{
	if (kDirExists(sPathname))
	{
		// get all regular files
		KDirectory Dir(sPathname, KFileType::FILE);

		// remove the manifest if existing, we do not want to send it
		bool bHasManifest = Dir.WildCardMatch("manifest.ini", true);

		if (Dir.WildCardMatch("index.html", true))
		{
			// we have an index.html

			// create a multipart/related structure (it will be removed
			// automatically by the serializer if there are no related files..)
			MIME(KMIME::MULTIPART_RELATED);

			// create a multipart/alternative structure inside the
			// multipart/related (will be removed by the serializer if there
			// is no text part)
			KMIMEMultiPartAlternative Alternative;

			if (Dir.WildCardMatch("index.txt", true))
			{
				// add the text version to the alternative part
				KString sFile(sPathname);
				sFile += "/index.txt";
				Alternative += KMIMEFileInline(sFile);
			}

			// add the html version
			KString sFile = sPathname;
			sFile += "/index.html";

			KInFile fFile(sFile);

			if (!bHasManifest)
			{
				// the manifest is most probably at the start of this file
				KString sLine;
				if (fFile.ReadLine(sLine))
				{
					sLine.Trim();
					if (sLine == "#manifest")
					{
						// yes it is
						// remove it by reading the file until an empty line is reached
						for (auto& sLineRef : fFile)
						{
							if (sLineRef.empty())
							{
								break;
							}
						}
					}
					else
					{
						// rewind and read from the beginning
						fFile.Rewind();
					}
				}
			}

			Alternative += KMIMEFileInline(fFile, KMIME::CreateByExtension(sFile));

			// and attach to the related part
			*this += std::move(Alternative);

			for (auto& it : Dir)
			{
				// add all other files as related to the first part
				*this += KMIMEFile("", it.Path());
			}
		}
		else
		{
			// just create a multipart/mixed with all files
			MIME(KMIME::MULTIPART_MIXED);

			for (auto& it : Dir)
			{
				*this += KMIMEFile("", it.Path());
			}
		}
	}
	else
	{
		kDebug(2, "Directory does not exist: {}", sPathname);
	}

} // ctor


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

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
constexpr KStringViewZ KMIME::PO;
constexpr KStringViewZ KMIME::AVI;
constexpr KStringViewZ KMIME::MPEG;
constexpr KStringViewZ KMIME::OGV;
constexpr KStringViewZ KMIME::WEBM;

#endif

static_assert(std::is_nothrow_move_constructible<KMIMEPart>::value,
			  "KMIMEPart is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<KMIMEText>::value,
			  "KMIMEText is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2
