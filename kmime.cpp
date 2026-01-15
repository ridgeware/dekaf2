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
#include "kfilesystem.h"
#include "kinstringstream.h"
#include "kuuid.h"
#include <utility>

DEKAF2_NAMESPACE_BEGIN

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
		{ "mp3"_ksv  , MP3        },

		{ "7z"_ksv   , SEVENZIP   },
		{ "bin"_ksv  , BINARY     },
		{ "br"_ksv   , BR         },
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
		{ "ts"_ksv   , TYPESCRIPT },
		{ "vsd"_ksv  , VSD        },
		{ "wasm"_ksv , WASM       },
		{ "xhtml"_ksv, XHTML      },
		{ "xls"_ksv  , XLS        },
		{ "xlsx"_ksv , XLSX       },
		{ "zip"_ksv  , ZIP        },
		{ "zst"_ksv  , ZSTD       },
		{ "zstd"_ksv , ZSTD       },

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
		{ "h"_ksv    , H          },
		{ "c"_ksv    , C          },
		{ "cc"_ksv   , CPP        },
		{ "cpp"_ksv  , CPP        },
		{ "css"_ksv  , CSS        },
		{ "csv"_ksv  , CSV        },
		{ "tsv"_ksv  , TSV        },
		{ "ics"_ksv  , CALENDAR   },
		{ "po"_ksv   , PO         },
		{ "md"_ksv   , MD         },
		{ "js"_ksv   , JAVASCRIPT },
		{ "java"_ksv , JAVA       },
		{ "cs"_ksv   , CSHARP     },
		{ "go"_ksv   , GOLANG     },
		{ "php"_ksv  , PHP        },
		{ "py"_ksv   , PYTHON     },
		{ "rb"_ksv   , RUBY       },
		{ "tex"_ksv  , TEX        },

		{ "avi"_ksv  , AVI        },
		{ "mpeg"_ksv , MPEG       },
		{ "mp4"_ksv  , MP4        },
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
	if (kNonEmptyFileExists(sFilename))
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
		case ZSTD.Hash():
		case SEVENZIP.Hash():
		case JPEG.Hash():
		case GIF.Hash():
		case PNG.Hash():
		case AVI.Hash():
		case MPEG.Hash():
		case MP3.Hash():
		case MP4.Hash():
		case OGV.Hash():
			return false;

		default:
			return true;
	}

	return true;

} // IsCompressible

// type "/" [tree "."]* subtype ["+" suffix]* [";" parameter]

namespace {
constexpr char TypeSeparator      = '/';
constexpr char TreeSeparator      = '.';
constexpr char SuffixSeparator    = '+';
constexpr char ParameterSeparator = ';';
constexpr KStringView Separators  = "/.+;";
}

//-----------------------------------------------------------------------------
KStringView KMIME::PastType() const
//-----------------------------------------------------------------------------
{
	auto iPos = m_mime.find(TypeSeparator);

	if (DEKAF2_UNLIKELY(iPos == KString::npos))
	{
		return KStringView{};
	}

	return m_mime.ToView(iPos+1, npos);

} // PastType

//-----------------------------------------------------------------------------
KStringView KMIME::PastTree() const
//-----------------------------------------------------------------------------
{
	auto iStart = m_mime.find(TypeSeparator);

	if (DEKAF2_UNLIKELY(iStart == KString::npos))
	{
		return KStringView{};
	}

	++iStart;

	for (;;)
	{
		auto iDot = m_mime.find(TreeSeparator, iStart);

		if (iDot == KString::npos)
		{
			return m_mime.ToView(iStart);
		}

		iStart = iDot + 1;
	}

} // PastTree

//-----------------------------------------------------------------------------
KStringView KMIME::Type() const
//-----------------------------------------------------------------------------
{
	return m_mime.ToView(0, m_mime.find(TypeSeparator));

} // Type

//-----------------------------------------------------------------------------
KStringView KMIME::Tree() const
//-----------------------------------------------------------------------------
{
	auto sRest = PastType();

	KStringView::size_type iEnd = 0;

	for (;;)
	{
		auto iDot  = sRest.find(TreeSeparator, iEnd);

		if (iDot == KStringView::npos)
		{
			break;
		}

		iEnd = iDot + 1;
	}

	if (iEnd > 0)
	{
		return sRest.Left(iEnd - 1);
	}

	return KStringView{};

} // Tree

