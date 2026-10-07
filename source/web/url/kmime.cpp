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

#include <dekaf2/web/url/kmime.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/encoding/kquotedprintable.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/kfrozen.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/io/pipes/kinpipe.h>
#include <dekaf2/io/streams/kinstringstream.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/util/id/kuuid.h>
#include <dekaf2/core/strings/kutf.h>
#include <dekaf2/core/types/kbit.h>
#include <cstring>
#include <utility>
#ifdef DEKAF2_IS_WINDOWS
	#include <cwchar>
	#include <windows.h>
#endif

DEKAF2_NAMESPACE_BEGIN

KThreadSafe<KUnorderedMap<KString, KString>> KMIME::s_ExtMap;
std::atomic<bool> KMIME::s_bHasExtensions { false };

namespace {

//-----------------------------------------------------------------------------
/// returns the byte at iPos of sData as an unsigned value, or 0 past its end
uint8_t Byte(KStringView sData, std::size_t iPos)
//-----------------------------------------------------------------------------
{
	return (iPos < sData.size()) ? static_cast<uint8_t>(sData[iPos]) : 0;

} // Byte

//-----------------------------------------------------------------------------
/// returns the little endian value at iPos of sData, or 0 past its end
template<typename T>
T LittleEndian(KStringView sData, std::size_t iPos)
//-----------------------------------------------------------------------------
{
	T iValue { 0 };

	if (iPos + sizeof(T) <= sData.size())
	{
		// the position needs no alignment
		std::memcpy(&iValue, sData.data() + iPos, sizeof(T));
		kFromLittleEndian(iValue);
	}

	return iValue;

} // LittleEndian

//-----------------------------------------------------------------------------
/// returns the type of a ZIP archive - the formats that are ZIP archives tell
/// themselves apart by their first entries
KString SniffZip(KStringView sData)
//-----------------------------------------------------------------------------
{
	// ODF and EPUB: the first entry is "mimetype", stored without compression, with
	// the MIME type as its content
	auto iNameLength  = LittleEndian<uint16_t>(sData, 26);
	auto iExtraLength = LittleEndian<uint16_t>(sData, 28);

	if (LittleEndian<uint16_t>(sData, 8) == 0 && iNameLength == 8 && sData.substr(30).starts_with("mimetype"))
	{
		std::size_t iStart = 30 + iNameLength + iExtraLength;
		std::size_t iSize  = LittleEndian<uint32_t>(sData, 18);

		if (iSize > 0 && iSize < 100 && iStart + iSize <= sData.size())
		{
			KStringView sMIME = sData.substr(iStart, iSize);

			if (sMIME.contains('/') && sMIME.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.+-/") == KStringView::npos)
			{
				return sMIME;
			}
		}
	}

	// the names of the entries in the local headers in the read part of the archive
	for (auto iPos = sData.find("PK\x03\x04"_ksv); iPos != KStringView::npos; iPos = sData.find("PK\x03\x04"_ksv, iPos + 4))
	{
		std::size_t iLength = LittleEndian<uint16_t>(sData, iPos + 26);

		if (iPos + 30 + iLength > sData.size())
		{
			break;
		}

		auto sName = sData.substr(iPos + 30, iLength);

		if (sName.starts_with("word/"))      return KMIME::DOCX;
		if (sName.starts_with("xl/"))        return KMIME::XLSX;
		if (sName.starts_with("ppt/"))       return KMIME::PPTX;
		if (sName == "META-INF/MANIFEST.MF") return KMIME::JAR;
	}

	return KMIME::ZIP;

} // SniffZip

//-----------------------------------------------------------------------------
/// returns the type of a file in the compound file format of the old Office
/// formats - by the UTF-16 names of their streams, in the directory of the file,
/// if it is in the read part
KString SniffCompoundFile(KStringView sData)
//-----------------------------------------------------------------------------
{
	if (sData.contains("W\0o\0r\0d\0D\0o\0c\0u\0m\0e\0n\0t\0"_ksv))       return KMIME::DOC;
	if (sData.contains("W\0o\0r\0k\0b\0o\0o\0k\0"_ksv))                   return KMIME::XLS;
	if (sData.contains("P\0o\0w\0e\0r\0P\0o\0i\0n\0t\0 \0D\0o\0c\0"_ksv)) return KMIME::PPT;

	return "application/x-ole-storage";

} // SniffCompoundFile

//-----------------------------------------------------------------------------
/// returns sText without the last UTF-8 character, if the reading of the start of
/// a file has cut it
KStringView WithoutCutCharacter(KStringView sText)
//-----------------------------------------------------------------------------
{
	// the lead byte of the last character is at most 4 bytes before the end
	for (std::size_t iBack = 1; iBack <= 4 && iBack <= sText.size(); ++iBack)
	{
		auto ch = Byte(sText, sText.size() - iBack);

		if ((ch & 0xC0) == 0x80)
		{
			// a continuation byte
			continue;
		}

		std::size_t iLength = ((ch & 0xE0) == 0xC0) ? 2
		                    : ((ch & 0xF0) == 0xE0) ? 3
		                    : ((ch & 0xF8) == 0xF0) ? 4
		                    :                         1;

		if (iLength > iBack)
		{
			sText.remove_suffix(iBack);
		}

		break;
	}

	return sText;

} // WithoutCutCharacter

//-----------------------------------------------------------------------------
/// returns the type of text in UTF-8, or an empty string for binary data
KString SniffText(KStringView sData)
//-----------------------------------------------------------------------------
{
	if (sData.empty())
	{
		return {};
	}

	KStringView sText = WithoutCutCharacter(sData);

	sText.remove_prefix("\xEF\xBB\xBF"_ksv);

	if (sText.contains('\0') || !kutf::Valid(sText))
	{
		return {};
	}

	for (auto ch : sText)
	{
		// control characters other than whitespace and escape (in colored logs) are binary
		if (static_cast<uint8_t>(ch) < 0x20 && KStringView("\t\n\r\f\v\x1B").find(ch) == KStringView::npos)
		{
			return {};
		}
	}

	sText.TrimLeft();

	auto sStart = kToLower(sText.substr(0, 1024));

	if (sStart.starts_with("<?xml"))
	{
		if (sStart.contains("<svg"))
		{
			return KMIME::SVG;
		}

		if (sStart.contains("<!doctype html") || sStart.contains("<html"))
		{
			return KMIME::XHTML;
		}

		return KMIME::XML;
	}

	if (sStart.starts_with("<!doctype html") || sStart.starts_with("<html"))
	{
		return KMIME::HTML_UTF8;
	}

	if (sStart.starts_with("<svg"))
	{
		return KMIME::SVG;
	}

	// JSON: an object starts with a string or ends right away, an array with a value -
	// unlike an INI section like [name]
	if (sStart.starts_with('{') || sStart.starts_with('['))
	{
		KStringView sNext = KStringView(sStart).substr(1);
		sNext.TrimLeft();

		if (sNext.empty()
		 || (sStart.front() == '{' && (sNext.front() == '"' || sNext.front() == '}'))
		 || (sStart.front() == '[' && KStringView("{[\"-0123456789]tfn").contains(sNext.front())))
		{
			return KMIME::JSON;
		}
	}

	if (sStart.starts_with("#!"))
	{
		static constexpr std::pair<KStringView, KStringViewZ> s_Interpreters[]
		{
			{ "sh"    , KMIME::SH         },
			{ "bash"  , KMIME::SH         },
			{ "zsh"   , KMIME::SH         },
			{ "ksh"   , KMIME::SH         },
			{ "mksh"  , KMIME::SH         },
			{ "dash"  , KMIME::SH         },
			{ "ash"   , KMIME::SH         },
			{ "csh"   , KMIME::CSH        },
			{ "tcsh"  , KMIME::CSH        },
			{ "python", KMIME::PYTHON     },
			{ "ruby"  , KMIME::RUBY       },
			{ "php"   , KMIME::PHP        },
			{ "node"  , KMIME::JAVASCRIPT },
			{ "nodejs", KMIME::JAVASCRIPT },
		};

		// the interpreter of a script: the first word of the line, or the one after
		// env with its options and variables, without the path and without a version
		// like the 3.12 of python3.12
		KStringView sLine = KStringView(sStart).substr(2);
		sLine = sLine.substr(0, sLine.find('\n'));

		KStringView sInterpreter;

		while (!sLine.empty())
		{
			sLine.TrimLeft();

			auto sWord = sLine.substr(0, sLine.find_first_of(" \t"));
			sLine.remove_prefix(sWord.size());

			auto sName = kBasename(sWord);

			if (sName != "env" && !sName.starts_with('-') && !sName.contains('='))
			{
				sInterpreter = sName.substr(0, sName.find_first_of("0123456789."));
				break;
			}
		}

		for (const auto& Interpreter : s_Interpreters)
		{
			if (sInterpreter == Interpreter.first)
			{
				return Interpreter.second;
			}
		}
	}

	return KMIME::TEXT_UTF8;

} // SniffText

//-----------------------------------------------------------------------------
/// returns the type of data by the signature at its start, or an empty string
KString SniffSignature(KStringView sData)
//-----------------------------------------------------------------------------
{
	if (sData.starts_with("%PDF-"))
	{
		return KMIME::PDF;
	}

	if (sData.starts_with("\x89PNG\r\n\x1A\n"_ksv))
	{
		return KMIME::PNG;
	}

	if (sData.starts_with("\xFF\xD8\xFF"_ksv))
	{
		return KMIME::JPEG;
	}

	if (sData.starts_with("GIF87a") || sData.starts_with("GIF89a"))
	{
		return KMIME::GIF;
	}

	if (sData.starts_with("II*\0"_ksv) || sData.starts_with("MM\0*"_ksv))
	{
		return KMIME::TIFF;
	}

	if (sData.starts_with("\0\0\0\x0CjP  \r\n\x87\n"_ksv))
	{
		return KMIME::JPEG2000;
	}

	// BMP: the two reserved fields of the header are 0
	if (sData.starts_with("BM") && sData.size() > 14 && LittleEndian<uint32_t>(sData, 6) == 0)
	{
		return KMIME::BMP;
	}

	// ICO: type 1, at least one image, and the reserved byte of its entry is 0
	if (sData.starts_with("\0\0\1\0"_ksv) && LittleEndian<uint16_t>(sData, 4) > 0 && sData.size() > 22 && Byte(sData, 9) == 0)
	{
		return KMIME::ICON;
	}

	if (sData.starts_with("RIFF"))
	{
		if (sData.substr(8).starts_with("WEBP"))
		{
			return KMIME::WEBP;
		}

		if (sData.substr(8).starts_with("WAVE"))
		{
			return KMIME::WAV;
		}

		if (sData.substr(8).starts_with("AVI "))
		{
			return KMIME::AVI;
		}
	}

	if (sData.starts_with("OggS"))
	{
		// the codec, in the first page
		auto sFirstPage = sData.substr(0, 128);

		if (sFirstPage.contains("\x80theora"_ksv))
		{
			return KMIME::OGV;
		}

		if (sFirstPage.contains("\x01vorbis"_ksv) || sFirstPage.contains("OpusHead") || sFirstPage.contains("\x7F\x46LAC"_ksv))
		{
			return KMIME::OGA;
		}

		return KMIME::OGX;
	}

	if (sData.starts_with("ID3"))
	{
		return KMIME::MP3;
	}

	// UTF-16 with a byte order mark - before MPEG audio, whose frames can start alike
	if (sData.starts_with("\xFF\xFE"_ksv) || sData.starts_with("\xFE\xFF"_ksv))
	{
		return KMIME::TEXT_PLAIN;
	}

	// a frame of MPEG audio: AAC in ADTS has the layer 0, MP3 one of the others
	if (Byte(sData, 0) == 0xFF && (Byte(sData, 1) & 0xE0) == 0xE0)
	{
		if ((Byte(sData, 1) & 0xF6) == 0xF0)
		{
			return KMIME::AAC;
		}

		if ((Byte(sData, 1) & 0x06) != 0)
		{
			return KMIME::MP3;
		}
	}

	if (sData.substr(4).starts_with("ftyp"))
	{
		// the ISO media file format holds also images - HEIF and AVIF are no videos
		auto sBrand = sData.substr(8, 4);

		for (KStringView sImage : { "heic", "heix", "heim", "heis", "mif1", "msf1", "avif", "avis" })
		{
			if (sBrand == sImage)
			{
				return {};
			}
		}

		return KMIME::MP4;
	}

	// Matroska, of which WebM is a profile
	if (sData.starts_with("\x1A\x45\xDF\xA3"_ksv))
	{
		return sData.substr(0, 64).contains("webm") ? KString(KMIME::WEBM) : KString();
	}

	if (sData.starts_with("MThd"))
	{
		return KMIME::MIDI;
	}

	if (sData.starts_with("PK\x03\x04"_ksv))
	{
		return SniffZip(sData);
	}

	if (sData.starts_with("PK\x05\x06"_ksv))
	{
		return KMIME::ZIP;
	}

	if (sData.starts_with("\x1F\x8B"_ksv))
	{
		return KMIME::GZIP;
	}

	if (sData.starts_with("BZh") && Byte(sData, 3) >= '1' && Byte(sData, 3) <= '9')
	{
		return KMIME::BZ2;
	}

	if (sData.starts_with("\xFD\x37zXZ\0"_ksv))
	{
		return KMIME::XZ;
	}

	if (sData.starts_with("\x28\xB5\x2F\xFD"_ksv))
	{
		return KMIME::ZSTD;
	}

	if (sData.starts_with("7z\xBC\xAF\x27\x1C"_ksv))
	{
		return KMIME::SEVENZIP;
	}

	if (sData.starts_with("Rar!\x1A\x07"_ksv))
	{
		return KMIME::RAR;
	}

	if (sData.substr(257).starts_with("ustar"))
	{
		return KMIME::TAR;
	}

	if (sData.starts_with("\0asm"_ksv))
	{
		return KMIME::WASM;
	}

	if (sData.starts_with("wOFF"))
	{
		return KMIME::WOFF;
	}

	if (sData.starts_with("wOF2"))
	{
		return KMIME::WOFF2;
	}

	if (sData.starts_with("OTTO"))
	{
		return KMIME::OTF;
	}

	if (sData.starts_with("ttcf"))
	{
		return KMIME::TTC;
	}

	// TrueType: the version 1.0, and a plausible count of tables (big endian)
	if (sData.starts_with("\0\1\0\0"_ksv) && Byte(sData, 4) == 0 && Byte(sData, 5) > 0 && Byte(sData, 5) < 64)
	{
		return KMIME::TTF;
	}

	if (sData.starts_with("FWS") || sData.starts_with("CWS") || sData.starts_with("ZWS"))
	{
		return KMIME::SWF;
	}

	if (sData.starts_with("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"_ksv))
	{
		return SniffCompoundFile(sData);
	}

	if (sData.starts_with("{\\rtf"))
	{
		return KMIME::RTF;
	}

	return SniffText(sData);

} // SniffSignature

#ifdef DEKAF2_IS_WINDOWS
//-----------------------------------------------------------------------------
/// returns the content type that installed programs registered for an extension,
/// or an empty string
KString RegisteredContentType(KStringView sExtension)
//-----------------------------------------------------------------------------
{
	if (sExtension.empty() || sExtension.find_first_of("\\/") != KStringView::npos)
	{
		return {};
	}

	auto  wsKey = L"." + kutf::Convert<std::wstring>(sExtension);
	DWORD iSize { 0 };

	if (::RegGetValueW(HKEY_CLASSES_ROOT, wsKey.c_str(), L"Content Type", RRF_RT_REG_SZ, nullptr, nullptr, &iSize) != ERROR_SUCCESS
	 || iSize < sizeof(wchar_t))
	{
		return {};
	}

	std::wstring wsType(iSize / sizeof(wchar_t), L'\0');

	if (::RegGetValueW(HKEY_CLASSES_ROOT, wsKey.c_str(), L"Content Type", RRF_RT_REG_SZ, nullptr, &wsType[0], &iSize) != ERROR_SUCCESS)
	{
		return {};
	}

	wsType.resize(std::wcslen(wsType.c_str()));

	auto sType = kutf::Convert<KString>(wsType);

	// only a type like text/plain
	if (!sType.contains('/') || sType.find_first_of("\r\n\t ") != KString::npos)
	{
		return {};
	}

	return sType;

} // RegisteredContentType
#endif

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMIME kGetMIMETypeOfData(KStringView sData)
//-----------------------------------------------------------------------------
{
	return KMIME(SniffSignature(sData));

} // kGetMIMETypeOfData

//-----------------------------------------------------------------------------
KMIME kGetMIMETypeOfFile(KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	// the signatures are at the start, and the text detection needs no more
	return kGetMIMETypeOfData(kReadAll(sFilename, 16 * 1024));

} // kGetMIMETypeOfFile

//-----------------------------------------------------------------------------
void KMIME::RememberExtension(const KString& sExtension, KStringView sMIME)
//-----------------------------------------------------------------------------
{
	auto ExtMap = s_ExtMap.unique();

	auto it = ExtMap->find(sExtension);

	if (it == ExtMap->end())
	{
		ExtMap->insert({ sExtension, KString(sMIME) });
	}
	else if (it->second.empty())
	{
		it->second = sMIME;
	}

	s_bHasExtensions = true;

} // RememberExtension

//-----------------------------------------------------------------------------
KString KMIME::GetExtension(KStringView sFilename)
//-----------------------------------------------------------------------------
{
	KString sExtension = kExtension(sFilename);

	if (sExtension.empty())
	{
		sExtension = kBasename(sFilename);

		if (sExtension.front() == '.')
		{
			sExtension.erase(0, 1);
		}
	}

	sExtension.MakeLowerASCII();

	return sExtension;

} // GetExtension

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
		{ "jlf"_ksv  , JLIFF      },
		{ "jliff"_ksv, JLIFF      },
		{ "xml"_ksv  , XML        },
		{ "swf"_ksv  , SWF        },
		{ "bz2"_ksv  , BZ2        },
		{ "tbz"_ksv  , BZ2        },
		{ "tbz2"_ksv , BZ2        },
		{ "gz"_ksv   , GZIP       },
		{ "tgz"_ksv  , GZIP       },
		{ "xz"_ksv   , XZ         },
		{ "txz"_ksv  , XZ         },
		{ "dmg"_ksv  , DMG        },
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
		{ "xlf"_ksv  , XLIFF12    },
		{ "xliff"_ksv, XLIFF12    },
		{ "txlf"_ksv , XLIFF12    },
		{ "xlf2"_ksv , XLIFF2     },
		{ "xliff2"_ksv,XLIFF2     },
		{ "xls"_ksv  , XLS        },
		{ "xlsx"_ksv , XLSX       },
		{ "zip"_ksv  , ZIP        },
		{ "zst"_ksv  , ZSTD       },
		{ "tzst"_ksv , ZSTD       },
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
		{ "mjs"_ksv  , JAVASCRIPT },
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

	auto sExtension = GetExtension(sFilename);

	{
		auto it = s_Extension_Map.find(sExtension);

		if (it != s_Extension_Map.end())
		{
			m_mime = it->second;
			return true;
		}
	}

	if (s_bHasExtensions)
	{
		auto DynMap = s_ExtMap.shared();

		auto it = DynMap->find(sExtension);

		if (it != DynMap->end())
		{
			if (it->second.empty())
			{
				// the registry of Windows knows no type for this extension
				m_mime = Default;
				return false;
			}

			m_mime = it->second;
			return true;
		}
	}

#ifdef DEKAF2_IS_WINDOWS
	// the content type that installed programs registered for the extension - also
	// none is remembered, which spares the next lookup
	auto sRegistered = RegisteredContentType(sExtension);

	RememberExtension(sExtension, sRegistered);

	if (!sRegistered.empty())
	{
		m_mime = std::move(sRegistered);
		return true;
	}
#endif

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
	if (!kNonEmptyFileExists(sFilename))
	{
		m_mime = Default;
		return false;
	}

#ifndef DEKAF2_IS_WINDOWS
	// check once if the 'file' command is available
	static KString s_sFileCommand = kWhich("file");

	// the output is one line per file - a line break in the name breaks its parsing
	if (!s_sFileCommand.empty() && sFilename.find_first_of(detail::kLineBreaksSet) == KStringView::npos)
	{
		// the argument vector goes to the file command as is - whitespace and quotes
		// in the name are no issue. The -- keeps a name starting with a dash from
		// being read as an option
		KInPipe Pipe({ s_sFileCommand, "--mime-type", "--", KString(sFilename) });

		if (Pipe.is_open())
		{
			auto sMime = Pipe.ReadLine();
			auto iClip = sMime.rfind(": ");

			if (iClip != KString::npos)
			{
				sMime.erase(0, iClip + 2);
			}

			m_mime = sMime;

			if (m_mime != NONE)
			{
				RememberExtension(GetExtension(sFilename), m_mime);
				return true;
			}
		}
	}
#endif

	// the signature at the start of the file - on Windows, and without the file command
	auto MIME = kGetMIMETypeOfFile(sFilename);

	if (MIME != NONE)
	{
		m_mime = MIME.Serialize();
		RememberExtension(GetExtension(sFilename), m_mime);
		return true;
	}

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
		case BR.Hash():
		case GZIP.Hash():
		case XZ.Hash():
		case DMG.Hash():
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
		case WEBM.Hash():
		case WEBP.Hash():
		// WOFF compresses with zlib, WOFF2 with Brotli
		case WOFF.Hash():
		case WOFF2.Hash():
			return false;

		default:
			return true;
	}

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
	m_Mime      = std::move(Mime);

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

			for (auto& sPart : MultiPartHeaders.Headers.Get(KHTTPHeader::CONTENT_DISPOSITION).Split(";"))
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
				return SetError(kFormat("cannot open file: {}", sOutFile));
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