//-----------------------------------------------------------------------------
KStringView KMIME::SubType() const
//-----------------------------------------------------------------------------
{
	auto sRest = PastTree();

	auto iPos = sRest.find_first_of(Separators);

	if (iPos == KStringView::npos)
	{
		return sRest;
	}

	return sRest.Left(iPos);

} // SubType

//-----------------------------------------------------------------------------
KStringView KMIME::Suffix() const
//-----------------------------------------------------------------------------
{
	auto iStart = m_mime.find(SuffixSeparator);

	if (iStart == KStringView::npos)
	{
		return KStringView{};
	}

	++iStart;

	auto iEnd = m_mime.find(ParameterSeparator, iStart);

	return m_mime.ToView(iStart, iEnd - iStart);

} // Suffix

//-----------------------------------------------------------------------------
KStringView KMIME::Parameter() const
//-----------------------------------------------------------------------------
{
	auto iStart = m_mime.find(ParameterSeparator);

	if (iStart == KStringView::npos)
	{
		return KStringView{};
	}

	++iStart;

	auto iSize = m_mime.size();

	while (iStart < iSize && KASCII::kIsSpace(m_mime[iStart]))
	{
		++iStart;
	}

	return m_mime.ToView(iStart);

} // Parameter

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME))
{
}

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KString sMessage, KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME)), m_Data(std::move(sMessage))
{
}

//-----------------------------------------------------------------------------
KMIMEPart::KMIMEPart(KString sControlName, KString sValue, KMIME MIME)
//-----------------------------------------------------------------------------
: m_MIME(std::move(MIME))
, m_Data(std::move(sValue))
, m_sControlName(std::move(sControlName))
{
}

//-----------------------------------------------------------------------------
const KString& KMIMEPart::GetBoundaryRandom() const
//-----------------------------------------------------------------------------
{
	if (m_sRandom.empty())
	{
		m_sRandom = KUUID().Serialize();
	}

	return m_sRandom;

} // CreateBoundaryRandom

//-----------------------------------------------------------------------------
KString KMIMEPart::GetMultiPartBoundary(std::size_t iLevel) const
//-----------------------------------------------------------------------------
{
	return kFormat("KMIME=_l{}_{}", iLevel, GetBoundaryRandom());

} // GetMultiPartBoundary

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

	if (IsMultiPart())
	{
		sContentType += kFormat("; boundary=\"{}\"", GetMultiPartBoundary(1));
	}

	return KMIME(std::move(sContentType));

} // GetContentType

//-----------------------------------------------------------------------------
bool KMIMEPart::Serialize(KStringRef& sOut, bool bForHTTP, const KReplacer& Replacer, uint16_t recursion, const KMIME& ParentMIME) const
//-----------------------------------------------------------------------------
{
	if (!IsMultiPart())
	{
		if (!m_Data.empty())
		{
			if (!(bForHTTP && recursion == 0))
			{
				sOut += "Content-Type: ";
				sOut += m_MIME.Serialize();
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
							sOut += '"';
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
						sOut += "attachment;\r\n filename=\"";
						sOut += KQuotedPrintable::Encode(m_sFileName, true);
						sOut += '"';
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
				if (IsBinary() || (Replacer.empty() && !Replacer.GetRemoveUnusedTokens()))
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
				if (Replacer.empty() && !Replacer.GetRemoveUnusedTokens())
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

		// having the '=' in the boundary guarantees for base64 and quoted printable encoding
		// that the boundary is unique
		KString sBoundary = GetMultiPartBoundary(recursion);

		if (!(bForHTTP && recursion == 1))
		{
			sOut += "Content-Type: ";
			sOut += m_MIME.Serialize();
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
bool KMIMEPart::File(KStringView sControlName, KStringViewZ sFilename, KStringView sDispname)
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
			SetMIME(KMIME::MULTIPART_RELATED);

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
			SetMIME(KMIME::MULTIPART_MIXED);

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

//-----------------------------------------------------------------------------
KMIMEReceiveMultiPartFormData::File::File(KStringView sFilename, KMIME Mime)
//-----------------------------------------------------------------------------
{
	// some browsers include(d) the pathname
	KStringView sOrigFilename = kBasename(sFilename);

	m_sFilename = kMakeSafeFilename(sOrigFilename);

	if (m_sFilename != sOrigFilename)
	{
		m_sOrigFilename = sOrigFilename;
	}

} // ctor

//-----------------------------------------------------------------------------
const KString& KMIMEReceiveMultiPartFormData::File::GetOrigFilename() const
//-----------------------------------------------------------------------------
{
	if (!m_sOrigFilename.empty())
	{
		return m_sOrigFilename;
	}
	else
	{
		return m_sFilename;
	}

} // GetOrigFilename

//-----------------------------------------------------------------------------
KMIMEReceiveMultiPartFormData::KMIMEReceiveMultiPartFormData(KStringView sOutputDirectory, KStringView sFormBoundary)
//-----------------------------------------------------------------------------
: m_sOutputDirectory(sOutputDirectory)
, m_sFormBoundary(sFormBoundary)
{
	if (!m_sFormBoundary.empty())
	{
		// we always search for the double slash prefixed boundary, including a \r\n
		// sequence in front of it..
		m_sFormBoundary.insert(0, "\r\n--");
	}

} // ctor

//-----------------------------------------------------------------------------
bool KMIMEReceiveMultiPartFormData::WriteToFile(KOutFile& OutFile, KStringView sData, File& file, KStringViewZ sOutFile)
//-----------------------------------------------------------------------------
{
	if (OutFile.Write(sData).Good())
	{
		return true;
	}

	OutFile.close();
	file.SetReceivedSize(kFileSize(sOutFile));

	return SetError(kFormat("error writing file: {}", file.GetFilename()));

} // WriteToFile

//-----------------------------------------------------------------------------
bool KMIMEReceiveMultiPartFormData::ReadFromStream(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	// "--[SomeBoundary]\r\n"
	// "Content-Disposition: form-data; name=\"upload\"; filename=\"something1.pdf\"\r\n"
	// "Content-Type: application/octet-stream\r\n"
	// "\r\n"
	// [data]
	// "\r\n--[SomeBoundary]\r\n"
	// "Content-Disposition: form-data; name=\"upload\"; filename=\"something2.pdf\"\r\n"
	// "Content-Type: application/octet-stream\r\n"
	// "\r\n"
	// [data]
	// "\r\n--[SomeBoundary]--\r\n"

	if (m_sFormBoundary.empty())
	{
		// pick the first line as form boundary
		m_sFormBoundary = InStream.ReadLine();
		// check that it starts with two dashes
		if (!m_sFormBoundary.starts_with("--"))
		{
			return SetError("form boundary does not start with two dashes");
		}
		// and prefix the boundary with \r\n - it is not part of the wrapped data
		m_sFormBoundary.insert(0, "\r\n");
	}
	else
	{
		if (!InStream.Good())
		{
			return SetError("input stream not good");
		}

		auto sFormBoundary = InStream.ReadLine();

		// skip the leading \r\n in the comparison
		if (sFormBoundary != m_sFormBoundary.ToView(2))
		{
			return SetError("form boundary does not match preset form boundary");
		}
	}

	if (m_sFormBoundary.empty())
	{
		return SetError("no form boundary set");
	}

	if (m_sFormBoundary.size() >= KDefaultCopyBufSize)
	{
		return SetError(kFormat("copy buffer size of {} too small for boundary of size {}", KDefaultCopyBufSize, m_sFormBoundary.size()));
	}

	static const KFindSetOfChars SplitAtSemicolon(";");

	KString sBuffer;

	for (; !sBuffer.empty() || InStream.Good();)
	{
		// read until empty line
		auto iHeaderEndPos = sBuffer.find("\r\n\r\n");

		if (iHeaderEndPos == KString::npos)
		{
			KString sLine;

			for (;;)
			{
				if (!InStream.Good()) return SetError("unexpected end of input");
				InStream.ReadLine(sLine);
				sBuffer += sLine;
				sBuffer += "\r\n";
				if (sLine.empty()) break;
				if (sBuffer.size() > 10000) return SetError("invalid multipart form header");
			}

			iHeaderEndPos = sBuffer.size();
		}
		else
		{
			iHeaderEndPos += 4; // include the \r\n\r\n sequence
		}

		KHTTPHeaders MultiPartHeaders;

		KInStringStream ISS(sBuffer);

		if (MultiPartHeaders.Parse(ISS))
		{
			// Parse() stops after empty line. Remove the header from the input buffer.
			sBuffer.erase(0, iHeaderEndPos);

			bool bHaveName { false };

			for (auto& sPart : MultiPartHeaders.Headers.Get(KHTTPHeader::CONTENT_DISPOSITION).Split(SplitAtSemicolon))
			{
				if (sPart.remove_prefix("filename="))
				{
					if (sPart.remove_suffix('"'))
					{
						sPart.remove_prefix('"');
					}
					else if (sPart.remove_suffix('\''))
					{
						sPart.remove_prefix('\'');
					}

					m_Files.push_back(File(sPart, MultiPartHeaders.Headers.Get(KHTTPHeader::CONTENT_TYPE)));
					bHaveName = true;
					break;
				}
			}

			if (!bHaveName)
			{
				return SetError("missing file name");
			}

			auto sOutFile = kFormat("{}{}{}", m_sOutputDirectory, kDirSep, m_Files.back().GetFilename());

			// now read from stream until boundary
			KOutFile OutFile(sOutFile);

			if (!OutFile.is_open())
			{
				return SetError(kFormat("cannot open file: ", sOutFile));
			}

			std::size_t iPos { 0 };
			// we always search for a boundary starting with "\r\n--[TheBoundary]"
			constexpr char bch = '\r';

			for (;;)
			{
				auto iWant = KDefaultCopyBufSize - sBuffer.size();
				auto iRead = InStream.Read(sBuffer, iWant);

				iPos = sBuffer.find(bch, iPos);

				if (iPos == npos)
				{
					// no start of boundary found - write whole buffer
					if (sBuffer.empty())
					{
						return SetError("unexpected end of input");
					}

					if (!WriteToFile(OutFile, sBuffer, m_Files.back(), sOutFile))
					{
						return false;
					}

					sBuffer.clear();
					iPos = 0;
				}
				else
				{
					auto sHaystack = sBuffer.ToView(iPos);
					// check how much of the boundary string we could check still inside this buffer
					if (sHaystack.size() >= m_sFormBoundary.size() + 4) // + 4 to check for --\r\n or \r\n as well
					{
						// we can check for the full boundary
						if (sHaystack.starts_with(m_sFormBoundary))
						{
							// write anything before the start of boundary to the file
							if (!WriteToFile(OutFile, sBuffer.ToView(0, iPos), m_Files.back(), sOutFile))
							{
								return false;
							}

							OutFile.close();
							m_Files.back().SetReceivedSize(kFileSize(sOutFile));
							m_Files.back().SetCompleted();

							kDebug(2, "wrote {} in file: {}",
							       kFormBytes(m_Files.back().GetReceivedSize()),
							       m_Files.back().GetFilename());

							sBuffer.erase(0, iPos + m_sFormBoundary.size());

							// check for quick abort
							if (sBuffer.starts_with("--\r\n"))
							{
								// this is the end of the upload
								return true;
							}

							// if the boundary was followed by a linebreak,
							// the next multipart will start now
							if (sBuffer.remove_prefix("\r\n"))
							{
								// get back into the outer loop
								break;
							}
							else
							{
								return SetError("garbage trailing the boundary");
							}
						}
						else
						{
							// not found, discard
							++iPos;
						}
					}
					else
					{
						// write anything before the start of the possible boundary to the file
						if (!WriteToFile(OutFile, sBuffer.ToView(0, iPos), m_Files.back(), sOutFile))
						{
							return false;
						}
						// remove the written data from the buffer
						sBuffer.erase(0, iPos);
						// and start over, in the hope to find the full boundary once the
						// buffer is refilled
						iPos = 0;
						// check for eof, in which case there will not come more input
						if (iWant > iRead)
						{
							return SetError("unexpected end of input");
						}
					}
				}
			}
		}
	}

	return SetError("unexpected end of input");

} // ReadFromStream

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
constexpr KStringViewZ KMIME::TYPESCRIPT;
constexpr KStringViewZ KMIME::VSD;
constexpr KStringViewZ KMIME::WASM;
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
constexpr KStringViewZ KMIME::H;
constexpr KStringViewZ KMIME::C;
constexpr KStringViewZ KMIME::CPP;
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

DEKAF2_NAMESPACE_END
